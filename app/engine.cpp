#include "engine_api.h"
#include "editor_api.h"
#include "../src/rdx_engine.cpp"
#include "../src/rdx_profile.cpp"
#include "../src/rdx_effects.cpp"
#include "../src/rdx_budget.h"
#include "../src/rdx_measure.h"
#if defined(RDX_FM1)
extern "C" uint32_t plat_ticks24(void);
static uint32_t budget_ticks(){return plat_ticks24();}
#else
static uint32_t budget_ticks(){return 0;}
#endif
#include "benchmark.inc"
static RdxBudget budget;
static RdxVoiceMeasure voice_measure;
static uint32_t fx_last,measure_last_voices;
static bool measure_guarded_block;
static bool budget_dirty=true,budget_keep_limit,budget_sample_valid;
static uint32_t synth_ticks;
static uint32_t synth_peak,fx_peak;
static volatile uint8_t detail_enabled;
static uint32_t detail_parts[RENDER_PARTS_COUNT],detail_peak,detail_blocks,detail_voices;
static uint32_t detail_fine[RENDER_FINE_PARTS_COUNT];
extern "C" const char* rdx_build_label(void){
#if defined(RDX_TEST_O3) && defined(RDX_TEST_UNROLL)
    return "O3 U4";
#elif defined(RDX_TEST_O3)
    return "O3";
#elif defined(RDX_TEST_UNROLL)
    return "O2 U4";
#else
    return "O2 B32";
#endif
}
static uint32_t detail_ticks(){
#ifdef PLAT_HOST
    return 0;
#else
    return budget_ticks();
#endif
}

// Explicit construction at boot, with static storage and no heap or C++ runtime.
alignas(RdxEngine) static unsigned char storage[sizeof(RdxEngine)];
static RdxEngine* engine;
#include "dry_pipeline.inc"
static RDX_RAM_CODE void render_core(RdxEngine* synth,float* left,float* right){synth->renderBlock(left,right);}
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
    if(!budget_dirty)budget_keep_limit=true;
    budget_dirty=true;
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
    voice_measure=RdxVoiceMeasure{};measure_guarded_block=false;fx_last=0;
    destination=1;fade=PRESET_STEADY;deferred_count=0;pedal_down=pedal_before=false;
    budget.reset(0);budget_dirty=true;budget_keep_limit=false;budget_sample_valid=false;synth_ticks=0;
    synth_peak=fx_peak=0;
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
    dry.valid=dry.prepared=0;
    if(fade==PRESET_FADE_OUT) {
        engine->applyPatch(pending_patch);
        dry.valid=0;
        preset_gain=output_trim();
    }
    fade=PRESET_STEADY;deferred_count=0;pedal_down=false;
    engine->panic();
    effects->begin(RDX_State::getState().workingPatch);
}
extern "C" void rdx_fx_toggle(void) {effects_on=!effects_on;if(effects_on)budget_keep_limit=false;else if(!budget_dirty)budget_keep_limit=true;budget_dirty=true;}
extern "C" uint32_t rdx_fx_status(void) {return !effects->valid() ? 2u : effects_on ? 1u : 0u;}
extern "C" uint32_t rdx_preset_count(void) {return RdxEngine::presetCount();}
extern "C" void rdx_preset_request(uint32_t i) {
    if(i>=RdxEngine::presetCount())return;
    RdxEngine::presetPatch(i,pending_patch);editor_pending=false;editor_dirty=editor_error=false;++editor_revision;
    external_patch=false;destination=i;fade=PRESET_FADE_OUT;deferred_count=0;pedal_before=pedal_down;
    budget_dirty=true;budget_keep_limit=false;
}
extern "C" int rdx_patch_valid(const uint8_t* bytes){RDX_Patch p;memcpy(&p,bytes,150);return RdxEngine::validPatch(p)&&RdxEffects::required(p)<=RdxEffects::arenaFloats;}
extern "C" void rdx_patch_export(uint8_t* bytes){memcpy(bytes,editor_pending?&edited_patch:fade==PRESET_FADE_OUT?&pending_patch:&RDX_State::getState().workingPatch,150);}
extern "C" void rdx_patch_import(const uint8_t* bytes){
    memcpy(&pending_patch,bytes,150);editor_pending=false;external_patch=true;editor_dirty=true;editor_error=false;++editor_revision;
    fade=PRESET_FADE_OUT;deferred_count=0;pedal_before=pedal_down;
    budget_dirty=true;budget_keep_limit=false;
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
    dry.prepared=0;
    if(dry.enabled){
        // Previous job has completed before entry. Apply the last admission
        // decision now, never while CPU1 owns the voices.
        engine->setVoiceLimit(budget.limit);
        dry.observed_turn=dry.turn_ticks;
        dry.observed_voices=engine->renderedVoices();
    }
    budget_sample_valid=false;synth_ticks=fx_last=0;
    if(editor_pending) {
        editor_pending=false;
        const auto& old=RDX_State::getState().workingPatch;
        bool reset_fx=old.common.monoPoly!=edited_patch.common.monoPoly;
        for(int s=0;s<2;++s)reset_fx=reset_fx || old.common.effects[s][0]!=edited_patch.common.effects[s][0] ||
            (edited_patch.common.effects[s][0]==EFFECT_DELAY && old.common.effects[s][2]!=edited_patch.common.effects[s][2]);
        if(fade==PRESET_FADE_OUT || reset_fx) {
            if(reset_fx){budget_dirty=true;budget_keep_limit=false;}
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
        const uint32_t start=budget_ticks();
        uint32_t parts[RENDER_PARTS_COUNT],fine[RENDER_FINE_PARTS_COUNT];
        if(detail_enabled)RdxProfile::render(*engine,l,r,parts,detail_ticks,detail_enabled==DETAIL_FINE?fine:nullptr);
        else if(dry.enabled){
            if(dry.valid)RDX_FAR(RdxEngine::mixDry)(dry.samples,dry.gain,l,r);
            else for(uint32_t i=0;i<DMA_BUFFER_LEN;++i)l[i]=r[i]=0.0f;
            dry.valid=0;dry.prepared=1;
        }else RDX_FAR(render_core)(engine,l,r);
        synth_ticks=dry.enabled?dry.worker_ticks:budget_ticks()-start;
        if(detail_enabled){
            if(detail_blocks!=UINT32_MAX)++detail_blocks;
#ifdef PLAT_HOST
            const uint32_t measured=0;
#else
            const uint32_t measured=synth_ticks;
#endif
            if(measured>=detail_peak){detail_peak=measured;detail_voices=engine->renderedVoices();
                for(uint32_t i=0;i<RENDER_PARTS_COUNT;++i)detail_parts[i]=parts[i];
                for(uint32_t i=0;i<RENDER_FINE_PARTS_COUNT;++i)detail_fine[i]=detail_enabled==DETAIL_FINE?fine[i]:0;}
        }
        if(synth_ticks>synth_peak)synth_peak=synth_ticks;
        budget_sample_valid=fade==PRESET_STEADY&&RDX_State::getState().workingPatch.common.monoPoly==RDX_MODE_POLY;
        // The completed dry bus has been copied to l/r. Reuse its storage for
        // the NEXT block now, overlapping generation with this block's FX and
        // output. Patch/fade transitions dispatch at callback end instead.
        if(dry.enabled&&fade==PRESET_STEADY&&dry.kick&&rdx_dry_publish())dry.kick();
        const uint32_t fx_start=budget_ticks();
        if(effects_on)effects->process(l,r);
        const uint32_t fx_ticks=budget_ticks()-fx_start;fx_last=fx_ticks;if(fx_ticks>fx_peak)fx_peak=fx_ticks;
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
        dry.valid=0;
        engine->sustain(pedal_before);
        for(uint32_t i=0;i<deferred_count;++i)dispatch(deferred[i]);
        deferred_count=0;
        fade=PRESET_FADE_IN;
    } else fade=PRESET_STEADY;
}
static void measure_restore(){engine->panic();engine->setVoiceLimit(budget.limit);engine->sustain(pedal_down);}
extern "C" void rdx_budget_observe(uint32_t ticks,uint32_t now){
    if(dry.enabled){
        // Pipeline throughput is bounded by the busier core/turnaround, not
        // the sum of overlapping worker and CPU0 service times.
        if(dry.observed_turn>ticks)ticks=dry.observed_turn;
    }
    if(voice_measure.active()){
        measure_guarded_block=true;measure_last_voices=engine->renderedVoices();
#ifdef PLAT_HOST
        // A host descheduling pause is not an FM1 deadline measurement.
        voice_measure.observe(now,0,0,0,measure_last_voices,budget_sample_valid);
#else
        voice_measure.observe(now,ticks,synth_ticks,fx_last,measure_last_voices,budget_sample_valid);
#endif
        if(!voice_measure.active())measure_restore();
        return;
    }
    if(detail_enabled)return; // Do not learn a normal voice budget from diagnostic execution.
    if(budget_dirty){uint8_t previous=budget.limit;budget.reset(now);if(budget_keep_limit)budget.limit=previous;budget_dirty=budget_keep_limit=false;}
    budget.observe(now,ticks,synth_ticks,dry.enabled?dry.observed_voices:engine->renderedVoices(),budget_sample_valid);
    if(!dry.enabled)engine->setVoiceLimit(budget.limit);
}
extern "C" void rdx_budget_overload(uint32_t now){
    if(dry.enabled)return; // observe already made this block's decision before publication
    if(voice_measure.active()||measure_guarded_block){
        voice_measure.overload(measure_guarded_block?measure_last_voices:engine->renderedVoices());
        measure_restore();return;
    }
    budget.overload(now);engine->setVoiceLimit(budget.limit);
}
extern "C" int rdx_measure_active(void){return voice_measure.active();}
extern "C" void rdx_measure_command(uint8_t command,uint32_t now){
    if(dry.enabled&&command==COST_START)return; // sweep is synchronous-only
    if(voice_measure.active()){voice_measure.stop(COST_STOPPED);measure_restore();}
    if(command==COST_START){
        rdx_detail_set(DETAIL_OFF);uint32_t predicted=budget.prediction(6);
#ifdef PLAT_HOST
        predicted=0;
#endif
        voice_measure.start(now,predicted);
    }
}
extern "C" void rdx_measure_prepare(uint32_t now){
    measure_guarded_block=false;
    if(!voice_measure.active())return;
    voice_measure.stamp=now;
    if(RDX_State::getState().workingPatch.common.monoPoly!=RDX_MODE_POLY){
        voice_measure.stop(COST_NEED_POLY);measure_restore();return;
    }
    if(editor_pending||fade!=PRESET_STEADY){
        if(voice_measure.state==COST_ARMED&&!voice_measure.target&&(uint32_t)(now-voice_measure.stageAt)<5000u)return;
        voice_measure.stop(COST_CHANGED);measure_restore();return;
    }
    if(voice_measure.state==COST_ARMED){
        static const uint8_t notes[6]={48,55,60,64,67,72};
        engine->panic();engine->sustain(pedal_down);engine->setVoiceLimit(voice_measure.target?voice_measure.target:1);
        for(uint32_t i=0;i<voice_measure.target;++i)engine->noteOn(notes[i],100);
        voice_measure.begin(now);
    }
}
extern "C" void rdx_measure_snapshot(uint32_t* out){voice_measure.snapshot(out);}
extern "C" uint32_t rdx_voice_status(void){return engine->voiceLimit()|((uint32_t)engine->activeVoices()<<8)|((uint32_t)RdxEngine::voiceCapacity<<16);}
extern "C" void rdx_cost_reset(void){synth_peak=fx_peak=0;}
extern "C" void rdx_cost_profile(uint32_t* ticks3){ticks3[0]=synth_peak;ticks3[1]=fx_peak;ticks3[2]=budget.prediction(RdxBudget::maximum);}
extern "C" void rdx_detail_set(uint8_t on){if(on&&voice_measure.active())rdx_measure_command(COST_STOP,0);detail_enabled=on>=DETAIL_MODES_COUNT?DETAIL_OFF:on;
    if(dry.enabled)detail_enabled=DETAIL_OFF;
    if(detail_enabled){detail_peak=detail_blocks=detail_voices=0;
        for(uint32_t i=0;i<RENDER_PARTS_COUNT;++i)detail_parts[i]=0;
        for(uint32_t i=0;i<RENDER_FINE_PARTS_COUNT;++i)detail_fine[i]=0;}}
extern "C" void rdx_detail_fine(uint32_t* out){for(uint32_t i=0;i<RENDER_FINE_PARTS_COUNT;++i)out[i]=detail_fine[i];}
extern "C" void rdx_detail_profile(uint32_t* out){
    for(uint32_t i=0;i<RENDER_PARTS_COUNT;++i)out[i]=detail_parts[i];
    out[5]=detail_peak;out[6]=detail_blocks;out[7]=detail_enabled;out[8]=detail_voices;
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
