#include "rdx_effects.h"
#include "fx/level_calibration.h"
#include "rdx_level_math.h"
#if defined(RDX_CALIBRATION_CAPTURE) && !defined(__EMSCRIPTEN__)
#error "FX calibration capture is only for the offline WebAssembly build"
#endif

uint32_t RdxEffects::slotSize(uint8_t id,uint8_t time) {
    if(id>=EFFECTS_COUNT || time>127)return arenaFloats+1u;
    switch(id) {
    case EFFECT_CHORUS:case EFFECT_PHASER:return 8192;
    case EFFECT_FLANGER:return 2u*((uint32_t)(0.015f*SAMPLE_RATE)+4u);
    case EFFECT_DELAY: {
        // The source's cached value 64 retains its reset tap of 1/4 second.
        uint32_t tap=time==64 ? SAMPLE_RATE/4 : (uint32_t)(DELAY_TIME_MS[time]*0.001f*SAMPLE_RATE);
        if(tap<1)tap=1;
        if(tap>=SAMPLE_RATE)tap=SAMPLE_RATE-1;
        return 2u*(tap+1u);
    }
    case EFFECT_REVERB: {
        uint32_t size=0;
        for(int ch=0;ch<2;++ch) {
            for(int i=0;i<NUM_COMBS;++i)size+=(uint32_t)((comb_lengths_ms[i]/1000.0f)*SAMPLE_RATE)+ch*17;
            for(int i=0;i<NUM_ALLPASSES;++i)size+=(uint32_t)((allpass_lengths_ms[i]/1000.0f)*SAMPLE_RATE)+i+ch;
        }
        return size;
    }
    default:return 0;
    }
}
uint32_t RdxEffects::required(const RDX_Patch& p) {
    return slotSize(p.common.effects[0][0],p.common.effects[0][2])+slotSize(p.common.effects[1][0],p.common.effects[1][2]);
}
void RdxEffects::init(float* arena) {
    inputDc_={};arena_=arena;
    for(uint8_t i=0;i<2;++i) {
        auto& s=slots_[i];
        s.distortion.init(SAMPLE_RATE,i);s.wah.init(SAMPLE_RATE,i);
        s.chorus.init(SAMPLE_RATE,i);s.flanger.init(SAMPLE_RATE,i);
        s.phaser.init(SAMPLE_RATE,i);s.delay.init(SAMPLE_RATE,i);s.reverb.init(SAMPLE_RATE,i);
    }
}
bool RdxEffects::begin(const RDX_Patch& p) {
    inputDc_={};prepared_=false;valid_=false;used_=cleared_=0;
    for(uint8_t i=0;i<2;++i) {
        auto& s=slots_[i];s.~Slot();new(&s) Slot;
        s.distortion.init(SAMPLE_RATE,i);s.wah.init(SAMPLE_RATE,i);
        s.chorus.init(SAMPLE_RATE,i);s.flanger.init(SAMPLE_RATE,i);
        s.phaser.init(SAMPLE_RATE,i);s.delay.init(SAMPLE_RATE,i);s.reverb.init(SAMPLE_RATE,i);
        s.id=p.common.effects[i][0];
        if(s.id>=EFFECTS_COUNT || p.common.effects[i][2]>127)return false;
        s.offset=used_;s.size=slotSize(s.id,p.common.effects[i][2]);used_+=s.size;
    }
    valid_=arena_ && used_<=arenaFloats;
    return valid_;
}
bool RdxEffects::serviceClear() {
    if(!valid_)return false;
    if(prepared_)return true;
    const uint32_t end=rdxMin(used_,cleared_+4096u);
    for(;cleared_<end;++cleared_)arena_[cleared_]=0.0f;
    if(cleared_==used_)prepared_=prepare();
    return prepared_;
}
bool RdxEffects::prepare() {
    for(auto& s:slots_) {
        float* b=arena_+s.offset;bool ok=true;
        switch(s.id) {
        case EFFECT_DISTORTION:ok=s.distortion.prepare(b,s.size,nullptr,0,SAMPLE_RATE);s.distortion.reset();s.distortion.enable(true);break;
        case EFFECT_TOUCH_WAH:ok=s.wah.prepare(b,s.size,nullptr,0,SAMPLE_RATE);s.wah.reset();s.wah.enable(true);break;
        case EFFECT_CHORUS:ok=s.chorus.prepare(b,s.size,nullptr,0,SAMPLE_RATE);s.chorus.reset();s.chorus.enable(true);break;
        case EFFECT_FLANGER:ok=s.flanger.prepare(b,s.size,nullptr,0,SAMPLE_RATE);s.flanger.reset();s.flanger.enable(true);break;
        case EFFECT_PHASER:ok=s.phaser.prepare(b,s.size,nullptr,0,SAMPLE_RATE);s.phaser.reset();s.phaser.enable(true);break;
        case EFFECT_DELAY:s.delay.setCapacity(s.size/2u);ok=s.delay.prepare(nullptr,0,b,s.size,SAMPLE_RATE);s.delay.reset();s.delay.enable(true);break;
        case EFFECT_REVERB:ok=s.reverb.prepare(b,s.size,nullptr,0,SAMPLE_RATE);s.reverb.reset();s.reverb.enable(true);break;
        default:break;
        }
        if(!ok){valid_=false;return false;}
    }
    return true;
}
void RdxEffects::process(float* l,float* r,bool matchLevels) {
    if(!prepared_||!valid_)return;
    for(uint32_t slot=0;slot<2u;++slot){
        auto& s=slots_[slot];float power=0.0f;
        if(matchLevels&&s.id!=EFFECT_THRU){
            const auto& p=RDX_State::getState().workingPatch.common.effects[slot];
            if(s.levelP1!=p[1]||s.levelP2!=p[2]){
                s.levelP1=p[1];s.levelP2=p[2];
                const uint32_t a=p[1]<96u?p[1]/32u:3u,b=p[2]<96u?p[2]/32u:3u;
                const float x=(float)(p[1]-a*32u)*(a==3u?1.0f/31.0f:1.0f/32.0f),y=(float)(p[2]-b*32u)*(b==3u?1.0f/31.0f:1.0f/32.0f);
                for(uint32_t k=0;k<4u;++k){const auto& t=effectLevelGain[s.id][k];const float u=t[a][b]+(t[a][b+1]-t[a][b])*y,v=t[a+1][b]+(t[a+1][b+1]-t[a+1][b])*y;s.levelAt[k]=u+(v-u)*x;}
            }
            for(uint32_t i=0;i<DMA_BUFFER_LEN;++i)power+=l[i]*l[i]+r[i]*r[i];
            power*=1.0f/(2.0f*DMA_BUFFER_LEN);s.levelPower+=(power-s.levelPower)*0.05f;
        }
        switch(s.id) {
    case EFFECT_DISTORTION:s.distortion.processBlock(l,r,DMA_BUFFER_LEN);break;
    case EFFECT_TOUCH_WAH:s.wah.processBlock(l,r,DMA_BUFFER_LEN);break;
    case EFFECT_CHORUS:s.chorus.processBlock(l,r,DMA_BUFFER_LEN);break;
    case EFFECT_FLANGER:s.flanger.processBlock(l,r,DMA_BUFFER_LEN);break;
    case EFFECT_PHASER:s.phaser.processBlock(l,r,DMA_BUFFER_LEN);break;
    case EFFECT_DELAY:s.delay.processBlock(l,r,DMA_BUFFER_LEN);break;
    case EFFECT_REVERB:s.reverb.processBlock(l,r,DMA_BUFFER_LEN);break;
    default:break;
        }
        if(matchLevels&&s.id!=EFFECT_THRU){
            rdx_dc_process(&s.dc,l,r,DMA_BUFFER_LEN);
            uint32_t k=0;while(k<2u&&s.levelPower>effectLevelPower[k+1u])++k;
            float x=(s.levelPower-effectLevelPower[k])*effectLevelInvPowerSpan[k];if(x<0.0f)x=0.0f;if(x>1.0f)x=1.0f;
            float target=s.levelAt[k]+(s.levelAt[k+1u]-s.levelAt[k])*x;
            if(s.id==EFFECT_TOUCH_WAH){
                float wetPower=0.0f;for(uint32_t i=0;i<DMA_BUFFER_LEN;++i)wetPower+=l[i]*l[i]+r[i]*r[i];
                wetPower*=1.0f/(2.0f*DMA_BUFFER_LEN);s.levelWetPower+=(wetPower-s.levelWetPower)*0.05f;
                // Hold makeup during silence/tails instead of pulling them up/down.
                if(power>1.0e-8f&&s.levelPower>1.0e-8f){float ratio=s.levelWetPower/s.levelPower;if(ratio<0.0625f)ratio=0.0625f;if(ratio>16.0f)ratio=16.0f;target=rdx_level_inverse_sqrt(ratio);}
                else target=s.levelGain;
            }
// The offline capture build keeps DC removal but measures uncorrected FX.
#ifndef RDX_CALIBRATION_CAPTURE
            for(uint32_t i=0;i<DMA_BUFFER_LEN;++i){s.levelGain+=(target-s.levelGain)*0.002f;l[i]*=s.levelGain;r[i]*=s.levelGain;}
#endif
        }
    }
}
float RdxEffects::phaserValue(uint8_t p) const {
    for(const auto& s:slots_)if(s.id==EFFECT_PHASER)return s.phaser.getTuneParam(p);
    return slots_[0].phaser.getTuneParam(p);
}
void RdxEffects::adjustPhaser(uint8_t p,int d) {
    for(auto& s:slots_)if(s.id==EFFECT_PHASER){s.phaser.adjustTuneParam(p,d);return;}
    slots_[0].phaser.adjustTuneParam(p,d);
}
