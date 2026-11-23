#include "rdx_engine.h"
#include "preset_bank.h"
static RdxVoiceBlockScratch render_scratch;

uint32_t RdxEngine::presetCount() {return sizeof presetData/sizeof presetData[0];}
bool RdxEngine::presetPatch(uint32_t index,RDX_Patch& p) {
    if(index>=presetCount())return false;
    memcpy(&p,presetData[index],sizeof p);
    return true;
}

RDX_Patch RdxEngine::initPatch() {
    RDX_Patch p{};
    memcpy(p.common.voiceName, "FM1 INIT  ", 10);
    p.common.transpose = 64;
    p.common.pbRange = 66;
    p.common.algorithm = 11;
    for (int i=0;i<4;++i) {
        p.common.pegLevel[i] = 64;
        auto& op = p.ops[i];
        op.enable = 1;
        op.freqCoarse = 1;
        op.freqDetune = 64;
        op.outLevel = i == 0 ? 100 : 0;
        op.egRate[0] = 110;
        op.egRate[1] = 80;
        op.egRate[2] = 60;
        op.egRate[3] = 100;
        op.egLevel[0] = 127;
        op.egLevel[1] = 110;
        op.egLevel[2] = 100;
    }
    return p;
}

bool RdxEngine::validPatch(const RDX_Patch& p) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(&p);
    for (uint32_t i=0;i<sizeof p;++i) if(bytes[i]>127) return false;
    const auto& c=p.common;
    if(c.transpose<40 || c.transpose>88 || c.monoPoly>2 || c.algorithm>11 || c.lfoWave>6 || c.pbRange<40 || c.pbRange>88) return false;
    for (const auto& op:p.ops) {
        if(op.enable>1 || op.scaleLC>3 || op.scaleRC>3 || op.lfoPMDEnable>1 ||
           op.pegEnable>1 || op.fbType>1 || op.freqMode>1 || op.freqCoarse>31 || op.freqFine>99) return false;
    }
    return true;
}

bool RdxEngine::applyPatch(const RDX_Patch& p) {
    if(!validPatch(p)) return false; // atomic from the caller's point of view
    if(phraseReady_)panicLive();else panic();
    auto& s=RDX_State::getState();
    s.controls=RDX_Controls{};
    s.workingPatch=p;
    s.storedPatch=p;
    updateDerived();
    for(int i=0;i<voiceCapacity;++i)if(sources_[i]==VOICE_LIVE){auto& v=voices_[i];v.init();v.cacheParams();v.syncLFO();}
    ready_=true;
    return true;
}
bool RdxEngine::updatePatch(const RDX_Patch& p) {
    if(!validPatch(p))return false;
    if(p.common.monoPoly!=RDX_State::getState().workingPatch.common.monoPoly){if(phraseReady_)panicLive();else panic();}
    RDX_State::getState().workingPatch=p;
    updateDerived();
    // Keep phases/gates and running envelopes. EG metadata takes effect on
    // subsequent segments/notes, rather than retriggering the voice on a turn.
    for(int i=0;i<voiceCapacity;++i)if(sources_[i]==VOICE_LIVE)voices_[i].cacheParams();
    return true;
}
static float algorithmGain(uint8_t algo);
void RdxEngine::updateDerived() {
    auto& s=RDX_State::getState();const auto& p=s.workingPatch;
    mono_=p.common.monoPoly!=RDX_MODE_POLY;
    derive(s.workingPatch,s.controls);
    const int algo=p.common.algorithm;
    // Keep the ESP32-S3 reference drive level independent of this port's voice
    // ceiling. Voice-capacity normalization otherwise changes distortion tone.
    outputGain_=0.07071067811865475f*(algo>=5 && algo<=7 ? ONE_DIV_SQRT2 : algo>=8 && algo<=10 ? ONE_DIV_SQRT3 : algo==11 ? 0.5f : 1.0f);
    if(phraseReady_)phraseRatio_=algorithmGain(phrasePatch_.common.algorithm)/algorithmGain(p.common.algorithm);
}

void RdxEngine::derive(RDX_Patch& p,RDX_Controls& controls){
    controls.portaTimeS=AM_DEPTH[p.common.portaTime]*2.5f;
    for(int i=0;i<4;++i) {
        const auto& op=p.ops[i];
        controls.freqCoef[i]=op.freqMode == RDX_FREQ_RATIO
            ? (op.freqCoarse ? op.freqCoarse+op.freqFine*0.01f : 0.5f+op.freqFine*0.005f)
            : (op.freqCoarse<8 ? 1.0f : op.freqCoarse<16 ? 10.0f : op.freqCoarse<24 ? 100.0f : 1000.0f)*fixedFine[op.freqFine];
        controls.detuneCoef[i]=detuneRatio[op.freqDetune];
    }
}
static float algorithmGain(uint8_t algo){return algo>=5&&algo<=7?ONE_DIV_SQRT2:algo>=8&&algo<=10?ONE_DIV_SQRT3:algo==11?0.5f:1.0f;}
bool RdxEngine::capturePhrase(const RDX_Patch& p){
    if(!validPatch(p))return false;
    clearPhrase();phrasePatch_=p;phraseControls_=RDX_State::getState().controls;phraseControls_.sustain=false;
    derive(phrasePatch_,phraseControls_);phraseReady_=true;
    phraseRatio_=algorithmGain(p.common.algorithm)/algorithmGain(RDX_State::getState().workingPatch.common.algorithm);
    return true;
}
void RdxEngine::releasePhrase(){
    phraseControls_.sustain=false;phraseCount_=0;
    for(int i=0;i<voiceCapacity;++i)if(sources_[i]==VOICE_PHRASE&&(voices_[i].isHeld()||voices_[i].isSustained()))voices_[i].noteOff();
}
void RdxEngine::clearPhrase(){
    for(int i=0;i<voiceCapacity;++i)if(sources_[i]==VOICE_PHRASE){
        new(&voices_[i])RDX_Voice;voices_[i].init();voices_[i].cacheParams();sources_[i]=VOICE_LIVE;retiring_[i]=0;
    }
    if(mono_&&!voices_[0].isActive())for(int i=1;i<voiceCapacity;++i)if(sources_[i]==VOICE_LIVE&&voices_[i].isActive()){
        new(&voices_[0])RDX_Voice(voices_[i]);voices_[i].init();retiring_[0]=retiring_[i];retiring_[i]=0;voiceAge_[0]=voiceAge_[i];break;
    }
    phraseCount_=0;phraseReady_=false;phraseRatio_=1.0f;
}
void RdxEngine::panicLive(){
    for(int i=0;i<voiceCapacity;++i)if(sources_[i]==VOICE_LIVE){voices_[i].init();retiring_[i]=0;voiceAge_[i]=0;}
    heldCount_=0;RDX_State::getState().controls.sustain=false;
}

void RdxEngine::panic() {
    for(auto& v:voices_) v.init();
    for(auto& age:voiceAge_)age=0;
    for(auto& fade:retiring_)fade=0;
    heldCount_=phraseCount_=0;phraseControls_.sustain=false;
    RDX_State::getState().controls.sustain=false;
}
uint8_t RdxEngine::voiceLimit() const {return phraseReady_||RDX_State::getState().workingPatch.common.monoPoly==RDX_MODE_POLY?limit_:1;}
uint8_t RdxEngine::activeVoices() const {uint8_t count=0;for(const auto& v:voices_)if(v.isActive())++count;return count;}
int RdxEngine::cheapestVoice(bool skipRetiring){
    int slot=-1;float lowest=1.0e30f;
    for(int i=0;i<voiceCapacity;++i){
        if(!voices_[i].isActive()||(skipRetiring&&retiring_[i]))continue;
        const float score=voices_[i].calcScore();
        if(slot<0||score<lowest||(score==lowest&&voiceAge_[i]>voiceAge_[slot])){slot=i;lowest=score;}
    }return slot;
}
void RdxEngine::setVoiceLimit(uint8_t limit){
    limit_=limit<1?1:limit>voiceCapacity?voiceCapacity:limit;
    if(!phraseReady_&&RDX_State::getState().workingPatch.common.monoPoly!=RDX_MODE_POLY)return;
    int count=0;for(int i=0;i<voiceCapacity;++i)if(voices_[i].isActive()&&!retiring_[i])++count;
    while(count>limit_){const int slot=cheapestVoice(true);if(slot<0)break;retiring_[slot]=32;--count;}
}

void RdxEngine::noteOn(uint8_t n,uint8_t v){noteOnSource(VOICE_LIVE,n,v);}
void RdxEngine::noteOff(uint8_t n){noteOffSource(VOICE_LIVE,n);}
void RdxEngine::sustain(bool down){sustainSource(VOICE_LIVE,down);}
void RdxEngine::noteOnSource(eVoiceSource source,uint8_t note,uint8_t velocity){
    if(!ready_||note>127||velocity>127||(source==VOICE_PHRASE&&!phraseReady_))return;
    if(!velocity){noteOffSource(source,note);return;}
    const bool mono=patchFor(source).common.monoPoly!=RDX_MODE_POLY;
    auto* held=source==VOICE_PHRASE?phraseHeld_:heldNotes_;
    auto* vel=source==VOICE_PHRASE?phraseVelocity_:heldVelocity_;
    auto& count=source==VOICE_PHRASE?phraseCount_:heldCount_;
    int slot=-1;
    if(mono){
        for(int i=0;i<voiceCapacity;++i)if(sources_[i]==source&&voices_[i].isActive()){slot=i;break;}
        if(count==16){for(int i=1;i<16;++i){held[i-1]=held[i];vel[i-1]=vel[i];}--count;}
        held[count]=note;vel[count++]=velocity;
    }
    if(slot<0){
        if(!phraseReady_&&mono)slot=0;
        else if(activeVoices()<limit_)for(int i=0;i<voiceCapacity;++i)if(!voices_[i].isActive()){slot=i;break;}
        if(slot<0)slot=cheapestVoice(true);if(slot<0)slot=cheapestVoice(false);if(slot<0)slot=0;
        retiring_[slot]=0;
        const uint8_t age=voiceAge_[slot];
        for(int i=0;i<voiceCapacity;++i)if(i!=slot&&voiceAge_[i]<=age&&voiceAge_[i]<voiceCapacity)++voiceAge_[i];
        voiceAge_[slot]=0;
    }
    if(sources_[slot]!=source){
        new(&voices_[slot])RDX_Voice(patchFor(source),controlsFor(source));
        sources_[slot]=source;voices_[slot].init();voices_[slot].cacheParams();
    }
    voices_[slot].noteOn(note,velocity);
}
void RdxEngine::noteOffSource(eVoiceSource source,uint8_t note){
    if(!ready_||note>127)return;
    if(patchFor(source).common.monoPoly!=RDX_MODE_POLY){
        auto* held=source==VOICE_PHRASE?phraseHeld_:heldNotes_;auto* vel=source==VOICE_PHRASE?phraseVelocity_:heldVelocity_;
        auto& count=source==VOICE_PHRASE?phraseCount_:heldCount_;int found=-1;
        for(int i=0;i<count;++i)if(held[i]==note){found=i;break;}if(found<0)return;
        bool top=found==count-1;
        for(int i=found+1;i<count;++i){held[i-1]=held[i];vel[i-1]=vel[i];}--count;
        for(int i=0;i<voiceCapacity;++i)if(sources_[i]==source&&voices_[i].isHeld()){
            if(top&&count)voices_[i].noteOn(held[count-1],vel[count-1]);else if(!count)voices_[i].noteOff();
        }return;
    }
    for(int i=0;i<voiceCapacity;++i)if(sources_[i]==source&&voices_[i].note()==note&&voices_[i].isHeld())voices_[i].noteOff();
}
void RdxEngine::sustainSource(eVoiceSource source,bool down){
    controlsFor(source).sustain=down;
    if(!down)for(int i=0;i<voiceCapacity;++i)if(sources_[i]==source&&voices_[i].isSustained())voices_[i].noteOff();
}

void RdxEngine::allNotesOff() {
    heldCount_=0;
    for(auto& v:voices_)if(v.isHeld())v.noteOff();
}

void RdxEngine::renderBlock(float* left,float* right) {
    if(!ready_) {
        for(uint32_t i=0;i<DMA_BUFFER_LEN;++i) left[i]=right[i]=0.0f;
        return;
    }
    uint8_t slots[voiceCapacity];int count=0;
    for(int v=0;v<slotCount();++v)if(voices_[v].isActive()){
        slots[count++]=(uint8_t)v;voices_[v].updateLfo();
    }
    rendered_=(uint8_t)count;
    for(uint32_t i=0;i<DMA_BUFFER_LEN;++i)left[i]=0.0f;
    for(uint32_t offset=0;offset<DMA_BUFFER_LEN;offset+=RDX_RENDER_CHUNK){
        for(int at=0;at<count;++at){
            const int v=slots[at];
            // A budget reduction fades the selected voice over 32 samples.
            // The slot stays in this bounded block list, silent after retirement.
            if(v==255)continue;
            uint32_t n=RDX_RENDER_CHUNK;
            if(n>DMA_BUFFER_LEN-offset)n=DMA_BUFFER_LEN-offset;
            if(retiring_[v]&&n>retiring_[v])n=retiring_[v];
            voices_[v].renderChunk(render_scratch,n,slotMono(v));
            if(sources_[v]==VOICE_PHRASE)for(uint32_t i=0;i<n;++i)render_scratch.wave[0][i]*=phraseRatio_;
            for(uint32_t i=0;i<n;++i){
                float value=render_scratch.wave[0][i];
                if(retiring_[v]){value*=retiring_[v]*(1.0f/32.0f);if(!--retiring_[v]){voices_[v].init();slots[at]=255;}}
                left[offset+i]+=value;
            }
        }
    }
    for(uint32_t i=0;i<DMA_BUFFER_LEN;++i){
        float sample=left[i];
        sample*=outputGain_;
        // Source's linear knee and smooth saturation, bounded to [-1,1].
        const float a=fabsf(sample);
        if(a>0.9f) {
            const float over=a-0.9f;
            sample=copysignf(0.9f+0.1f*over/(0.1f+over),sample);
        }
        left[i]=right[i]=sample;
    }
}

void RdxEngine::renderDry(float (*dry)[DMA_BUFFER_LEN]) {
    uint8_t slots[voiceCapacity];int count=0;
    for(int v=0;v<voiceCapacity;++v){
        for(uint32_t i=0;i<DMA_BUFFER_LEN;++i)dry[v][i]=0.0f;
        if(ready_&&v<slotCount()&&voices_[v].isActive()){
            slots[count++]=(uint8_t)v;voices_[v].updateLfo();
        }
    }
    rendered_=(uint8_t)count;
    for(uint32_t offset=0;offset<DMA_BUFFER_LEN;offset+=RDX_RENDER_CHUNK){
        for(int at=0;at<count;++at){
            const int v=slots[at];if(v==255)continue;
            uint32_t n=RDX_RENDER_CHUNK;
            if(n>DMA_BUFFER_LEN-offset)n=DMA_BUFFER_LEN-offset;
            if(retiring_[v]&&n>retiring_[v])n=retiring_[v];
            voices_[v].renderChunk(render_scratch,n,slotMono(v));
            if(sources_[v]==VOICE_PHRASE)for(uint32_t i=0;i<n;++i)render_scratch.wave[0][i]*=phraseRatio_;
            for(uint32_t i=0;i<n;++i){
                float value=render_scratch.wave[0][i];
                if(retiring_[v]){value*=retiring_[v]*(1.0f/32.0f);if(!--retiring_[v]){voices_[v].init();slots[at]=255;}}
                dry[v][offset+i]=value;
            }
        }
    }
}
void RdxEngine::mixDry(const float (*dry)[DMA_BUFFER_LEN],float gain,float* left,float* right){
    for(uint32_t i=0;i<DMA_BUFFER_LEN;++i){
        float sample=0.0f;
        for(int v=0;v<voiceCapacity;++v)sample+=dry[v][i];
        sample*=gain;
        const float a=fabsf(sample);
        if(a>0.9f){const float over=a-0.9f;sample=copysignf(0.9f+0.1f*over/(0.1f+over),sample);}
        left[i]=right[i]=sample;
    }
}
