#include "engine_api.h"
#include "editor_api.h"
#include "benchmark.h"
#include "../src/rdx_encoder.h"
#include "../src/gui/font_mo.h"
#include "../src/rdx_looper.h"
#include "../src/rdx_drafts.h"

enum eEventType { EVENT_ON, EVENT_OFF, EVENT_PANIC, EVENT_PRESET, EVENT_FX, EVENT_EDITOR, EVENT_BEND, EVENT_CC, EVENT_MEASURE, EVENT_SPLIT, EVENT_LOOP, EVENTS_COUNT };
static void event_push(uint8_t,uint8_t,uint8_t);
typedef struct {uint8_t type,note,velocity,control;int16_t delta;} rdx_event_t;
static rdx_event_t events[128];
static volatile uint32_t event_read,event_write,panic_requested;
static uint32_t keys_before,buttons_before;
static int32_t octave;
static uint8_t key_notes[NKEYS];
static volatile int32_t preset=1;
static float left[APP_BLOCK],right[APP_BLOCK];
static float master_gain;
static uint8_t editor_page,editor_op,editor_tune,selector_used;
enum eStoreScreen { STORE_CLOSED,STORE_EDIT,STORE_CONFIRM,STORE_DONE,STORE_ERROR,STORE_SCREENS_COUNT };
static uint8_t store_screen,store_slot,store_cursor,store_trim;
static uint8_t store_patch[150],store_bank[PLAT_STORE_MAX];
static RdxLooper looper;
static uint8_t looper_screen;
enum eDraftScreen { DRAFT_CLOSED,DRAFT_SELECT,DRAFT_SAVE_CONFIRM,DRAFT_LOAD_CONFIRM,DRAFT_SAVED,DRAFT_LOADED,DRAFT_ERROR,DRAFT_SCREENS_COUNT };
enum eDraftAction { DRAFT_ACTION_NONE,DRAFT_ACTION_SAVE,DRAFT_ACTION_LOAD,DRAFT_ACTIONS_COUNT };
static uint8_t draft_screen,draft_slot,draft_mask;
static char draft_names[DRAFT_SLOTS][11];
static uint8_t draft_header[DRAFT_HEADER];
static volatile uint8_t maintenance;
static const DraftIO draft_io={OBJ_DRAFT_BASE,plat_store_load,plat_store_save,rdx_patch_valid,plat_store_size};
static void draft_refresh(void){
    uint8_t m[DRAFT_MANIFEST];draft_mask=0;memset(draft_names,0,sizeof draft_names);
    for(uint32_t slot=0;slot<DRAFT_SLOTS;++slot)if(draft_manifest(&draft_io,slot,m)){draft_mask|=(uint8_t)(1u<<slot);memcpy(draft_names[slot],m+20,10);}
}
static void draft_commit(void);
enum eOverlay { OVERLAY_NONE,OVERLAY_STORE,OVERLAY_LOOP,OVERLAY_DRAFT,OVERLAYS_COUNT };
static uint8_t live_notes[128],loop_notes[128],live_cc[4]={0,127,127,0};
static uint16_t live_bend=8192;
static void loop_release(void){
    memset(loop_notes,0,sizeof loop_notes);rdx_phrase_release();
    rdx_control_change(1,live_cc[0]);rdx_control_change(7,live_cc[1]);rdx_control_change(11,live_cc[2]);rdx_control_change(64,live_cc[3]);rdx_pitch_bend(live_bend);
}
static void loop_emit(uint8_t type,uint8_t a,uint8_t b){
    if(type==LOOP_RELEASE){loop_release();return;}
    if(type==LOOP_NOTE){if(b){if(loop_notes[a]<255)++loop_notes[a];rdx_phrase_note(a,b);}else{if(loop_notes[a])--loop_notes[a];rdx_phrase_note(a,0);}}
    else if(type==LOOP_BEND)rdx_phrase_bend((uint16_t)(a|(b<<7)));
    else if(type==LOOP_CC){rdx_phrase_cc(a,b);}
}
static void guide_restart(void);
static void performance_event(uint8_t type,uint8_t a,uint8_t b){
    if(looper.state==LOOP_ARMED&&type==LOOP_NOTE&&b){guide_restart();if(!rdx_phrase_capture())loop_stop(&looper);}
    if(type==LOOP_NOTE){if(b){if(live_notes[a]<255)++live_notes[a];rdx_note_on(a,b);}else{if(live_notes[a])--live_notes[a];rdx_note_off(a);}}
    else if(type==LOOP_BEND){live_bend=(uint16_t)(a|(b<<7));rdx_pitch_bend(live_bend);}
    else if(type==LOOP_CC){if(a==121){live_bend=8192;live_cc[0]=live_cc[3]=0;live_cc[2]=127;}if(a==1)live_cc[0]=b;else if(a==7)live_cc[1]=b;else if(a==11)live_cc[2]=b;else if(a==64)live_cc[3]=b;rdx_control_change(a,b);}
    if(type!=LOOP_CC||a==1||a==7||a==11||a==64)loop_capture(&looper,type,a,b);
}
static void phrase_panic(void){loop_stop(&looper);loop_release();memset(live_notes,0,sizeof live_notes);memset(loop_notes,0,sizeof loop_notes);live_cc[3]=0;rdx_panic();}
enum eGuideMode { GUIDE_OFF,GUIDE_AUTO,GUIDE_ON,GUIDE_MODES_COUNT };
static uint8_t guide_mode=GUIDE_AUTO,guide_force,guide_beat;
static uint32_t guide_phase,click_left,click_phase,beat_pulses;
static void guide_restart(void){guide_phase=0;guide_force=1;guide_beat=0;}
static void app_lights(void);
static void store_commit(void);
static EditorSnapshot editor_view;
static uint8_t editor_redraw;
// Audio owns the counters. UI copies/resets only under interrupt exclusion.
typedef struct {uint32_t blocks,last,peak,control_peak,render_peak,output_peak,over90;} RdxTiming;
static RdxTiming timing;
static uint8_t timing_page;
static uint8_t bench_selection;
static uint8_t bench_started;
static uint8_t profile_fine;
static uint8_t cost_selection,cost_follow=1;
static uint8_t dry_view;
static uint32_t cost_xruns;
static volatile uint8_t preset_home_requested;
#include "control_bridge.h"
#include "core_test.h"
#define RDX_CONTROL_ISOLATED 1
#include "ui_profile.h"
_Static_assert(UI_PARTS_COUNT==sizeof(control_request.ui)/sizeof(control_request.ui[0]),"UI snapshot size");
static void audio_checkpoint(void);
static void audio_checkpoint_at(uint32_t where);
#include "gui_worker.h"
static void audio_job(uint32_t unused){(void)unused;rdx_dry_service();}
static void audio_kick(void){
    // All CPU0 dispatch sites exclude audio IRQs, including main-loop dispatch.
    uint32_t f=plat_lock();
    if(!rdx_dry_acquire()&&plat_cpu2_on()&&!plat_cpu2_busy())plat_cpu2_run(audio_job,0);
    plat_unlock(f);
}
static void audio_checkpoint_at(uint32_t where){if(plat_core()==1)rdx_dry_service_at(where);}
static void audio_checkpoint(void){audio_checkpoint_at(DRY_AT_CONTROL);}
static uint32_t audio_control_lock(void){
    // Main-loop only: snapshots/panic must not inspect a worker-owned voice.
    // Audio never enters this bounded wait. Main also retires BSP faults here.
    uint32_t start=plat_ticks24(),f=plat_lock();
    while(!rdx_dry_acquire()){
        plat_unlock(f);audio_kick();
        if((uint32_t)(plat_ticks24()-start)>2400000u){
            plat_cpu2_wait();
            f=plat_lock();
            // wait either retired CPU1 or completed its last job. A late
            // audio IRQ might have dispatched again; never reclaim that job.
            if(!plat_cpu2_busy()){rdx_dry_retired();panic_requested=1;return f;}
            plat_unlock(f);
        }
        for(volatile uint32_t pause=0;pause<128u;++pause){}
        f=plat_lock();
    }
    return f;
}
static uint8_t core_selection=CORE_TEST_IDLE;
#include "soundmondo.h"
static uint32_t timing_started,timing_xruns,timing_draw;
void app_overload(void){if(timing.over90!=UINT32_MAX)++timing.over90;rdx_budget_overload(plat_ms());}
static void timing_reset(void){
    if(control_active){control_reply.reset_timing=1;timing_started=plat_ms();timing_xruns=control_request.xruns;timing_draw=0;return;}
    uint32_t f=audio_control_lock();memset(&timing,0,sizeof(timing));rdx_cost_reset();rdx_dry_reset_stats();ui_profile_reset();plat_unlock(f);
    timing_started=plat_ms();timing_xruns=plat_xruns();timing_draw=0;
}
static RdxEncoderMotion knob_motion[4];
static void reset_knob_motion(void){for(uint8_t k=0;k<4;++k)rdx_encoder_reset(&knob_motion[k]);}
static void detail_set(uint8_t on){if(control_active){control_detail(on);return;}uint32_t f=plat_lock();rdx_detail_set(on);plat_unlock(f);}
static void preset_home(void){if(timing_page==6)event_push(EVENT_MEASURE,COST_STOP,0);editor_page=EDITOR_HOME;timing_page=0;detail_set(0);editor_redraw=1;reset_knob_motion();}
static uint8_t wrap_selection(uint8_t at,int32_t move,uint8_t count){
    int32_t next=((int32_t)at+move)%count;if(next<0)next+=count;return (uint8_t)next;
}
static void select_in_menu(int32_t move){
    if((editor_page>=EDITOR_OP1&&editor_page<=EDITOR_OP3)||editor_page==EDITOR_AEG_RATES||editor_page==EDITOR_AEG_LEVELS){
        editor_op=wrap_selection(editor_op,move,4);return;
    }
    if(editor_page>=EDITOR_LFO1&&editor_page<=EDITOR_LFO3){editor_page=EDITOR_LFO1+wrap_selection(editor_page-EDITOR_LFO1,move,3);return;}
    if(editor_page>=EDITOR_PEG_RATES&&editor_page<=EDITOR_PEG_ON){editor_page=EDITOR_PEG_RATES+wrap_selection(editor_page-EDITOR_PEG_RATES,move,3);return;}
    if(editor_page==EDITOR_FX1||editor_page==EDITOR_FX2){editor_page=EDITOR_FX1+wrap_selection(editor_page-EDITOR_FX1,move,2);return;}
    if(editor_page==EDITOR_VOICE||editor_page==EDITOR_MIDI){editor_page=wrap_selection(editor_page==EDITOR_MIDI,move,2)?EDITOR_MIDI:EDITOR_VOICE;return;}
    if(editor_page==EDITOR_LAB){editor_tune=wrap_selection(editor_tune,move,5);return;}
    // Home and the four all-operator overview pages share one menu node.
    if(editor_page<=EDITOR_ALGO)editor_page=wrap_selection(editor_page,move,5);
}
static int continuous_parameter(uint8_t page,uint8_t knob){
    switch(page){
      case EDITOR_HOME:case EDITOR_LEVEL:case EDITOR_FEEDBACK:case EDITOR_FREQ:case EDITOR_OP2:case EDITOR_LFO3:
      case EDITOR_AEG_RATES:case EDITOR_AEG_LEVELS:case EDITOR_PEG_RATES:case EDITOR_PEG_LEVELS:return 1;
      case EDITOR_VOICE:return knob!=1;
      case EDITOR_OP1:return knob>=2;
      case EDITOR_OP3:return knob==0||knob==3;
      case EDITOR_LFO1:return knob!=0;
      case EDITOR_FX1:case EDITOR_FX2:return knob>=2;
      case EDITOR_LAB:return knob>=1;
      default:return 0;
    }
}

// Main loop producer; interrupt consumer. plat_lock supplies the platform's
// interrupt exclusion. Full queues deliberately panic and discard the batch.
static void event_push(uint8_t type,uint8_t note,uint8_t velocity) {
    if(control_active){control_push((rdx_event_t){type,note,velocity,0,0});return;}
    uint32_t f=plat_lock(),w=event_write;
    if(w-event_read==128u) panic_requested=1;
    else {events[w&127u]=(rdx_event_t){type,note,velocity,0,0};event_write=w+1u;}
    plat_unlock(f);
}
static void editor_event(uint8_t page,uint8_t op,uint8_t control,int32_t delta,uint8_t fine) {
    if(delta>127)delta=127;if(delta<-127)delta=-127;
    if(control_active){control_push((rdx_event_t){EVENT_EDITOR,page,op,(uint8_t)(control|(fine?128u:0u)),(int16_t)delta});return;}
    uint32_t f=plat_lock(),w=event_write;
    if(w-event_read==128u)panic_requested=1;
    else {events[w&127u]=(rdx_event_t){EVENT_EDITOR,page,op,(uint8_t)(control|(fine?128u:0u)),(int16_t)delta};event_write=w+1u;}
    plat_unlock(f);
}
void app_init(void) {
    event_read=event_write=panic_requested=0;keys_before=buttons_before=0;
    octave=0;preset=1;master_gain=0.0f;rdx_boot();loop_init(&looper);looper_screen=0;live_cc[0]=live_cc[3]=0;live_cc[1]=live_cc[2]=127;live_bend=8192;memset(live_notes,0,sizeof live_notes);memset(loop_notes,0,sizeof loop_notes);
    plat_usb_audio_full(1); // Record at fixed level; MASTER controls local monitoring.
    editor_page=EDITOR_HOME;editor_op=editor_tune=selector_used=0;
    editor_redraw=1;
    reset_knob_motion();
    timing_page=0;bench_started=bench_selection=profile_fine=0;preset_home_requested=0;timing_reset();
    dry_view=0;
    sm_reset();
    core_test_init();
    rdx_dry_set_kick(audio_kick);
    for(uint32_t bank=0;bank<2u;++bank){int n=plat_store_load(OBJ_USER_BANK0+bank,store_bank,RDX_USER_BANK_BYTES);if(n>0)rdx_user_decode(bank,store_bank,(uint32_t)n);}
    guide_mode=GUIDE_AUTO;guide_phase=click_left=click_phase=beat_pulses=0;guide_force=guide_beat=0;memset(key_notes,0,sizeof key_notes);
    store_screen=STORE_CLOSED;draft_screen=DRAFT_CLOSED;draft_slot=0;maintenance=0;draft_refresh();
    rdx_dry_mode(plat_cpu2_on()!=0);
}
void app_panic(void) {
    // All platform app_panic entry points are CPU0 main-loop update/recovery.
    gui_collect(1);gui_ready=0;
    core_mode=CORE_TEST_OFF;
    if(core_pending){plat_cpu2_wait();core_pending=0;}
    uint32_t f=audio_control_lock();
    rdx_measure_command(COST_STOP,plat_ms());
    rdx_dry_mode(0);
    phrase_panic();event_read=event_write;panic_requested=0;
    plat_unlock(f);
}
void app_render(int32_t* lr,uint32_t n,uint32_t master_q12) {
    uint32_t i;
    if(maintenance){for(i=0;i<n*2u;++i)lr[i]=0;return;}
    if(n!=APP_BLOCK) {for(i=0;i<n*2u;++i)lr[i]=0;return;}
    if(!rdx_dry_acquire()){
        for(i=0;i<n*2u;++i)lr[i]=0;
        rdx_dry_late();audio_kick();return;
    }
    rdx_measure_recover();
    const uint32_t t0=plat_ticks24();
    rdx_dry_block_start(t0);
    sm_audio();
    if(panic_requested) {rdx_measure_command(COST_STOP,plat_ms());phrase_panic();event_read=event_write;panic_requested=0;}
    for(i=0;i<32u && event_read!=event_write;++i) {
        rdx_event_t e=events[event_read&127u];++event_read;
        if(e.type!=EVENT_MEASURE&&rdx_measure_active())rdx_measure_command(COST_STOP,plat_ms());
        if(e.type==EVENT_ON)performance_event(LOOP_NOTE,e.note,e.velocity);
        else if(e.type==EVENT_OFF)performance_event(LOOP_NOTE,e.note,0);
        else if(e.type==EVENT_PANIC)phrase_panic();
        else if(e.type==EVENT_PRESET)rdx_preset_request(e.note);
        else if(e.type==EVENT_FX)rdx_fx_toggle();
        else if(e.type==EVENT_EDITOR)rdx_editor_adjust(e.note,e.velocity,e.control&127u,e.delta,(e.control&128u)!=0);
        else if(e.type==EVENT_BEND)performance_event(LOOP_BEND,(uint8_t)(e.delta&127),(uint8_t)((uint16_t)e.delta>>7));
        else if(e.type==EVENT_CC){
            if(e.note==120||e.note==126||e.note==127)phrase_panic();
            else if(e.note==123||e.note==124||e.note==125){loop_stop(&looper);loop_release();memset(live_notes,0,sizeof live_notes);}
            performance_event(LOOP_CC,e.note,e.velocity);
        }
        else if(e.type==EVENT_MEASURE)rdx_measure_command(e.note,plat_ms());
        else if(e.type==EVENT_SPLIT)rdx_dry_mode(e.note&&plat_cpu2_on());
        else if(e.type==EVENT_LOOP){if(e.note==LOOP_GUIDE){if(e.delta>=0&&e.delta<GUIDE_MODES_COUNT)guide_mode=(uint8_t)e.delta;continue;}uint8_t before=looper.state,before_quant=looper.quantize;loop_command(&looper,e.note,e.delta);if(e.note==LOOP_QUANTIZE&&before_quant!=looper.quantize&&before>=LOOP_PLAY){loop_release();looper.position=0;looper.fraction=0;looper.deferred_count=0;memset(looper.cursor,0,sizeof looper.cursor);guide_restart();}if(e.note==LOOP_CLEAR&&before==LOOP_STOP)rdx_phrase_clear();if((before==LOOP_STOP&&looper.state!=LOOP_STOP)||(before==LOOP_RECORD&&looper.state==LOOP_PLAY))guide_restart();if((before==LOOP_RECORD&&looper.state==LOOP_PLAY)||looper.state==LOOP_STOP)loop_release();}
    }
    loop_advance(&looper,loop_emit);
    rdx_measure_prepare(plat_ms());
    const uint32_t t1=plat_ticks24();
    rdx_render(left,right);
    const uint32_t t2=plat_ticks24();
    const uint8_t clock_on=looper.state!=LOOP_STOP;
    const uint8_t audible=guide_mode==GUIDE_ON||(guide_mode==GUIDE_AUTO&&(looper.state==LOOP_ARMED||looper.state==LOOP_RECORD||looper.state==LOOP_OVERDUB||looper.state==LOOP_OVERDUB_ARMED));
    static const float click_wave[16]={0.0f,0.382683f,0.707107f,0.923880f,1.0f,0.923880f,0.707107f,0.382683f,0.0f,-0.382683f,-0.707107f,-0.923880f,-1.0f,-0.923880f,-0.707107f,-0.382683f};
    const float target=(float)(master_q12>4096u ? 4096u : master_q12)*(1.0f/4096.0f);
    for(i=0;i<n;++i) {
        master_gain+=(target-master_gain)*0.004f;
        float click=0.0f;
        if(clock_on){
            if(guide_force||guide_phase>=2646000u){
                if(guide_phase>=2646000u)guide_phase-=2646000u;
                guide_force=0;++beat_pulses;guide_beat=(uint8_t)((guide_beat+1u)%4u);click_left=audible?221u:0;click_phase=0;
            }
            guide_phase+=looper.tempo;
            if(click_left&&audible){click=click_wave[(click_phase>>(guide_beat==1u?0u:1u))&15u]*(float)click_left*(0.18f/221.0f);++click_phase;--click_left;}
        }else{click_left=0;guide_phase=0;}
        left[i]+=click;right[i]+=click;
        float l=left[i]>1.0f ? 1.0f : left[i]<-1.0f ? -1.0f : left[i];
        float r=right[i]>1.0f ? 1.0f : right[i]<-1.0f ? -1.0f : right[i];
        lr[2u*i]=(int32_t)(l*master_gain*8388607.0f);
        lr[2u*i+1u]=(int32_t)(r*master_gain*8388607.0f);
    }
    const uint32_t t3=plat_ticks24();
    timing.last=t3-t0;if(timing.last>timing.peak)timing.peak=timing.last;
    if(t1-t0>timing.control_peak)timing.control_peak=t1-t0;
    if(t2-t1>timing.render_peak)timing.render_peak=t2-t1;
    if(t3-t2>timing.output_peak)timing.output_peak=t3-t2;
    if(timing.blocks!=UINT32_MAX)++timing.blocks;
    rdx_budget_observe(t3-t0,plat_ms());
    if(rdx_dry_publish())audio_kick();
}
// Worker callbacks consume captured input and engine snapshots only.
#ifndef PLAT_HOST
static uint32_t wait_audio_ticks;
static void control_wait_audio(void){
    const uint32_t start=ui_ticks();audio_checkpoint_at(DRY_AT_WAIT);
    wait_audio_ticks+=ui_ticks()-start;
}
#endif
static void control_dma_wait(void){
    audio_checkpoint_at(DRY_AT_WAIT);
    const uint32_t start=ui_ticks();uint32_t nested=0;
#ifndef PLAT_HOST
    if(plat_core()==1){
        wait_audio_ticks=0;
        lcd_sync_service(control_wait_audio,plat_ticks24,2400000u); // 100 ms additional wall ceiling
        nested=wait_audio_ticks;
    }else lcd_sync();
#else
    lcd_sync();
#endif
    ui_record_excluding(UI_DMA_WAIT,start,nested);
    audio_checkpoint_at(DRY_AT_WAIT);
}
static void control_blit(uint32_t x,uint32_t y){
    control_dma_wait();
    // Publish CPU1's RAM canvas before starting peripheral DMA reads.
    audio_checkpoint_at(DRY_AT_SUBMIT);plat_cpu2_sync();
    const uint32_t start=ui_ticks();cv_blit(x,y);ui_record(UI_SUBMIT,start);
    audio_checkpoint_at(DRY_AT_SUBMIT);
}
static void control_begin(uint32_t w,uint32_t h,uint16_t color){
    control_dma_wait();
    audio_checkpoint_at(DRY_AT_CLEAR);
    const uint32_t start=ui_ticks();cv_begin(w,h,color);ui_record(UI_CLEAR,start);
    audio_checkpoint_at(DRY_AT_CLEAR);
}
enum ePixelStyle { PIXEL_BODY,PIXEL_LARGE,PIXEL_STYLES_COUNT };
// Same source glyphs and 15/8 pixel magnification as the original ESP panel.
// Strips remain bounded; the audio worker is serviced between every glyph.
static uint32_t pixel_length(const char* text){uint32_t n=0;while(text[n]&&n<39u)++n;return n;}
static int32_t pixel_text(int32_t x,int32_t y,const char* text,uint32_t length,uint32_t numerator){
    static const uint8_t columns[2][19]={{0,0,1,1,2,2,3,3,4,4,0,0,0,0,0,0,0,0,0},{0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,4,4,4,4}};
    static const uint8_t rows[2][26]={{1,1,2,2,4,4,8,8,16,16,32,32,64,0,0,0,0,0,0,0,0,0,0,0,0,0},{1,1,1,1,2,2,2,2,4,4,4,4,8,8,8,16,16,16,16,32,32,32,32,64,64,64}};
    const uint32_t large=numerator==30u,height=large?26u:13u,width=large?19u:10u;
    for(uint32_t at=0;at<length;++at){
        audio_checkpoint_at(DRY_AT_TEXT);const uint32_t start=ui_ticks();
        uint8_t c=(uint8_t)text[at];if(c<16u)c=' ';
        const uint8_t* glyph=font_mo[c-16u];
        const int32_t left=x+(int32_t)(at*6u*numerator/8u);
        for(uint32_t dy=0;dy<height;++dy){int32_t yy=y+(int32_t)dy;if(yy<0||yy>=8)continue;
            uint32_t mask=rows[large][dy];
            for(uint32_t dx=0;dx<width;++dx)if(glyph[columns[large][dx]]&mask)cv_pset(left+(int32_t)dx,yy,C_WHITE);
        }
        ui_record(UI_TEXT,start);
    }
    audio_checkpoint_at(DRY_AT_TEXT);return x+(int32_t)(length*6u*numerator/8u);
}
static void control_fill(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint16_t color){
    if(x>=240u||y>=240u||!w||!h)return;
    if(w>240u-x)w=240u-x;if(h>240u-y)h=240u-y;
    for(uint32_t row=0;row<h;row+=8u){uint32_t n=h-row;if(n>8u)n=8u;control_begin(w,n,color);control_blit(x,y+row);}
}
#define cv_blit control_blit
#define cv_begin control_begin
#define lcd_fill control_fill
#define plat_lock control_lock
#define plat_unlock control_unlock
#define plat_keys control_keys
#define plat_buttons control_buttons
#define plat_enc control_enc
#define rdx_editor_snapshot control_snapshot
#define rdx_cost_profile control_cost
#define rdx_detail_profile control_profile
#define rdx_detail_fine control_fine
#define rdx_detail_set control_detail
#define rdx_voice_status control_voices
#define rdx_fx_status control_fx
#define plat_xruns control_xruns
#define plat_cpu_pct control_cpu
#define plat_cpu_khz control_khz
#define timing control_request.timing
static void control_input(void) {
    uint8_t go_home=control_request.home;control_request.home=0;
    if(go_home)preset_home();
    const uint8_t previous_page=editor_page,previous_op=editor_op;
    uint32_t k,keys=plat_keys(),buttons=plat_buttons();
    uint32_t pressed=buttons&~buttons_before;
    uint32_t released=buttons_before&~buttons;
    if((pressed&(1u<<B_REC))&&(buttons&(1u<<B_SEL))){
        if(timing_page==6)event_push(EVENT_MEASURE,COST_STOP,0);
        selector_used=1;timing_page=(timing_page+1u)%8u;detail_set(0);editor_redraw=1;timing_draw=0;
        if(timing_page)lcd_fill(0,0,240,240,C_BLACK);
        pressed&=~(1u<<B_REC);
    }
    if(timing_page){
        if(pressed&(1u<<B_SAVE)){
            if(timing_page==6){uint32_t s=control_request.measure[0];event_push(EVENT_MEASURE,s>=COST_ARMED&&s<=COST_MEASURE?COST_STOP:COST_START,0);cost_follow=1;cost_xruns=control_request.xruns;}
            else if(timing_page==2){rdx_bench_start();bench_started=1;}
            else if(timing_page==3){uint32_t p[9],f=plat_lock();rdx_detail_profile(p);rdx_detail_set(p[7]?DETAIL_OFF:profile_fine?DETAIL_FINE:DETAIL_COARSE);plat_unlock(f);}
            else if(timing_page==4){core_mode=core_mode?CORE_TEST_OFF:core_selection;core_done=core_errors=core_late=core_roundtrip=0;timing_reset();}
            else if(timing_page==5){timing_reset();gui_jobs=gui_errors=gui_peak=0;control_jobs=control_peak=control_midi_count=control_drops=0;}
            else timing_reset();timing_draw=0;
        }
        if(pressed&(1u<<B_HOME)){if(timing_page==6)event_push(EVENT_MEASURE,COST_STOP,0);timing_page=0;detail_set(0);editor_redraw=1;}
        // Keep keyboard/preset processing below; consume page/edit controls.
        pressed&=(1u<<B_HOME)|(1u<<B_OCTDN)|(1u<<B_OCTUP);
    }
    if(!timing_page&&store_screen==STORE_CLOSED&&(pressed&(1u<<B_ARP))){draft_screen=draft_screen==DRAFT_CLOSED?DRAFT_SELECT:DRAFT_CLOSED;looper_screen=1;pressed&=~(1u<<B_ARP);}
    if(draft_screen!=DRAFT_CLOSED){
        if(pressed&(1u<<B_HOME)){draft_screen=DRAFT_CLOSED;looper_screen=0;editor_page=EDITOR_HOME;editor_redraw=1;}
        else{
            int32_t v=plat_enc(EN_SELECT);if(v){draft_slot=wrap_selection(draft_slot,v,DRAFT_SLOTS);draft_screen=DRAFT_SELECT;}
            if(pressed&(1u<<B_SAVE)){
                if(control_request.loop[0]!=LOOP_STOP||!control_request.loop[2])draft_screen=DRAFT_ERROR;
                else if(draft_screen==DRAFT_SAVE_CONFIRM){control_reply.draft_action=DRAFT_ACTION_SAVE;control_reply.draft_slot=draft_slot;}
                else draft_screen=DRAFT_SAVE_CONFIRM;
            }
            if(pressed&(1u<<B_PLAY)){
                if(control_request.loop[0]!=LOOP_STOP){event_push(EVENT_LOOP,LOOP_TOGGLE,0);draft_screen=DRAFT_SELECT;}
                else if(!(control_request.draft_mask&(1u<<draft_slot)))draft_screen=DRAFT_ERROR;
                else if(draft_screen==DRAFT_LOAD_CONFIRM){control_reply.draft_action=DRAFT_ACTION_LOAD;control_reply.draft_slot=draft_slot;}
                else draft_screen=DRAFT_LOAD_CONFIRM;
            }
        }
        for(uint32_t i=0;i<NE;++i)(void)plat_enc(i);goto keyboard_input;
    }
    if(!timing_page&&store_screen==STORE_CLOSED){
        if(pressed&(1u<<B_SEQ)){looper_screen=!looper_screen;editor_redraw=1;}
        if(pressed&(1u<<B_REC)){looper_screen=1;event_push(EVENT_LOOP,LOOP_REC,0);pressed&=~(1u<<B_REC);editor_redraw=1;}
        if(pressed&(1u<<B_PLAY)){looper_screen=1;event_push(EVENT_LOOP,LOOP_TOGGLE,0);editor_redraw=1;}
        if(pressed&(1u<<B_HOME)){looper_screen=0;editor_redraw=1;}
        if(looper_screen){
            int32_t v=plat_enc(EN_PRESET);if(v){preset=clamp(preset+v,0,(int32_t)rdx_preset_count()-1);event_push(EVENT_PRESET,(uint8_t)preset,0);}
            v=plat_enc(EN_K1);if(v)control_push((rdx_event_t){EVENT_LOOP,LOOP_GUIDE,0,0,(int16_t)wrap_selection((uint8_t)control_request.guide,v,GUIDE_MODES_COUNT)});
            v=plat_enc(EN_K2);if(v)control_push((rdx_event_t){EVENT_LOOP,LOOP_QUANTIZE,0,0,(int16_t)wrap_selection((uint8_t)control_request.loop[7],v,LOOP_QUANT_MODES_COUNT)});
            v=plat_enc(EN_K4);if(v)control_push((rdx_event_t){EVENT_LOOP,LOOP_TEMPO,0,0,(int16_t)clamp(v,-127,127)});
            if((pressed&(1u<<B_SAVE))&&(buttons&(1u<<B_SEL))){event_push(EVENT_LOOP,LOOP_CLEAR,0);selector_used=1;pressed&=~(1u<<B_SAVE);}
            if(pressed&((1u<<B_EDIT)|(1u<<B_ENV)|(1u<<B_LFO)|(1u<<B_GLO)|(1u<<B_FX)))looper_screen=0;
        }
    }
    if(!timing_page&&(pressed&(1u<<B_SAVE))&&store_screen==STORE_CLOSED){
        memcpy(store_patch,control_request.editor.patch,150);store_trim=control_request.patch_trim;
        store_slot=control_request.editor.preset>=33u?(uint8_t)(control_request.editor.preset-33u):0;store_cursor=0;store_screen=STORE_EDIT;
        pressed&=~(1u<<B_SAVE);editor_redraw=1;
    }
    if(store_screen!=STORE_CLOSED){
        if(pressed&(1u<<B_HOME)){store_screen=STORE_CLOSED;looper_screen=0;editor_page=EDITOR_HOME;editor_redraw=1;}
        else if(store_screen==STORE_EDIT||store_screen==STORE_CONFIRM){
            int32_t v=plat_enc(EN_SELECT);if(v){store_slot=wrap_selection(store_slot,v,32);store_screen=STORE_EDIT;}
            v=plat_enc(EN_K1);if(v)store_cursor=wrap_selection(store_cursor,v,10);
            v=plat_enc(EN_K2);if(v){int c=store_patch[store_cursor];if(c<32||c>126)c=32;store_patch[store_cursor]=(uint8_t)(32+wrap_selection(c-32,v,95));store_screen=STORE_EDIT;}
            v=plat_enc(EN_K3);if(v){store_slot=(uint8_t)(wrap_selection(store_slot/8,v,4)*8+store_slot%8);store_screen=STORE_EDIT;}
            v=plat_enc(EN_K4);if(v){store_slot=(uint8_t)(store_slot/8*8+wrap_selection(store_slot%8,v,8));store_screen=STORE_EDIT;}
            if(pressed&(1u<<B_SAVE)){
                if(store_screen==STORE_EDIT&&(control_request.user_mask&(1u<<store_slot)))store_screen=STORE_CONFIRM;
                else if(control_request.loop[0]!=LOOP_STOP){store_screen=STORE_ERROR;}
                else{control_reply.save_pending=1;control_reply.save_slot=store_slot;control_reply.save_trim=store_trim;memcpy(control_reply.save_patch,store_patch,150);}
            }
        }
        // Always consume encoders while modal, but still deliver keyboard offs.
        for(uint32_t i=0;i<NE;++i)(void)plat_enc(i);
        goto keyboard_input;
    }
    if(looper_screen&&!timing_page){for(uint32_t i=0;i<NE;++i)(void)plat_enc(i);goto keyboard_input;}
    if((pressed|released)&(1u<<B_SEL))reset_knob_motion();
    if(pressed&(1u<<B_SEL))selector_used=0;
    if((released&(1u<<B_SEL))&&!selector_used)editor_op=(uint8_t)((editor_op+1u)%4u);
    if(pressed&(1u<<B_HOME))editor_page=EDITOR_HOME;
    if(pressed&(1u<<B_EDIT)) {
        static const uint8_t pages[]={EDITOR_LEVEL,EDITOR_FEEDBACK,EDITOR_FREQ,EDITOR_ALGO,EDITOR_OP1,EDITOR_OP2,EDITOR_OP3};
        uint32_t at=0;for(;at<7u;++at)if(pages[at]==editor_page)break;
        editor_page=pages[at<7u?(at+1u)%7u:0u];
    }
    if(pressed&(1u<<B_ENV))editor_page=editor_page>=EDITOR_AEG_RATES&&editor_page<EDITOR_PEG_ON?(uint8_t)(editor_page+1u):EDITOR_AEG_RATES;
    if(pressed&(1u<<B_LFO))editor_page=editor_page>=EDITOR_LFO1&&editor_page<EDITOR_LFO3?(uint8_t)(editor_page+1u):EDITOR_LFO1;
    if(pressed&(1u<<B_GLO)){if(buttons&(1u<<B_SEL)){editor_page=EDITOR_LAB;selector_used=1;}else editor_page=editor_page==EDITOR_VOICE?EDITOR_MIDI:EDITOR_VOICE;}
    if(pressed&(1u<<B_FX)) {
        if(buttons&(1u<<B_SEL)){selector_used=1;event_push(EVENT_FX,0,0);}
        else editor_page=editor_page==EDITOR_FX1?EDITOR_FX2:EDITOR_FX1;
    }

    int32_t move=plat_enc(EN_SELECT);
    if(move&&timing_page==6){cost_selection=wrap_selection(cost_follow?control_request.measure[1]:cost_selection,clamp(move,-127,127),COST_ROWS);cost_follow=0;timing_draw=0;}
    if(move&&timing_page==5){gui_enabled=!gui_enabled;editor_redraw=1;timing_draw=0;}
    if(move&&timing_page==7){event_push(EVENT_SPLIT,!control_request.dry[0],0);editor_redraw=1;timing_draw=0;}
    if(move&&timing_page==4){core_selection=core_selection==CORE_TEST_IDLE?CORE_TEST_BUSY:CORE_TEST_IDLE;timing_draw=0;}
    if(move&&timing_page==3){
        profile_fine=wrap_selection(profile_fine,clamp(move,-127,127),2);uint32_t p[9],f=plat_lock();rdx_detail_profile(p);
        rdx_detail_set(p[7]?(profile_fine?DETAIL_FINE:DETAIL_COARSE):DETAIL_OFF);plat_unlock(f);timing_draw=0;
    }
    if(move&&timing_page==2){bench_selection=wrap_selection(bench_selection,clamp(move,-127,127),BENCH_CASES_COUNT);timing_draw=0;}
    if(move&&!timing_page){
        if(buttons&(1u<<B_SEL))selector_used=1;
        select_in_menu(clamp(move,-127,127));
    }
    move=plat_enc(EN_ALGO);if(move&&!timing_page){editor_page=EDITOR_ALGO;editor_event(EDITOR_ALGO,0,0,move,0);}
    if(previous_page!=editor_page||previous_op!=editor_op)reset_knob_motion();
    const uint32_t now=plat_ms();
    for(uint8_t k=0;k<4;++k) {
        move=plat_enc(EN_K1+k);if(!move)continue;
        if(timing_page){if(timing_page==7&&k==0){dry_view=!dry_view;timing_draw=0;}continue;}
        move=clamp(move,-127,127);
        if(editor_page==EDITOR_HOME)editor_page=EDITOR_LEVEL;
        uint8_t fine=(buttons&(1u<<B_SEL))!=0;if(fine)selector_used=1;
        if(editor_page==EDITOR_LAB&&k==0){editor_tune=(uint8_t)clamp((int32_t)editor_tune+move,0,4);reset_knob_motion();continue;}
        int accelerate=continuous_parameter(editor_page,k)&&!fine;
#ifdef PLAT_HOST
        if(control_request.exact)accelerate=0; // host "spin" explicitly requests exact steps
#endif
        move=rdx_encoder_delta(&knob_motion[k],move,now,accelerate);
        editor_event(editor_page,editor_page==EDITOR_LAB?editor_tune:editor_op,k,move,fine);
    }
    const int32_t step=plat_enc(EN_PRESET);
    if(step) {
        preset_home();
        uint32_t f=plat_lock();
        preset=clamp(preset+step,0,(int32_t)rdx_preset_count()-1);
        event_push(EVENT_PRESET,(uint8_t)preset,0);
        plat_unlock(f);
    }
keyboard_input:
    if(pressed&((1u<<B_OCTDN)|(1u<<B_OCTUP)))octave=clamp(octave+((pressed&(1u<<B_OCTUP))?1:-1),-2,2);
    for(k=0;k<NKEYS;++k) {
        uint32_t bit=1u<<k;uint8_t note=(uint8_t)(53+(int32_t)k+12*octave);
        if((keys&bit) && !(keys_before&bit)){key_notes[k]=note;event_push(EVENT_ON,note,100);}
        else if(!(keys&bit) && (keys_before&bit))event_push(EVENT_OFF,key_notes[k],0);
    }
    keys_before=keys;buttons_before=buttons;
}
static void pixel_line(uint32_t y,const char* text,uint32_t length,uint32_t numerator){
    if(y>=240u)return;
    const uint32_t height=7u*numerator/8u,width=length*6u*numerator/8u;
    for(uint32_t band=0;band<height&&y+band<240u;band+=8u){uint32_t h=height-band;if(h>8u)h=8u;if(h>240u-y-band)h=240u-y-band;
        cv_begin(240,h,C_BLACK);pixel_text((240-(int32_t)width)/2,-(int32_t)band,text,length,numerator);cv_blit(0,y+band);
    }
}
static void draw_text_row(uint32_t y,uint8_t style,const char* text,uint16_t color){
    (void)color;uint32_t length=pixel_length(text);
    if(y==4u){
        // Compose the background and glyphs before submitting each strip. A separate
        // erase pass makes the title visibly disappear during cooperative LCD waits.
        const int32_t x=(240-(int32_t)(length*6u*15u/8u))/2;
        for(uint32_t band=0;band<24u;band+=8u){cv_begin(240,8,C_BLACK);pixel_text(x,4-(int32_t)band,text,length,15u);cv_blit(0,band);}
        cv_begin(240,1,C_WHITE);cv_blit(0,28);return;}
    if(style==PIXEL_LARGE&&length<=10u){pixel_line(y,text,length,30u);return;}
    // Diagnostic rows may use two lines. User-facing copy stays within 20 cells.
    if(length>20u){pixel_line(y,text,20u,15u);pixel_line(y+13u,text+20u,length-20u,15u);}
    else pixel_line(y,text,length,15u);
}
// Button hints occupy a dedicated area; brackets distinguish physical keys
// from their actions. Knob assignments belong beside their editable values.
static void button_hints(const char* a,const char* b,const char* c,const char* d){
    const char* rows[4]={a,b,c,d};
    for(uint32_t band=0;band<61u;band+=8u){uint32_t h=61u-band;if(h>8u)h=8u;cv_begin(240,h,C_BLACK);
        for(uint32_t row=0;row<4u;++row)if(rows[row]){uint32_t n=pixel_length(rows[row]);if(n>20u)n=20u;
            pixel_text(6,(int32_t)(row*16u)-(int32_t)band,rows[row],n,15u);
        }cv_blit(0,179u+band);
    }
}
// Bounded UI-only formatter; target has no snprintf.
static void timing_format(char* out,const char* fmt,uint32_t a,uint32_t b,const char* text){
    uint32_t n=0,arg=0;
    while(*fmt&&n<39u){
        if(fmt[0]=='%'&&fmt[1]=='l'&&fmt[2]=='u'){
            char digits[10];uint32_t count=0,v=arg++?b:a;
            do{digits[count++]=(char)('0'+v%10u);v/=10u;}while(v);
            while(count&&n<39u)out[n++]=digits[--count];fmt+=3;
        }else if(fmt[0]=='%'&&fmt[1]=='s'){
            while(*text&&n<39u)out[n++]=*text++;fmt+=2;
        }else if(fmt[0]=='%'&&fmt[1]=='%'){out[n++]='%';fmt+=2;}
        else out[n++]=*fmt++;
    }out[n]=0;
}
static void control_frame(void) {
    audio_checkpoint();
    control_input();
    static uint8_t last_overlay=OVERLAYS_COUNT;static uint32_t store_draw_hash,draft_draw_hash,loop_draw_time,loop_draw_state=255;static uint8_t draft_draw_valid;
    uint8_t overlay=draft_screen!=DRAFT_CLOSED?OVERLAY_DRAFT:store_screen!=STORE_CLOSED?OVERLAY_STORE:looper_screen&&!timing_page?OVERLAY_LOOP:OVERLAY_NONE;
    if(overlay!=last_overlay){lcd_fill(0,0,240,240,C_BLACK);last_overlay=overlay;store_draw_hash=0;draft_draw_valid=0;loop_draw_time=0;editor_redraw=1;gui_ready=0;}
    if(draft_screen!=DRAFT_CLOSED){
        uint32_t hash=gui_hash(control_request.draft_names[draft_slot],11)^((uint32_t)draft_screen<<24)^((uint32_t)draft_slot<<16)^control_request.draft_mask^control_request.loop[0];
        if(!editor_redraw&&draft_draw_valid&&hash==draft_draw_hash)return;draft_draw_valid=1;draft_draw_hash=hash;
        char row[40];
        draw_text_row(4,PIXEL_LARGE,"DRAFTS",C_WHITE);
        timing_format(row,"%lu / 4",draft_slot+1u,0,"");draw_text_row(46,PIXEL_LARGE,row,C_WHITE);
        draw_text_row(82,PIXEL_LARGE,control_request.draft_mask&(1u<<draft_slot)?control_request.draft_names[draft_slot]:"EMPTY",C_WHITE);
        const char* status=draft_screen==DRAFT_SAVE_CONFIRM?"REPLACE THIS DRAFT?":draft_screen==DRAFT_LOAD_CONFIRM?"REPLACE THIS LOOP?":draft_screen==DRAFT_SAVED?"DRAFT SAVED":draft_screen==DRAFT_LOADED?"DRAFT LOADED":draft_screen==DRAFT_ERROR?(control_request.loop[0]!=LOOP_STOP?"STOP LOOP FIRST":"EMPTY / READ ERROR"):"SELECT: CHOOSE SLOT";
        draw_text_row(126,PIXEL_BODY,status,C_HI);
        draw_text_row(146,PIXEL_BODY,"SELECT: SLOT",C_GRAY);
        draw_text_row(164,PIXEL_BODY,"PRESS AGAIN: CONFIRM",C_WHITE);
        button_hints(draft_screen==DRAFT_SAVE_CONFIRM?"[SAVE] CONFIRM":"[SAVE] STORE",draft_screen==DRAFT_LOAD_CONFIRM?"[PLAY] CONFIRM":"[PLAY] LOAD","[ARP] LOOPER","[HOME] EXIT");editor_redraw=0;return;
    }
    if(store_screen!=STORE_CLOSED){
        uint32_t hash=gui_hash(store_patch,150)^((uint32_t)store_screen<<24)^((uint32_t)store_slot<<16)^((uint32_t)store_cursor<<8)^control_request.loop[0];
        if(!editor_redraw&&store_draw_hash==hash)return;
        store_draw_hash=hash;
        char row[40],name[11];memcpy(name,store_patch,10);name[10]=0;
        for(uint32_t i=0;i<10u;++i)if(name[i]<32||name[i]>126)name[i]=' ';
        draw_text_row(4,PIXEL_LARGE,"STORE",C_WHITE);
        draw_text_row(46,PIXEL_LARGE,name,C_WHITE);
        control_fill(7u+store_cursor*180u/8u,74u,18u,2u,C_WHITE);
        draw_text_row(84,PIXEL_BODY,"K1 POS  K2 CHAR",C_WHITE);
        timing_format(row,"USER %lu-%lu",store_slot/8+1,store_slot%8+1,"");draw_text_row(108,PIXEL_LARGE,row,C_WHITE);
        draw_text_row(138,PIXEL_BODY,"K3 BANK  K4 SLOT",C_WHITE);
        draw_text_row(153,PIXEL_BODY,control_request.user_names[store_slot],C_GRAY);
        draw_text_row(166,PIXEL_BODY,store_screen==STORE_CONFIRM?"REPLACE? SAVE YES":store_screen==STORE_DONE?"SAVED":store_screen==STORE_ERROR?control_request.loop[0]!=LOOP_STOP?"STOP LOOP FIRST":"SAVE ERROR / HOME":"SELECT DESTINATION",C_HI);
        if(store_screen==STORE_DONE||store_screen==STORE_ERROR)button_hints("[HOME] EXIT",0,0,0);
        else button_hints(store_screen==STORE_CONFIRM?"[SAVE] REPLACE":"[SAVE] STORE","[HOME] CANCEL",0,0);editor_redraw=0;return;
    }
    if(looper_screen&&!timing_page){
        uint32_t now=plat_ms();if(!editor_redraw&&loop_draw_time&&now-loop_draw_time<100u&&loop_draw_state==control_request.loop[0])return;
        loop_draw_time=now;loop_draw_state=control_request.loop[0];
        char row[40];const uint32_t* l=control_request.loop;
        static const char* states[LOOP_STATES_COUNT]={"STOPPED","REC STANDBY","RECORDING","PLAYING","OVERDUB ARMED","OVERDUBBING"};
        if(editor_redraw)draw_text_row(4,PIXEL_LARGE,"LOOPER",C_WHITE);
        draw_text_row(46,PIXEL_BODY,states[l[0]<LOOP_STATES_COUNT?l[0]:0],C_HI);
        timing_format(row,l[6]?"TAKE FULL":!l[2]?(l[0]==LOOP_ARMED?"PLAY FIRST NOTE":l[0]==LOOP_STOP?"REC TO BEGIN":"BEAT %lu"):"BEAT %lu / %lu",l[1]/96u+1u,(l[2]+95u)/96u,"");draw_text_row(64,PIXEL_BODY,row,C_WHITE);
        cv_begin(240,8,C_BLACK);for(uint32_t x=6;x<234u;++x){cv_pset(x,0,C_WHITE);cv_pset(x,7,C_WHITE);}for(uint32_t yy=0;yy<8u;++yy){cv_pset(6,yy,C_WHITE);cv_pset(233,yy,C_WHITE);}
        uint32_t progress=l[2]?l[1]*224u/l[2]:0;if(progress>224u)progress=224u;for(uint32_t xx=0;xx<progress;++xx)for(uint32_t yy=2;yy<6u;++yy)cv_pset(8u+xx,yy,C_WHITE);cv_blit(0,32);
        timing_format(row,"K4 TEMPO %lu BPM",l[3],0,"");draw_text_row(84,PIXEL_BODY,row,C_WHITE);
        static const char* guides[GUIDE_MODES_COUNT]={"OFF","AUTO","ON"};
        timing_format(row,"K1 CLICK %s",0,0,guides[control_request.guide<GUIDE_MODES_COUNT?control_request.guide:GUIDE_AUTO]);draw_text_row(104,PIXEL_BODY,row,C_WHITE);
        static const char* quant_labels[LOOP_QUANT_MODES_COUNT]={"OFF","1/4","1/4T","1/8","1/8T","1/16","1/16T","1/32","1/32T"};
        timing_format(row,"K2 QUANT %s",0,0,quant_labels[l[7]<LOOP_QUANT_MODES_COUNT?l[7]:LOOP_QUANT_OFF]);draw_text_row(124,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"LOOP %s",0,0,control_request.phrase_name);draw_text_row(144,PIXEL_BODY,row,C_WHITE);
        char live[11];memcpy(live,control_request.editor.patch,10);live[10]=0;
        timing_format(row,"LIVE %s",0,0,live);draw_text_row(162,PIXEL_BODY,row,C_WHITE);
        const char* rec=l[0]==LOOP_RECORD||l[0]==LOOP_OVERDUB?"[REC] FINISH":l[0]==LOOP_ARMED||l[0]==LOOP_OVERDUB_ARMED?"[REC] CANCEL":l[2]?"[REC] OVERDUB":"[REC] RECORD";
        if(l[4]>=LOOP_TRACKS&&l[0]==LOOP_STOP)rec="[SEL+SAVE] CLEAR";
        else if(l[4]>=LOOP_TRACKS&&l[0]==LOOP_PLAY)rec="[SEQ] CLOSE";
        button_hints(rec,l[0]==LOOP_STOP?l[2]?"[PLAY] START":"[SEQ] CLOSE":"[PLAY] STOP","[ARP] DRAFTS","[HOME] EDIT");editor_redraw=0;return;
    }
    if(timing_page==6){
        uint32_t now=plat_ms();if(timing_draw&&now-timing_draw<250u)return;timing_draw=now;
        const uint32_t* m=control_request.measure;uint32_t selected=cost_follow?m[1]:cost_selection;if(selected>=COST_ROWS)selected=0;
        const uint32_t* row=m+COST_HEADER_WORDS+selected*COST_ROW_WORDS;
        static const char* states[COST_STATES_COUNT]={"IDLE","WAIT","SETTLE","MEASURE","DONE","STOP","LIMIT","VOICE LOSS","POLY ONLY","CHANGED"};char text[40];
        draw_text_row(4,PIXEL_LARGE,control_request.dry[0]?"CPU1 VOICE SWEEP":"VOICE COST SWEEP",C_WHITE);
        timing_format(text,"TEST %luV %s",m[1],0,states[m[0]<COST_STATES_COUNT?m[0]:COST_IDLE]);draw_text_row(46,PIXEL_BODY,text,C_WHITE);
        timing_format(text,"ROW %luV ACT %lu",selected,row[COST_ACTUAL],"");draw_text_row(72,PIXEL_BODY,text,C_WHITE);
        timing_format(text,control_request.dry[0]?"END %lu WORK %lu us":"APP %lu SYN %lu us",(row[COST_APP]+23u)/24u,(row[COST_SYN]+23u)/24u,"");draw_text_row(98,PIXEL_BODY,text,C_WHITE);
        if(control_request.dry[0])timing_format(text,"Q %lu TURN %lu us",(row[COST_QUEUE]+23u)/24u,(row[COST_TURN]+23u)/24u,"");
        else timing_format(text,"FX %lu P6 %lu us",(row[COST_FX]+23u)/24u,(m[3]+23u)/24u,"");
        draw_text_row(124,PIXEL_BODY,text,C_WHITE);
        timing_format(text,"STEADY %lu XRUN %lu",row[COST_STEADY_BLOCKS],plat_xruns()-cost_xruns,"");draw_text_row(150,PIXEL_BODY,text,C_WHITE);
#ifdef PLAT_HOST
        draw_text_row(174,PIXEL_BODY,"HOST: NO DEVICE TIMING",C_GRAY);
#else
        timing_format(text,"90%% GUARD %lu us",(m[4]+23u)/24u,0,"");draw_text_row(174,PIXEL_BODY,text,C_GRAY);
#endif
        draw_text_row(198,PIXEL_BODY,"SAVE START/STOP HOME",C_HI);
        timing_format(text,"SELECT ROW / %lu s",m[2]/1000u,0,"");draw_text_row(216,PIXEL_BODY,text,C_GRAY);return;
    }
    if(timing_page==7){
        uint32_t now=plat_ms();char row[40];
        if((uint32_t)(now-timing_draw)<250u&&timing_draw)return;
        timing_draw=now;
        const uint32_t* d=control_request.dry;const uint32_t* q=control_request.dry_detail;
        static const char* at[DRY_CHECKPOINTS_COUNT]={"IDLE","CONTROL","LCD","TEXT","SCALE","EDITOR","PROTO","WAIT","CLEAR","SUBMIT"};
        if(dry_view){
            const uint32_t* u=control_request.ui;
            draw_text_row(4,PIXEL_LARGE,"GUI / LCD PROFILE",C_WHITE);
            timing_format(row,"WAIT %lu CLEAR %lu us",(u[UI_DMA_WAIT]+23u)/24u,(u[UI_CLEAR]+23u)/24u,"");draw_text_row(46,PIXEL_BODY,row,C_WHITE);
            timing_format(row,"TEXT %lu SUBMIT %lu us",(u[UI_TEXT]+23u)/24u,(u[UI_SUBMIT]+23u)/24u,"");draw_text_row(72,PIXEL_BODY,row,C_WHITE);
            timing_format(row,"EDITOR %lu us / CPU%lu",(u[UI_EDITOR]+23u)/24u,plat_core(),"");draw_text_row(98,PIXEL_BODY,row,C_WHITE);
            timing_format(row,"QMAX %lu AT %s",(q[0]+23u)/24u,0,at[q[3]<DRY_CHECKPOINTS_COUNT?q[3]:DRY_AT_IDLE]);draw_text_row(124,PIXEL_BODY,row,C_WHITE);
            timing_format(row,"WORK %lu TURN %lu",(d[2]+23u)/24u,(d[3]+23u)/24u,"");draw_text_row(150,PIXEL_BODY,row,C_WHITE);
            timing_format(row,"LATE %lu XRUN %lu",d[4],plat_xruns()-timing_xruns,"");draw_text_row(174,PIXEL_BODY,row,C_WHITE);
            timing_format(row,"VOICE %lu/%lu",(control_request.voices>>8)&255u,control_request.voices&255u,"");draw_text_row(198,PIXEL_BODY,row,C_HI);
            timing_format(row,"LCD ERR %lu / K1: QUEUE",control_request.lcd_errors,0,"");draw_text_row(216,PIXEL_BODY,row,C_GRAY);return;
        }
        draw_text_row(4,PIXEL_LARGE,"DRY CORE PIPELINE",C_WHITE);
        timing_format(row,"WORK %lu TURN %lu",(d[2]+23u)/24u,(d[3]+23u)/24u,"");draw_text_row(46,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"QMAX %lu WORK %lu",(q[0]+23u)/24u,(q[1]+23u)/24u,"");draw_text_row(72,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"AT %s / %luV WHY %lu",q[4],q[7],at[q[3]<DRY_CHECKPOINTS_COUNT?q[3]:DRY_AT_IDLE]);draw_text_row(98,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"APP %lu FX %lu us",(timing.peak+23u)/24u,(control_request.cost[1]+23u)/24u,"");draw_text_row(124,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"VOICE %lu/%lu",(control_request.voices>>8)&255u,control_request.voices&255u,"");draw_text_row(150,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"LATE %lu FAULT %lu",d[4],plat_cpu2_gave_up(),"");draw_text_row(174,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"XRUN %lu / SYN CPU%lu",plat_xruns()-timing_xruns,d[0]?1u:0u,"");draw_text_row(198,PIXEL_BODY,row,C_HI);
        draw_text_row(216,PIXEL_BODY,"K1: LCD / SAVE RESET",C_GRAY);return;
    }
    if(timing_page==5){
        uint32_t now=plat_ms();if(timing_draw&&now-timing_draw<250u)return;timing_draw=now;
        uint32_t f=plat_lock(),peak=timing.peak,cost[3];rdx_cost_profile(cost);plat_unlock(f);char row[40];
        draw_text_row(4,PIXEL_LARGE,"AUDIO ISOLATION",C_WHITE);
        timing_format(row,"CTL CPU%lu JOBS %lu",control_last_core,control_jobs,"");draw_text_row(46,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"ERR %lu FAULT %lu",control_errors,plat_cpu2_gave_up(),"");draw_text_row(72,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"CTL %lu us STK %lu",(control_peak+23u)/24u,plat_cpu2_stack(),"");draw_text_row(98,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"APP %lu SYN %lu us",(peak+23u)/24u,(cost[0]+23u)/24u,"");draw_text_row(124,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"XRUN %lu / %lu s",plat_xruns()-timing_xruns,(now-timing_started)/1000u,"");draw_text_row(150,PIXEL_BODY,row,C_WHITE);
        draw_text_row(174,PIXEL_BODY,control_failed?"FAILED / CPU0 CONTROL":gui_enabled?"SELECT: CPU1 CONTROL":"SELECT: CPU0 CONTROL",C_WHITE);
        draw_text_row(198,PIXEL_BODY,"SAVE RESET / HOME TEST",C_HI);
        timing_format(row,"MIDI %lu DROP %lu",control_midi_count,control_drops,"");draw_text_row(216,PIXEL_BODY,row,C_GRAY);return;
    }
    if(timing_page==4){
        uint32_t now=plat_ms();if(timing_draw&&now-timing_draw<250u)return;timing_draw=now;
        uint32_t f=plat_lock(),peak=timing.peak,over=timing.over90,cost[3];rdx_cost_profile(cost);plat_unlock(f);char row[40];
        draw_text_row(4,PIXEL_LARGE,"CONTROL CORE TEST",C_WHITE);
        timing_format(row,"CORE %lu FAULT %lu",plat_cpu2_on(),plat_cpu2_gave_up(),"");draw_text_row(46,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"OK %lu ERR %lu",core_done,core_errors,"");draw_text_row(72,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"ROUND %lu ms LATE %lu",core_roundtrip,core_late,"");draw_text_row(98,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"APP %lu SYN %lu us",(peak+23u)/24u,(cost[0]+23u)/24u,"");draw_text_row(124,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"XRUN %lu >90%% %lu",plat_xruns()-timing_xruns,over,"");draw_text_row(150,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"%s / %lu s",(now-timing_started)/1000u,0,core_mode==CORE_TEST_BUSY?"BUSY":core_mode==CORE_TEST_IDLE?"IDLE":"OFF");draw_text_row(174,PIXEL_BODY,row,C_WHITE);
        draw_text_row(198,PIXEL_BODY,core_selection==CORE_TEST_BUSY?"SELECT: BUSY / SAVE RUN":"SELECT: IDLE / SAVE RUN",C_HI);
#ifdef PLAT_HOST
        draw_text_row(216,PIXEL_BODY,"HOST: SIMULATED CORE",C_GRAY);
#else
        draw_text_row(216,PIXEL_BODY,"SAVE STOP/RESET / HOME",C_GRAY);
#endif
        return;
    }
    if(timing_page==3){
        uint32_t now=plat_ms();if(timing_draw&&now-timing_draw<500u)return;timing_draw=now;
        uint32_t p[9],fine[4],f=plat_lock();rdx_detail_profile(p);rdx_detail_fine(fine);plat_unlock(f);char row[40];
        if(profile_fine){
            draw_text_row(4,PIXEL_LARGE,"FINE RENDER",C_WHITE);
            timing_format(row,"ENV STEP %lu us",(fine[0]+23u)/24u,0,"");draw_text_row(46,PIXEL_BODY,row,C_WHITE);
            timing_format(row,"ENV GAIN %lu us",(fine[1]+23u)/24u,0,"");draw_text_row(72,PIXEL_BODY,row,C_WHITE);
            timing_format(row,"PITCH %lu  WAVE %lu us",(fine[2]+23u)/24u,(fine[3]+23u)/24u,"");draw_text_row(98,PIXEL_BODY,row,C_WHITE);
            timing_format(row,"MOD %lu  MIX %lu us",(p[1]+23u)/24u,(p[4]+23u)/24u,"");draw_text_row(124,PIXEL_BODY,row,C_WHITE);
            timing_format(row,"TOTAL %lu us VOICES %lu",(p[5]+23u)/24u,p[8],"");draw_text_row(150,PIXEL_BODY,row,C_WHITE);
            draw_text_row(174,PIXEL_BODY,p[7]?"ON / SAVE: STOP":"OFF / SAVE: START",C_HI);
            draw_text_row(198,PIXEL_BODY,"SELECT: COARSE / HOME",C_WHITE);
#ifdef PLAT_HOST
            draw_text_row(216,PIXEL_BODY,"HOST: NO TIMING",C_GRAY);
#else
            draw_text_row(216,PIXEL_BODY,"WORST TOTAL BLOCK",C_GRAY);
#endif
            return;
        }
        draw_text_row(4,PIXEL_LARGE,"RENDER PROFILE",C_WHITE);
        timing_format(row,"PREP %lu  MOD %lu us",(p[0]+23u)/24u,(p[1]+23u)/24u,"");draw_text_row(46,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"ENV %lu  OPS %lu us",(p[2]+23u)/24u,(p[3]+23u)/24u,"");draw_text_row(72,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"MIX %lu  TOTAL %lu us",(p[4]+23u)/24u,(p[5]+23u)/24u,"");draw_text_row(98,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"VOICES %lu BLOCKS %lu",p[8],p[6],"");draw_text_row(124,PIXEL_BODY,row,C_WHITE);
        draw_text_row(150,PIXEL_BODY,p[7]?"ON / SAVE: STOP":"OFF / SAVE: START",C_HI);
        draw_text_row(174,PIXEL_BODY,"WORST TOTAL BLOCK",C_GRAY);
        draw_text_row(198,PIXEL_BODY,"SELECT: FINE / HOME",C_WHITE);
#ifdef PLAT_HOST
        draw_text_row(216,PIXEL_BODY,"HOST: NO TIMING",C_GRAY);
#else
        draw_text_row(216,PIXEL_BODY,"SPLIT PATH / IRQ ON",C_GRAY);
#endif
        return;
    }
    if(timing_page==2){
        rdx_bench_tick();
        uint32_t now=plat_ms();if(timing_draw&&now-timing_draw<200u)return;timing_draw=now;
        uint32_t result[4];rdx_bench_result(bench_selection,result);char row[40];
        draw_text_row(4,PIXEL_LARGE,"DSP BENCHMARK",C_WHITE);
        draw_text_row(46,PIXEL_BODY,rdx_bench_name(bench_selection),C_WHITE);
        timing_format(row,"MIN %lu MAX %lu ticks",result[0],result[1],"");draw_text_row(72,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"MIN %lu ns / op",result[0]*1000u/(24u*256u),0,"");draw_text_row(98,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"CASE %lu/15 REP %lu",result[2],result[3],"");draw_text_row(124,PIXEL_BODY,row,C_WHITE);
        draw_text_row(150,PIXEL_BODY,!bench_started?"SAVE: START":result[2]>=BENCH_CASES_COUNT?"DONE / SAVE: RERUN":"RUNNING / SAVE: RERUN",C_HI);
        draw_text_row(174,PIXEL_BODY,"SELECT: RESULT",C_WHITE);
        draw_text_row(198,PIXEL_BODY,"256 OPS / IRQ ON",C_GRAY);
#ifdef PLAT_HOST
        draw_text_row(216,PIXEL_BODY,"HOST: NO TIMING",C_GRAY);
#else
        draw_text_row(216,PIXEL_BODY,"24MHz TICKS / HOME EXIT",C_GRAY);
#endif
        return;
    }
    if(timing_page){
        uint32_t now=plat_ms();if(timing_draw&&now-timing_draw<500u)return;timing_draw=now;
        uint32_t f=plat_lock();
        uint32_t peak=timing.peak,over=timing.over90,cost[3];rdx_cost_profile(cost);
        plat_unlock(f);
        char row[40];
        draw_text_row(4,PIXEL_LARGE,"AUDIO TIMING",C_WHITE);
        timing_format(row,"APP MAX %lu us",(peak+23u)/24u,0,"");draw_text_row(46,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"SYN %lu  FX %lu us",(cost[0]+23u)/24u,(cost[1]+23u)/24u,"");draw_text_row(72,PIXEL_BODY,row,C_WHITE);
        timing_format(row,control_request.dry[0]?"7V %lu us CPU %lu%%":"6V %lu us CPU %lu%%",(cost[2]+23u)/24u,plat_cpu_pct(),"");draw_text_row(98,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"XRUN %lu  >90%% %lu",plat_xruns()-timing_xruns,over,"");draw_text_row(124,PIXEL_BODY,row,C_WHITE);
        timing_format(row,"%lu s  P%lu  FX %s",(now-timing_started)/1000u,(uint32_t)preset,rdx_fx_status()?"ON":"OFF");draw_text_row(150,PIXEL_BODY,row,C_WHITE);
        f=plat_lock();uint32_t voices=rdx_voice_status();plat_unlock(f);
        timing_format(row,control_request.dry[0]?"VOICES %lu/%lu  MAX 7":"VOICES %lu/%lu  MAX 6",(voices>>8)&255u,voices&255u,"");draw_text_row(174,PIXEL_BODY,row,C_WHITE);
        draw_text_row(198,PIXEL_BODY,"SAVE RESET / HOME EXIT",C_HI);
        uint32_t khz=plat_cpu_khz();
        timing_format(row,khz?"%s CLK %lu MHz":"%s CLK ?",khz/1000u,0,rdx_build_label());draw_text_row(216,PIXEL_BODY,row,C_GRAY);return;
    }
    uint32_t lock=plat_lock();rdx_editor_snapshot(&editor_view);plat_unlock(lock);
    static uint32_t last_revision=0,last_fx=99;static uint8_t last_page=255,last_op=255,last_tune=255;
    if(!editor_redraw&&last_revision==editor_view.revision&&last_fx==editor_view.fx&&last_page==editor_page&&last_op==editor_op&&last_tune==editor_tune)return;
    if(gui_ready&&(gui_request.page!=editor_page||gui_request.op!=editor_op||gui_request.tune!=editor_tune))gui_ready=0;
    if(!gui_ready){
        gui_start(&editor_view,editor_page,editor_op,editor_tune);
        if(!gui_ready)return;
    }
    // A completed immutable snapshot may be one frame old. Display it, then
    // revision comparison schedules the newer state on the next frame.
    editor_view=gui_request.view;gui_ready=0;
    editor_redraw=0;
    last_revision=editor_view.revision;last_fx=editor_view.fx;last_page=editor_page;last_op=editor_op;last_tune=editor_tune;
    // Each strip below includes its background. Never blank the whole LCD
    // before drawing: the physical panel exposes that intermediate black frame.
    char context[11];memcpy(context,editor_view.patch,10);context[10]=0;
    draw_text_row(4,PIXEL_BODY,editor_page==EDITOR_HOME?"VOICE":context,C_WHITE);
    const uint8_t* pixels=gui_reply.pixels;
    for(uint32_t y=0;y<120;y+=8) {
        cv_begin(240,8,C_BLACK);
        for(uint32_t row=0;row<8;++row)for(uint32_t x=0;x<240;++x) {
            uint32_t at=(y+row)*240u+x;
            if((pixels[at>>3]>>(at&7u))&1u)cv_pset(x,row,C_WHITE);
        }
        cv_blit(0,44u+y);
    }
    cv_begin(240,8,C_BLACK);cv_blit(0,174);
    if(editor_view.error){button_hints("FX MEMORY LIMIT","[FX] EDIT EFFECTS","[HOME] HOME",0);}
    else if(editor_page==EDITOR_HOME){button_hints("[EDIT] EDIT VOICE","[ENV] ENVELOPES","[SAVE] STORE","[REC] RECORD");}
    else{
        // SEL selects an operator only where the page actually has operators.
        const char* first=((editor_page>=EDITOR_OP1&&editor_page<=EDITOR_OP3)||editor_page==EDITOR_AEG_RATES||editor_page==EDITOR_AEG_LEVELS)?"[SEL] NEXT OP":
            editor_page==EDITOR_FX1||editor_page==EDITOR_FX2?"[FX] NEXT SLOT":
            editor_page>=EDITOR_LFO1&&editor_page<=EDITOR_LFO3?"[LFO] NEXT PAGE":
            editor_page>=EDITOR_PEG_RATES&&editor_page<=EDITOR_PEG_ON?"[ENV] NEXT PAGE":
            editor_page==EDITOR_VOICE||editor_page==EDITOR_MIDI?"[GLO] NEXT PAGE":"[EDIT] NEXT PAGE";
        button_hints(first,"[SAVE] STORE",editor_view.fx?"[SEL+FX] FX OFF":"[SEL+FX] FX ON","[HOME] HOME");
    }
}
#undef plat_lock
#undef plat_unlock
#undef plat_keys
#undef plat_buttons
#undef plat_enc
#undef rdx_editor_snapshot
#undef rdx_cost_profile
#undef rdx_detail_profile
#undef rdx_detail_fine
#undef rdx_detail_set
#undef rdx_voice_status
#undef rdx_fx_status
#undef plat_xruns
#undef plat_cpu_pct
#undef plat_cpu_khz
#undef timing
#undef cv_blit
#undef cv_begin
#undef lcd_fill
void app_say(const char* a,const char* b) {(void)a;(void)b;}
void app_ui_init(void) {gui_collect(1);gui_ready=0;editor_redraw=1;rdx_editor_boot();lcd_fill(0,0,240,240,C_BLACK);}

static void app_lights(void){
    uint32_t buttons=0,keys=control_request.keys,dim=(1u<<B_REC)|(1u<<B_SEQ),f=plat_lock();
    uint32_t state=looper.state,phase=guide_phase,length=looper.length;uint8_t pulse=state!=LOOP_STOP&&phase<(uint32_t)looper.tempo*4410u;
    for(uint32_t k=0;k<NKEYS;++k){uint32_t n=53u+k+(uint32_t)(12*octave);if(n<128u&&(live_notes[n]||loop_notes[n]))keys|=1u<<k;}
    plat_unlock(f);
    if(state==LOOP_RECORD||state==LOOP_OVERDUB)buttons|=1u<<B_REC;
    else if((state==LOOP_ARMED||state==LOOP_OVERDUB_ARMED)&&pulse)buttons|=1u<<B_REC;
    if(state>=LOOP_RECORD){dim|=LED_PLAY_GREEN;if(pulse)buttons|=LED_PLAY_GREEN;}
    else if(length)dim|=LED_PLAY_GREEN;
    if(looper_screen)buttons|=1u<<B_SEQ;
    else if(editor_page==EDITOR_HOME)buttons|=1u<<B_HOME;
    else if(editor_page==EDITOR_FX1||editor_page==EDITOR_FX2)buttons|=1u<<B_FX;
    else if(editor_page>=EDITOR_AEG_RATES&&editor_page<=EDITOR_PEG_ON)buttons|=1u<<B_ENV;
    else if(editor_page>=EDITOR_LFO1&&editor_page<=EDITOR_LFO3)buttons|=1u<<B_LFO;
    else if(editor_page==EDITOR_VOICE||editor_page==EDITOR_MIDI)buttons|=1u<<B_GLO;
    else buttons|=1u<<B_EDIT;
    if(control_request.editor.fx==1)dim|=1u<<B_FX;
    if(control_request.editor.dirty)dim|=1u<<B_SAVE;
    if(draft_mask)dim|=1u<<B_ARP;
    if(store_screen!=STORE_CLOSED)buttons|=1u<<B_SAVE;
    if(draft_screen!=DRAFT_CLOSED)buttons|=1u<<B_ARP;
    if(octave<0)buttons|=1u<<B_OCTDN;else if(octave>0)buttons|=1u<<B_OCTUP;
    plat_lights(buttons,keys,dim,0,1);
}
// Main-loop only; no CPU1/UI job is active at this point. Saving deliberately
// silences playback before the platform's flash critical section.
static void store_commit(void){
    if(!control_reply.save_pending)return;
    if(looper.state!=LOOP_STOP){control_reply.save_pending=0;store_screen=STORE_ERROR;return;}
    const uint32_t slot=control_reply.save_slot;control_reply.save_pending=0;
    uint32_t f=audio_control_lock();
    if(looper.state!=LOOP_STOP){plat_unlock(f);store_screen=STORE_ERROR;return;}
    int encoded=rdx_user_encode(slot,control_reply.save_patch,control_reply.save_trim,store_bank);
    uint32_t dry_state[8];rdx_dry_snapshot(dry_state);
    rdx_dry_mode(0);phrase_panic();event_read=event_write;maintenance=1;
    plat_unlock(f);
    // Dry completion precedes BSP return: retire that tail before flash guard.
    plat_cpu2_wait();plat_cpu2_sync();
    int ok=0;
    if(encoded){
        uint32_t hash=gui_hash(store_bank,RDX_USER_BANK_BYTES);
        if(!plat_store_save(OBJ_USER_BANK0+slot/16u,store_bank,RDX_USER_BANK_BYTES)){
            int n=plat_store_load(OBJ_USER_BANK0+slot/16u,store_bank,RDX_USER_BANK_BYTES);
            if(n==RDX_USER_BANK_BYTES&&hash==gui_hash(store_bank,RDX_USER_BANK_BYTES))ok=rdx_user_decode(slot/16u,store_bank,(uint32_t)n);
        }
    }
    f=plat_lock();
    if(ok){preset=(int32_t)slot+33;rdx_preset_request((uint32_t)preset);}
    rdx_dry_mode(dry_state[0]&&plat_cpu2_on());maintenance=0;plat_unlock(f);
    store_screen=ok?STORE_DONE:STORE_ERROR;editor_redraw=1;gui_ready=0;
}
// No control/GUI job is active. IRQs output silence throughout validation,
// flash work and restoration, so the main-owned phrase is never raced.
static void draft_commit(void){
    uint32_t action=control_reply.draft_action,slot=control_reply.draft_slot;control_reply.draft_action=DRAFT_ACTION_NONE;
    if(!action)return;
    if(slot>=DRAFT_SLOTS||action>=DRAFT_ACTIONS_COUNT){draft_screen=DRAFT_ERROR;return;}
    uint32_t f=audio_control_lock(),dry_state[8];if(looper.state!=LOOP_STOP){plat_unlock(f);draft_screen=DRAFT_ERROR;return;}rdx_dry_snapshot(dry_state);rdx_dry_mode(0);phrase_panic();event_read=event_write;maintenance=1;plat_unlock(f);
    plat_cpu2_wait();plat_cpu2_sync();int ok=0;
    if(action==DRAFT_ACTION_SAVE){
        uint8_t patch[150],perf[3];
        if(rdx_phrase_export(patch,perf))ok=draft_save(&draft_io,slot,&looper,patch,perf,store_bank);
    }else if(action==DRAFT_ACTION_LOAD){
        ok=draft_load(&draft_io,slot,&looper,draft_header,store_bank);
        if(ok&&!rdx_phrase_restore(draft_header+32,draft_header+20)){loop_init(&looper);ok=0;}
    }
    draft_refresh();f=plat_lock();guide_restart();rdx_dry_mode(dry_state[0]&&plat_cpu2_on());maintenance=0;plat_unlock(f);
    draft_screen=ok?(action==DRAFT_ACTION_SAVE?DRAFT_SAVED:DRAFT_LOADED):DRAFT_ERROR;editor_redraw=1;gui_ready=0;
}
#include "control_dispatch.h"
#ifdef PLAT_HOST
int app_expect(const char* what,int* got) {
    if(!strcmp(what,"draft")){*got=draft_screen;return 1;}
    if(!strcmp(what,"draft_mask")){*got=draft_mask;return 1;}
    if(!strcmp(what,"guide")){*got=guide_mode;return 1;}
    if(!strcmp(what,"beat_pulses")){*got=beat_pulses;return 1;}
    if(!strcmp(what,"quantize")){*got=looper.quantize;return 1;}
    if(!strcmp(what,"loop_view")){*got=looper_screen;return 1;}
    if(!strcmp(what,"loop_length")){*got=looper.length;return 1;}
    if(!strcmp(what,"loop_tempo")){*got=looper.tempo;return 1;}
    if(!strcmp(what,"live_note53")){*got=live_notes[53];return 1;}
    if(!strcmp(what,"live_note65")){*got=live_notes[65];return 1;}
    if(!strcmp(what,"store")){*got=store_screen;return 1;}
    if(!strcmp(what,"user_mask")){*got=(int)rdx_user_mask();return 1;}
    if(!strcmp(what,"loop")){*got=looper.state;return 1;}
    if(!strcmp(what,"phrase_name0")){*got=(unsigned char)control_request.phrase_name[0];return 1;}
    if(!strcmp(what,"live_note60")){*got=live_notes[60];return 1;}
    if(!strcmp(what,"loop_note60")){*got=loop_notes[60];return 1;}
    if(!strcmp(what,"loop_events")){*got=looper.count[0];return 1;}

    if(!strcmp(what,"preset"))*got=(int)preset;
    else if(!strcmp(what,"dry_enabled")){uint32_t d[8];rdx_dry_snapshot(d);*got=d[0];}
    else if(!strcmp(what,"dry_jobs")){uint32_t d[8];rdx_dry_snapshot(d);*got=d[1]>0;}
    else if(!strcmp(what,"dry_late")){uint32_t d[8];rdx_dry_snapshot(d);*got=d[4];}
    else if(!strcmp(what,"dry_view"))*got=dry_view;
    else if(!strcmp(what,"ui_samples"))*got=ui_calls[UI_DMA_WAIT]&&ui_calls[UI_CLEAR]&&ui_calls[UI_TEXT]&&ui_calls[UI_SUBMIT];
    else if(!strcmp(what,"timing"))*got=timing_page;
    else if(!strcmp(what,"core_mode"))*got=core_mode;
    else if(!strcmp(what,"core_ok"))*got=core_done>0;
    else if(!strcmp(what,"core_errors"))*got=core_errors;
    else if(!strcmp(what,"gui_enabled"))*got=gui_enabled;
    else if(!strcmp(what,"gui_jobs"))*got=gui_jobs>0;
    else if(!strcmp(what,"gui_core"))*got=gui_last_core;
    else if(!strcmp(what,"gui_errors"))*got=gui_errors;
    else if(!strcmp(what,"control_core"))*got=control_last_core;
    else if(!strcmp(what,"control_errors"))*got=control_errors;
    else if(!strcmp(what,"control_drops"))*got=control_drops;
    else if(!strcmp(what,"control_midi"))*got=control_midi_count>0;
    else if(!strcmp(what,"cost_state")){uint32_t p[COST_SNAPSHOT_WORDS];rdx_measure_snapshot(p);*got=p[0];}
    else if(!strcmp(what,"cost_target")){uint32_t p[COST_SNAPSHOT_WORDS];rdx_measure_snapshot(p);*got=p[1];}
    else if(!strcmp(what,"cost_selection"))*got=cost_selection;
    else if(!strcmp(what,"bench_selection"))*got=bench_selection;
    else if(!strcmp(what,"bench_case")){uint32_t r[4];rdx_bench_result(0,r);*got=r[2];}
    else if(!strcmp(what,"profile_enabled")){uint32_t p[9];rdx_detail_profile(p);*got=p[7];}
    else if(!strcmp(what,"profile_fine"))*got=profile_fine;
    else if(!strcmp(what,"profile_blocks")){uint32_t p[9];rdx_detail_profile(p);*got=p[6]>0;}
    else if(!strcmp(what,"voice_limit"))*got=rdx_voice_status()&255u;
    else if(!strcmp(what,"voice_active"))*got=(rdx_voice_status()>>8)&255u;
    else if(!strcmp(what,"voice_capacity"))*got=(rdx_voice_status()>>16)&255u;
    else if(!strcmp(what,"voices_in_budget")){uint32_t v=rdx_voice_status();*got=((v>>8)&255u)<=(v&255u);}
    else if(!strcmp(what,"bank"))*got=preset?((int)preset-1)/8+1:0;
    else if(!strcmp(what,"patch"))*got=preset?((int)preset-1)%8+1:0;
    else if(!strcmp(what,"octave"))*got=(int)octave;
    else if(!strcmp(what,"key_mask"))*got=(int)keys_before;
    else if(!strcmp(what,"fx"))*got=(int)rdx_fx_status();
    else if(!strcmp(what,"page"))*got=(int)editor_page;
    else if(!strcmp(what,"operator"))*got=(int)editor_op;
    else if(!strcmp(what,"op1_level"))*got=editor_view.patch[38+18];
    else if(!strcmp(what,"op2_level"))*got=editor_view.patch[38+28+18];
    else if(!strcmp(what,"op3_level"))*got=editor_view.patch[38+56+18];
    else if(!strcmp(what,"op4_level"))*got=editor_view.patch[38+84+18];
    else if(!strcmp(what,"algorithm"))*got=editor_view.patch[16];
    else if(!strcmp(what,"lfo_wave"))*got=editor_view.patch[17];
    else if(!strcmp(what,"midi_rx"))*got=(int)editor_view.midi_rx;
    else if(!strcmp(what,"midi_control"))*got=(int)editor_view.midi_control;
    else if(!strcmp(what,"peg1"))*got=(int)editor_view.patch[38+16];
    else return 0;
    return 1;
}
#endif
