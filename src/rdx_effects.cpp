#include "rdx_effects.h"

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
    arena_=arena;
    for(uint8_t i=0;i<2;++i) {
        auto& s=slots_[i];
        s.distortion.init(SAMPLE_RATE,i);s.wah.init(SAMPLE_RATE,i);
        s.chorus.init(SAMPLE_RATE,i);s.flanger.init(SAMPLE_RATE,i);
        s.phaser.init(SAMPLE_RATE,i);s.delay.init(SAMPLE_RATE,i);s.reverb.init(SAMPLE_RATE,i);
    }
}
bool RdxEffects::begin(const RDX_Patch& p) {
    prepared_=false;valid_=false;used_=cleared_=0;
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
void RdxEffects::process(float* l,float* r) {
    if(!prepared_||!valid_)return;
    for(auto& s:slots_)switch(s.id) {
    case EFFECT_DISTORTION:s.distortion.processBlock(l,r,DMA_BUFFER_LEN);break;
    case EFFECT_TOUCH_WAH:s.wah.processBlock(l,r,DMA_BUFFER_LEN);break;
    case EFFECT_CHORUS:s.chorus.processBlock(l,r,DMA_BUFFER_LEN);break;
    case EFFECT_FLANGER:s.flanger.processBlock(l,r,DMA_BUFFER_LEN);break;
    case EFFECT_PHASER:s.phaser.processBlock(l,r,DMA_BUFFER_LEN);break;
    case EFFECT_DELAY:s.delay.processBlock(l,r,DMA_BUFFER_LEN);break;
    case EFFECT_REVERB:s.reverb.processBlock(l,r,DMA_BUFFER_LEN);break;
    default:break;
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
