#include "rdx_profile.h"
// Static rather than audio-stack storage. Reused sequentially for each voice.
static float profile_mod[DMA_BUFFER_LEN][8],profile_env[DMA_BUFFER_LEN][4],profile_wave[DMA_BUFFER_LEN];
static RdxEnvelopePoint profile_points[DMA_BUFFER_LEN][4];
static float profile_phase[DMA_BUFFER_LEN][4];
uint32_t RdxProfile::prepare(RdxEngine* engine,uint8_t* slots,float* left){
    for(uint32_t i=0;i<DMA_BUFFER_LEN;++i)left[i]=0.0f;
    uint32_t count=0;
    if(engine->ready_)for(int v=0;v<engine->slotCount();++v)
      if(engine->voices_[v].isActive()){slots[count++]=(uint8_t)v;engine->voices_[v].updateLfo();}
    engine->rendered_=(uint8_t)count;return count;
}
void RdxProfile::mods(RDX_Voice* voice,uint32_t n,bool mono){
    if(mono)for(uint32_t i=0;i<n;++i)voice->profileMods<true>(profile_mod[i]);
    else for(uint32_t i=0;i<n;++i)voice->profileMods<false>(profile_mod[i]);
}
void RdxProfile::envelopes(RDX_Voice* voice,uint32_t n){
    for(uint32_t i=0;i<n;++i)voice->profileEnvelopes(profile_env[i]);
}
void RdxProfile::waves(RDX_Voice* voice,uint32_t n){
    for(uint32_t i=0;i<n;++i)profile_wave[i]=voice->profileWave(profile_mod[i],profile_env[i]);
}
void RdxProfile::envelopeSteps(RDX_Voice* voice,uint32_t n){
    for(uint32_t i=0;i<n;++i)voice->profileEnvelopeAdvance(profile_points[i]);
}
void RdxProfile::envelopeGains(uint32_t n){
    for(uint32_t i=0;i<n;++i)for(int op=3;op>=0;--op){
        const auto& p=profile_points[i][op];
        profile_env[i][op]=p.rising?p.k*p.level+p.c:rdxGain(p.level);
    }
}
void RdxProfile::pitches(RDX_Voice* voice,uint32_t n){
    for(uint32_t i=0;i<n;++i)voice->profilePitch(profile_mod[i],profile_phase[i]);
}
void RdxProfile::preparedWaves(RDX_Voice* voice,uint32_t n){
    for(uint32_t i=0;i<n;++i)profile_wave[i]=voice->profileWavePrepared(profile_mod[i],profile_env[i],profile_phase[i]);
}
void RdxProfile::mix(RdxEngine* engine,uint32_t v,uint32_t n,float* left){
    for(uint32_t i=0;i<n;++i){
        float value=profile_wave[i];
        if(engine->sources_[v]==RdxEngine::VOICE_PHRASE)value*=engine->phraseRatio_;
        if(engine->retiring_[v]){
            value*=engine->retiring_[v]*(1.0f/32.0f);
            if(!--engine->retiring_[v])engine->voices_[v].init();
        }
        left[i]+=value;
    }
}
void RdxProfile::finish(RdxEngine* engine,float* left,float* right){
    for(uint32_t i=0;i<DMA_BUFFER_LEN;++i){
        float sample=left[i]*engine->outputGain_;
        const float a=fabsf(sample);
        if(a>0.9f){const float over=a-0.9f;sample=copysignf(0.9f+0.1f*over/(0.1f+over),sample);}
        left[i]=right[i]=sample;
    }
}
void RdxProfile::render(RdxEngine& engine,float* left,float* right,uint32_t* ticks,uint32_t(*clock)(),uint32_t* fine){
    for(uint32_t i=0;i<RENDER_PARTS_COUNT;++i)ticks[i]=0;
    if(fine)for(uint32_t i=0;i<RENDER_FINE_PARTS_COUNT;++i)fine[i]=0;
    uint8_t slots[RdxEngine::voiceCapacity];
    uint32_t t=clock();const uint32_t count=RDX_FAR(prepare)(&engine,slots,left);
    ticks[RENDER_PREP]=clock()-t;
    for(uint32_t at=0;at<count;++at){
        const uint32_t v=slots[at],n=engine.retiring_[v]?engine.retiring_[v]:DMA_BUFFER_LEN;
        RDX_Voice* voice=&engine.voices_[v];
        t=clock();RDX_FAR(mods)(voice,n,engine.slotMono(v));ticks[RENDER_MOD]+=clock()-t;
        if(fine){
            t=clock();RDX_FAR(envelopeSteps)(voice,n);fine[RENDER_ENV_STEP]+=clock()-t;
            t=clock();RDX_FAR(envelopeGains)(n);fine[RENDER_ENV_GAIN]+=clock()-t;
            t=clock();RDX_FAR(pitches)(voice,n);fine[RENDER_OP_PITCH]+=clock()-t;
            t=clock();RDX_FAR(preparedWaves)(voice,n);fine[RENDER_OP_WAVE]+=clock()-t;
        }else{
            t=clock();RDX_FAR(envelopes)(voice,n);ticks[RENDER_ENV]+=clock()-t;
            t=clock();RDX_FAR(waves)(voice,n);ticks[RENDER_OP]+=clock()-t;
        }
        t=clock();RDX_FAR(mix)(&engine,v,n,left);ticks[RENDER_MIX]+=clock()-t;
    }
    if(fine){ticks[RENDER_ENV]=fine[RENDER_ENV_STEP]+fine[RENDER_ENV_GAIN];
        ticks[RENDER_OP]=fine[RENDER_OP_PITCH]+fine[RENDER_OP_WAVE];}
    t=clock();RDX_FAR(finish)(&engine,left,right);ticks[RENDER_MIX]+=clock()-t;
}
