#include "engine_api.h"
#include "editor_api.h"
#include "../src/rdx_engine.cpp"
#include "../src/rdx_effects.cpp"

// Explicit construction at boot, with static storage and no heap or C++ runtime.
alignas(RdxEngine) static unsigned char storage[sizeof(RdxEngine)];
static RdxEngine* engine;
alignas(RdxEffects) static unsigned char effects_storage[sizeof(RdxEffects)];
static RdxEffects* effects;
#if defined(RDX_FM1) && !defined(PLAT_HOST)
static float effect_arena[RdxEffects::arenaFloats] __attribute__((section(".pool")));
#else
static float effect_arena[RdxEffects::arenaFloats];
#endif
static bool effects_on=true;
enum ePresetFade {PRESET_STEADY,PRESET_FADE_OUT,PRESET_FADE_IN,PRESET_FADES_COUNT};
static ePresetFade fade;
static uint32_t destination=1;
// MoDemLead's source distortion drive is intentionally hot. Trim only the
// final output (-12.04 dB), retaining its operator/FX drive and patch bytes.
static float preset_gain=1.0f;
static bool external_patch;
static uint8_t system_metadata[32];
static float output_trim(){return !external_patch&&destination==10u?0.25f:1.0f;}
static RDX_Patch pending_patch,edited_patch;
static uint32_t editor_revision;
static bool editor_pending,editor_dirty,editor_error;
static uint8_t midi_rx=1,midi_control=0,modulation=0,midi_volume=127,expression=127;
static int bend=0;
static float performance_gain=1.0f;
static void stage_edit(const RDX_Patch& p) {
    if(!RdxEngine::validPatch(p)||RdxEffects::required(p)>RdxEffects::arenaFloats){editor_error=true;++editor_revision;return;}
    edited_patch=p;editor_pending=editor_dirty=true;editor_error=false;++editor_revision;
}
enum eDeferredType {DEFER_NOTE_ON,DEFER_NOTE_OFF,DEFER_SUSTAIN,DEFER_TYPES_COUNT};
struct DeferredEvent {eDeferredType type;uint8_t note,value;};
static DeferredEvent deferred[32];
static uint32_t deferred_count;
static bool pedal_down,pedal_before;
static void dispatch(DeferredEvent e) {
    if(e.type==DEFER_NOTE_ON)engine->noteOn(e.note,e.value);
    else if(e.type==DEFER_NOTE_OFF)engine->noteOff(e.note);
    else if(e.type==DEFER_SUSTAIN)engine->sustain(e.value!=0);
}
static void event(DeferredEvent e) {
    if(fade!=PRESET_FADE_OUT) {dispatch(e);return;}
    if(deferred_count<32)deferred[deferred_count++]=e;
    else rdx_panic(); // explicit overload fallback: silence and discard
}
extern "C" void rdx_boot(void) {
    destination=1;fade=PRESET_STEADY;deferred_count=0;pedal_down=pedal_before=false;
    preset_gain=1.0f;
    external_patch=false;memset(system_metadata,0,32);system_metadata[6]=1;system_metadata[7]=64;system_metadata[9]=120;system_metadata[10]=20;system_metadata[11]=system_metadata[12]=system_metadata[13]=1;
    midi_rx=1;midi_control=modulation=0;midi_volume=expression=127;bend=0;performance_gain=1.0f;
    editor_revision=1;editor_pending=editor_dirty=editor_error=false;
    engine=new(storage) RdxEngine;
    effects=new(effects_storage) RdxEffects;
    effects->init(effect_arena);effects_on=true;
    RDX_Patch p{};
    RdxEngine::presetPatch(destination,p);
    engine->applyPatch(p);
    pending_patch=p;
    effects->begin(p);
}
extern "C" void rdx_note_on(uint8_t n,uint8_t v) { event({DEFER_NOTE_ON,n,v}); }
extern "C" void rdx_note_off(uint8_t n) { event({DEFER_NOTE_OFF,n,0}); }
extern "C" void rdx_sustain(uint8_t v) {pedal_down=v!=0;event({DEFER_SUSTAIN,0,v});}
extern "C" void rdx_all_notes_off(void){deferred_count=0;engine->allNotesOff();}
extern "C" uint32_t rdx_midi_receive_channel(void){return midi_rx;}
extern "C" void rdx_pitch_bend(uint16_t value){if(value<=16383u)bend=(int)value-8192;}
extern "C" void rdx_control_change(uint8_t cc,uint8_t value){
    if(value>127)return;
    if(cc==1){modulation=value;return;}
    if(cc==7){midi_volume=value;return;}
    if(cc==11){expression=value;return;}
    if(cc==64){rdx_sustain(value>=64);return;}
    if(cc==120||cc==126||cc==127){rdx_panic();return;}
    if(cc==121){bend=0;modulation=0;expression=127;rdx_sustain(0);return;}
    if(cc==123||cc==124||cc==125){rdx_all_notes_off();if(cc==124||cc==125){midi_rx=1;++editor_revision;}return;}
    if(!midi_control)return;
    RDX_Patch p=editor_pending?edited_patch:fade==PRESET_FADE_OUT?pending_patch:RDX_State::getState().workingPatch;
    if(cc==80){
        static const uint8_t upper[12]={11,21,32,42,53,63,74,85,95,106,116,127};
        uint8_t a=0;while(a<11&&value>upper[a])++a;p.common.algorithm=a;
    }else{
        const uint8_t bases[4]={85,102,108,114};int op=-1,param=0;
        for(int i=0;i<4;++i)if(cc>=bases[i]&&cc<bases[i]+6){op=i;param=cc-bases[i];break;}
        if(op<0)return;auto& q=p.ops[op];
        switch(param){case 0:q.outLevel=value;break;case 1:q.feedback=value;break;
          case 2:q.fbType=value>=64;break;case 3:q.freqMode=value>=64;break;
          case 4:q.freqCoarse=value>31?31:value;break;case 5:q.freqFine=value>99?99:value;break;}
    }
    stage_edit(p);
}
extern "C" void rdx_panic(void) {
    if(fade==PRESET_FADE_OUT) {
        engine->applyPatch(pending_patch);
        preset_gain=output_trim();
    }
    fade=PRESET_STEADY;deferred_count=0;pedal_down=false;
    engine->panic();
    effects->begin(RDX_State::getState().workingPatch);
}
extern "C" void rdx_fx_toggle(void) {effects_on=!effects_on;}
extern "C" uint32_t rdx_fx_status(void) {return !effects->valid() ? 2u : effects_on ? 1u : 0u;}
extern "C" uint32_t rdx_preset_count(void) {return RdxEngine::presetCount();}
extern "C" void rdx_preset_request(uint32_t i) {
    if(i>=RdxEngine::presetCount())return;
    RdxEngine::presetPatch(i,pending_patch);editor_pending=false;editor_dirty=editor_error=false;++editor_revision;
    external_patch=false;destination=i;fade=PRESET_FADE_OUT;deferred_count=0;pedal_before=pedal_down;
}
extern "C" int rdx_patch_valid(const uint8_t* bytes){RDX_Patch p;memcpy(&p,bytes,150);return RdxEngine::validPatch(p)&&RdxEffects::required(p)<=RdxEffects::arenaFloats;}
extern "C" void rdx_patch_export(uint8_t* bytes){memcpy(bytes,editor_pending?&edited_patch:fade==PRESET_FADE_OUT?&pending_patch:&RDX_State::getState().workingPatch,150);}
extern "C" void rdx_patch_import(const uint8_t* bytes){
    memcpy(&pending_patch,bytes,150);editor_pending=false;external_patch=true;editor_dirty=true;editor_error=false;++editor_revision;
    fade=PRESET_FADE_OUT;deferred_count=0;pedal_before=pedal_down;
}
extern "C" void rdx_patch_edit(const uint8_t* bytes){RDX_Patch p;memcpy(&p,bytes,150);stage_edit(p);}
extern "C" void rdx_system_export(uint8_t* bytes){memcpy(bytes,system_metadata,32);bytes[1]=midi_rx?midi_rx-1:16;bytes[14]=midi_control;}
extern "C" void rdx_system_import(const uint8_t* bytes){memcpy(system_metadata,bytes,32);midi_rx=bytes[1]==16?0:bytes[1]+1;midi_control=bytes[14]!=0;++editor_revision;}
extern "C" void rdx_preset_name(uint32_t i,char* name) {
    RDX_Patch p{};
    if(RdxEngine::presetPatch(i,p))memcpy(name,p.common.voiceName,10);
    else memcpy(name,"UNKNOWN   ",10);
    name[10]=0;
}
extern "C" void rdx_render(float* l,float* r) {
    if(editor_pending) {
        editor_pending=false;
        const auto& old=RDX_State::getState().workingPatch;
        bool reset_fx=old.common.monoPoly!=edited_patch.common.monoPoly;
        for(int s=0;s<2;++s)reset_fx=reset_fx || old.common.effects[s][0]!=edited_patch.common.effects[s][0] ||
            (edited_patch.common.effects[s][0]==EFFECT_DELAY && old.common.effects[s][2]!=edited_patch.common.effects[s][2]);
        if(fade==PRESET_FADE_OUT || reset_fx) {
            pending_patch=edited_patch;
            if(fade!=PRESET_FADE_OUT){fade=PRESET_FADE_OUT;pedal_before=pedal_down;}
        } else engine->updatePatch(edited_patch);
    }
    auto& ctl=RDX_State::getState().controls;
    ctl.pitchbendSemitones=(float)bend*(bend<0?1.0f/8192.0f:1.0f/8191.0f)*((int)RDX_State::getState().workingPatch.common.pbRange-64);
    ctl.modWheelFactor=(float)modulation*(1.0f/127.0f);
    const float performance_target=(float)midi_volume*(float)expression*(1.0f/(127.0f*127.0f));
    const bool ready=effects->serviceClear();
    if(ready) {
        engine->renderBlock(l,r);
        if(effects_on)effects->process(l,r);
    } else for(uint32_t i=0;i<DMA_BUFFER_LEN;++i)l[i]=r[i]=0.0f;
    for(uint32_t i=0;i<DMA_BUFFER_LEN;++i){performance_gain+=(performance_target-performance_gain)*0.004f;
        const float gain=preset_gain*performance_gain;l[i]*=gain;r[i]*=gain;}
    // A pending change can replace an arena still being prepared. Keep new
    // notes frozen until the new effect state is ready, then fade in.
    if(!ready && fade!=PRESET_FADE_OUT)return;
    if(fade==PRESET_STEADY)return;
    for(uint32_t i=0;i<DMA_BUFFER_LEN;++i) {
        const float g=fade==PRESET_FADE_OUT ? (127.0f-i)*(1.0f/127.0f) : i*(1.0f/127.0f);
        l[i]*=g;r[i]*=g;
    }
    if(fade==PRESET_FADE_OUT) {
        engine->applyPatch(pending_patch);
        preset_gain=output_trim();
        effects->begin(pending_patch);
        engine->sustain(pedal_before);
        for(uint32_t i=0;i<deferred_count;++i)dispatch(deferred[i]);
        deferred_count=0;
        fade=PRESET_FADE_IN;
    } else fade=PRESET_STEADY;
}
extern "C" void rdx_editor_snapshot(EditorSnapshot* out) {
    const auto& p=fade==PRESET_FADE_OUT?pending_patch:RDX_State::getState().workingPatch;
    memcpy(out->patch,&p,150);
    for(uint8_t i=0;i<5;++i)out->phaser[i]=effects->phaserValue(i);
    out->revision=editor_revision;out->preset=destination;out->fx=rdx_fx_status();out->dirty=editor_dirty;out->error=editor_error;
    out->midi_rx=midi_rx;out->midi_control=midi_control;
}
extern "C" void rdx_editor_adjust(uint8_t page,uint8_t op,uint8_t knob,int16_t delta,uint8_t fine) {
    if(page>=EDITOR_PAGES_COUNT||op>4||knob>3||!delta)return;
    if(page==EDITOR_MIDI){
        if(knob==0){int n=(int)midi_rx+delta;uint8_t next=(uint8_t)(n<0?0:n>16?16:n);if(next!=midi_rx){rdx_panic();midi_rx=next;}}
        else if(knob==1){int n=(int)midi_control+delta;midi_control=n>0?1:0;}
        else return;++editor_revision;return;
    }
    if(page==EDITOR_LAB){if(knob==1)effects->adjustPhaser(op,delta);else if(knob==2)effects->adjustPhaser(3,delta);else if(knob==3)effects->adjustPhaser(4,delta);++editor_revision;return;}
    if(op>3)return;
    RDX_Patch p=editor_pending?edited_patch:fade==PRESET_FADE_OUT?pending_patch:RDX_State::getState().workingPatch;
    auto change=[&](uint8_t& value,int lo,int hi){int n=(int)value+delta;value=(uint8_t)(n<lo?lo:n>hi?hi:n);};
    auto feedback=[&](RDX_OpParams& q){int n=(q.fbType?-(int)q.feedback:(int)q.feedback)+delta;n=n<-127?-127:n>127?127:n;q.feedback=(uint8_t)(n<0?-n:n);q.fbType=n<0?RDX_FB_SQUARE:RDX_FB_SAW;};
    auto scaling=[&](uint8_t& depth,uint8_t& curve){bool exp=curve==1||curve==2;int n=(curve<2?-(int)depth:(int)depth)+delta;n=n<-127?-127:n>127?127:n;depth=(uint8_t)(n<0?-n:n);curve=n<0?(exp?1:0):(exp?2:3);};
    auto curve=[&](uint8_t& value){const bool negative=value<2;int exp=(value==1||value==2)?1:0;exp+=delta;const bool isExp=exp>0;value=negative?(isExp?1:0):(isExp?2:3);};
    auto& q=p.ops[op];
    switch(page) {
    case EDITOR_HOME:case EDITOR_LEVEL:change(p.ops[knob].outLevel,0,127);break;
    case EDITOR_FEEDBACK:feedback(p.ops[knob]);break;
    case EDITOR_FREQ:if(fine)change(p.ops[knob].freqFine,0,99);else change(p.ops[knob].freqCoarse,0,31);break;
    case EDITOR_ALGO:if(knob==0)change(p.common.algorithm,0,11);else return;break;
    case EDITOR_VOICE:if(knob==0)change(p.common.transpose,40,88);else if(knob==1)change(p.common.monoPoly,0,2);else if(knob==2)change(p.common.portaTime,0,127);else change(p.common.pbRange,40,88);break;
    case EDITOR_OP1:if(knob==0)change(q.enable,0,1);else if(knob==1)change(q.freqMode,0,1);else if(knob==2){if(fine)change(q.freqFine,0,99);else change(q.freqCoarse,0,31);}else change(q.freqDetune,0,127);break;
    case EDITOR_OP2:if(knob==0)change(q.outLevel,0,127);else if(knob==1)change(q.velSens,0,127);else if(knob==2)feedback(q);else change(q.rateScaling,0,127);break;
    case EDITOR_OP3:if(knob==0)scaling(q.scaleLD,q.scaleLC);else if(knob==1)curve(q.scaleLC);else if(knob==2)curve(q.scaleRC);else scaling(q.scaleRD,q.scaleRC);break;
    case EDITOR_LFO1:if(knob==0)change(p.common.lfoWave,0,6);else if(knob==1)change(p.common.lfoSpeed,0,127);else if(knob==2)change(p.common.lfoDelay,0,127);else change(p.common.lfoPMD,0,127);break;
    case EDITOR_LFO2:change(p.ops[knob].lfoPMDEnable,0,1);break;
    case EDITOR_LFO3:change(p.ops[knob].lfoAMD,0,127);break;
    case EDITOR_FX1:case EDITOR_FX2:{int s=page==EDITOR_FX1?0:1;if(knob==0)change(p.common.effects[s][0],0,7);else if(knob>1)change(p.common.effects[s][knob-1],0,127);else return;break;}
    case EDITOR_AEG_RATES:change(q.egRate[knob],0,127);break;
    case EDITOR_AEG_LEVELS:change(q.egLevel[knob],0,127);break;
    case EDITOR_PEG_RATES:change(p.common.pegRate[knob],0,127);break;
    case EDITOR_PEG_LEVELS:change(p.common.pegLevel[knob],16,112);break;
    case EDITOR_PEG_ON:change(p.ops[knob].pegEnable,0,1);break;
    default:return;
    }
    stage_edit(p);
}
