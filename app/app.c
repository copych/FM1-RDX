#include "engine_api.h"
#include "editor_api.h"
#include "benchmark.h"
#include "../src/rdx_encoder.h"

enum eEventType { EVENT_ON, EVENT_OFF, EVENT_PANIC, EVENT_PRESET, EVENT_FX, EVENT_EDITOR, EVENT_BEND, EVENT_CC, EVENT_MEASURE, EVENT_SPLIT, EVENTS_COUNT };
static void event_push(uint8_t,uint8_t,uint8_t);
typedef struct {uint8_t type,note,velocity,control;int16_t delta;} rdx_event_t;
static rdx_event_t events[128];
static volatile uint32_t event_read,event_write,panic_requested;
static uint32_t keys_before,buttons_before;
static int32_t octave;
static volatile int32_t preset=1;
static float left[APP_BLOCK],right[APP_BLOCK];
static float master_gain;
static uint8_t editor_page,editor_op,editor_tune,selector_used;
static uint32_t save_notice_until;
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
    octave=0;preset=1;master_gain=0.0f;rdx_boot();
    plat_usb_audio_full(1); // Record at fixed level; MASTER controls local monitoring.
    editor_page=EDITOR_HOME;editor_op=editor_tune=selector_used=0;save_notice_until=0;
    editor_redraw=1;
    reset_knob_motion();
    timing_page=0;bench_started=bench_selection=profile_fine=0;preset_home_requested=0;timing_reset();
    dry_view=0;
    sm_reset();
    core_test_init();
    rdx_dry_set_kick(audio_kick);
}
void app_panic(void) {
    // All platform app_panic entry points are CPU0 main-loop update/recovery.
    gui_collect(1);gui_ready=0;
    core_mode=CORE_TEST_OFF;
    if(core_pending){plat_cpu2_wait();core_pending=0;}
    uint32_t f=audio_control_lock();
    rdx_measure_command(COST_STOP,plat_ms());
    rdx_dry_mode(0);
    rdx_panic();event_read=event_write;panic_requested=0;
    plat_unlock(f);
}
void app_render(int32_t* lr,uint32_t n,uint32_t master_q12) {
    uint32_t i;
    if(n!=APP_BLOCK) {for(i=0;i<n*2u;++i)lr[i]=0;return;}
    if(!rdx_dry_acquire()){
        for(i=0;i<n*2u;++i)lr[i]=0;
        rdx_dry_late();audio_kick();return;
    }
    const uint32_t t0=plat_ticks24();
    rdx_dry_block_start(t0);
    sm_audio();
    if(panic_requested) {rdx_measure_command(COST_STOP,plat_ms());rdx_panic();event_read=event_write;panic_requested=0;}
    for(i=0;i<32u && event_read!=event_write;++i) {
        rdx_event_t e=events[event_read&127u];++event_read;
        if(e.type!=EVENT_MEASURE&&rdx_measure_active())rdx_measure_command(COST_STOP,plat_ms());
        if(e.type==EVENT_ON)rdx_note_on(e.note,e.velocity);
        else if(e.type==EVENT_OFF)rdx_note_off(e.note);
        else if(e.type==EVENT_PANIC)rdx_panic();
        else if(e.type==EVENT_PRESET)rdx_preset_request(e.note);
        else if(e.type==EVENT_FX)rdx_fx_toggle();
        else if(e.type==EVENT_EDITOR)rdx_editor_adjust(e.note,e.velocity,e.control&127u,e.delta,(e.control&128u)!=0);
        else if(e.type==EVENT_BEND)rdx_pitch_bend((uint16_t)e.delta);
        else if(e.type==EVENT_CC)rdx_control_change(e.note,e.velocity);
        else if(e.type==EVENT_MEASURE)rdx_measure_command(e.note,plat_ms());
        else if(e.type==EVENT_SPLIT)rdx_dry_mode(e.note&&plat_cpu2_on());
    }
    rdx_measure_prepare(plat_ms());
    const uint32_t t1=plat_ticks24();
    rdx_render(left,right);
    const uint32_t t2=plat_ticks24();
    const float target=(float)(master_q12>4096u ? 4096u : master_q12)*(1.0f/4096.0f);
    for(i=0;i<n;++i) {
        master_gain+=(target-master_gain)*0.004f;
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
static void control_dma_wait(void){
    audio_checkpoint_at(DRY_AT_WAIT);
    const uint32_t start=ui_ticks();lcd_sync();ui_record(UI_DMA_WAIT,start);
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
static int32_t control_text(int32_t x,int32_t y,const felucca_font_t* font,const char* text,uint16_t color){
    // The original rasterizer advances independently per byte/glyph; preserving
    // its returned x retains clipping, blending and glyph placement exactly.
    for(;*text;++text){
        const char one[2]={*text,0};audio_checkpoint_at(DRY_AT_TEXT);
        const uint32_t start=ui_ticks();x=cv_text(x,y,font,one,color);ui_record(UI_TEXT,start);
    }
    audio_checkpoint_at(DRY_AT_TEXT);return x;
}
static void control_fill(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint16_t color){
    if(x>=240u||y>=240u||!w||!h)return;
    if(w>240u-x)w=240u-x;if(h>240u-y)h=240u-y;
    for(uint32_t row=0;row<h;row+=8u){uint32_t n=h-row;if(n>8u)n=8u;control_begin(w,n,color);control_blit(x,y+row);}
}
#define cv_blit control_blit
#define cv_begin control_begin
#define cv_text control_text
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
    if(pressed&(1u<<B_GLO))editor_page=editor_page==EDITOR_VOICE?EDITOR_MIDI:EDITOR_VOICE;
    if(pressed&(1u<<B_REC))editor_page=EDITOR_LAB;
    if(pressed&(1u<<B_FX)) {
        if(buttons&(1u<<B_SEL)){selector_used=1;event_push(EVENT_FX,0,0);}
        else editor_page=editor_page==EDITOR_FX1?EDITOR_FX2:EDITOR_FX1;
    }
    if(pressed&(1u<<B_SAVE))save_notice_until=plat_ms()+2000u;
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
    move=plat_enc(EN_ALGO);if(move&&!timing_page)editor_event(EDITOR_ALGO,0,0,move,0);
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
    if(pressed&((1u<<B_OCTDN)|(1u<<B_OCTUP))) {
        event_push(EVENT_PANIC,0,0);
        octave=clamp(octave+((pressed&(1u<<B_OCTUP)) ? 1 : -1),-2,2);
        keys_before=keys; // held keys retire; new octave starts on next press
    }
    for(k=0;k<NKEYS;++k) {
        uint32_t bit=1u<<k;uint8_t note=(uint8_t)(53+(int32_t)k+12*octave);
        if((keys&bit) && !(keys_before&bit))event_push(EVENT_ON,note,100);
        else if(!(keys&bit) && (keys_before&bit))event_push(EVENT_OFF,note,0);
    }
    keys_before=keys;buttons_before=buttons;
}
static void draw_text_row(uint32_t y,const felucca_font_t* font,const char* text,uint16_t color) {
    for(uint32_t band=0;band<font->h;band+=8u) {
        uint32_t height=font->h-band;if(height>8u)height=8u;
        cv_begin(240,height,C_BLACK);
        cv_text(10,-(int32_t)band,font,text,color);
        cv_blit(0,y+band);
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
    if(timing_page==6){
        uint32_t now=plat_ms();if(timing_draw&&now-timing_draw<250u)return;timing_draw=now;
        const uint32_t* m=control_request.measure;uint32_t selected=cost_follow?m[1]:cost_selection;if(selected>=COST_ROWS)selected=0;
        const uint32_t* row=m+COST_HEADER_WORDS+selected*COST_ROW_WORDS;
        static const char* states[COST_STATES_COUNT]={"IDLE","WAIT","SETTLE","MEASURE","DONE","STOP","LIMIT","VOICE LOSS","POLY ONLY","CHANGED"};char text[40];
        draw_text_row(4,&FONT_M,"VOICE COST SWEEP",C_WHITE);
        timing_format(text,"TEST %luV %s",m[1],0,states[m[0]<COST_STATES_COUNT?m[0]:COST_IDLE]);draw_text_row(46,&FONT_B,text,C_WHITE);
        timing_format(text,"ROW %luV ACT %lu",selected,row[COST_ACTUAL],"");draw_text_row(72,&FONT_B,text,C_WHITE);
        timing_format(text,"APP %lu SYN %lu us",(row[COST_APP]+23u)/24u,(row[COST_SYN]+23u)/24u,"");draw_text_row(98,&FONT_B,text,C_WHITE);
        timing_format(text,"FX %lu P6 %lu us",(row[COST_FX]+23u)/24u,(m[3]+23u)/24u,"");draw_text_row(124,&FONT_B,text,C_WHITE);
        timing_format(text,"STEADY %lu XRUN %lu",row[COST_STEADY_BLOCKS],plat_xruns()-cost_xruns,"");draw_text_row(150,&FONT_B,text,C_WHITE);
#ifdef PLAT_HOST
        draw_text_row(174,&FONT_B,"HOST: NO DEVICE TIMING",C_GRAY);
#else
        timing_format(text,"90%% GUARD %lu us",(m[4]+23u)/24u,0,"");draw_text_row(174,&FONT_B,text,C_GRAY);
#endif
        draw_text_row(198,&FONT_B,"SAVE START/STOP HOME",C_HI);
        timing_format(text,"SELECT ROW / %lu s",m[2]/1000u,0,"");draw_text_row(216,&FONT_B,text,C_GRAY);return;
    }
    if(timing_page==7){
        uint32_t now=plat_ms();char row[40];
        if((uint32_t)(now-timing_draw)<250u&&timing_draw)return;
        timing_draw=now;
        const uint32_t* d=control_request.dry;const uint32_t* q=control_request.dry_detail;
        static const char* at[DRY_CHECKPOINTS_COUNT]={"IDLE","CONTROL","LCD","TEXT","SCALE","EDITOR","PROTO","WAIT","CLEAR","SUBMIT"};
        if(dry_view){
            const uint32_t* u=control_request.ui;
            draw_text_row(4,&FONT_M,"GUI / LCD PROFILE",C_WHITE);
            timing_format(row,"WAIT %lu CLEAR %lu us",(u[UI_DMA_WAIT]+23u)/24u,(u[UI_CLEAR]+23u)/24u,"");draw_text_row(46,&FONT_B,row,C_WHITE);
            timing_format(row,"TEXT %lu SUBMIT %lu us",(u[UI_TEXT]+23u)/24u,(u[UI_SUBMIT]+23u)/24u,"");draw_text_row(72,&FONT_B,row,C_WHITE);
            timing_format(row,"EDITOR %lu us / CPU%lu",(u[UI_EDITOR]+23u)/24u,plat_core(),"");draw_text_row(98,&FONT_B,row,C_WHITE);
            timing_format(row,"QMAX %lu AT %s",(q[0]+23u)/24u,0,at[q[3]<DRY_CHECKPOINTS_COUNT?q[3]:DRY_AT_IDLE]);draw_text_row(124,&FONT_B,row,C_WHITE);
            timing_format(row,"WORK %lu TURN %lu",(d[2]+23u)/24u,(d[3]+23u)/24u,"");draw_text_row(150,&FONT_B,row,C_WHITE);
            timing_format(row,"LATE %lu XRUN %lu",d[4],plat_xruns()-timing_xruns,"");draw_text_row(174,&FONT_B,row,C_WHITE);
            timing_format(row,"VOICE %lu/%lu",(control_request.voices>>8)&255u,control_request.voices&255u,"");draw_text_row(198,&FONT_B,row,C_HI);
            draw_text_row(216,&FONT_B,"K1: QUEUE / SAVE RESET",C_GRAY);return;
        }
        draw_text_row(4,&FONT_M,"DRY CORE PIPELINE",C_WHITE);
        timing_format(row,"WORK %lu TURN %lu",(d[2]+23u)/24u,(d[3]+23u)/24u,"");draw_text_row(46,&FONT_B,row,C_WHITE);
        timing_format(row,"QMAX %lu WORK %lu",(q[0]+23u)/24u,(q[1]+23u)/24u,"");draw_text_row(72,&FONT_B,row,C_WHITE);
        timing_format(row,"AT %s / %luV WHY %lu",q[4],q[7],at[q[3]<DRY_CHECKPOINTS_COUNT?q[3]:DRY_AT_IDLE]);draw_text_row(98,&FONT_B,row,C_WHITE);
        timing_format(row,"APP %lu FX %lu us",(timing.peak+23u)/24u,(control_request.cost[1]+23u)/24u,"");draw_text_row(124,&FONT_B,row,C_WHITE);
        timing_format(row,"VOICE %lu/%lu",(control_request.voices>>8)&255u,control_request.voices&255u,"");draw_text_row(150,&FONT_B,row,C_WHITE);
        timing_format(row,"LATE %lu FAULT %lu",d[4],plat_cpu2_gave_up(),"");draw_text_row(174,&FONT_B,row,C_WHITE);
        timing_format(row,"XRUN %lu / SYN CPU%lu",plat_xruns()-timing_xruns,d[0]?1u:0u,"");draw_text_row(198,&FONT_B,row,C_HI);
        draw_text_row(216,&FONT_B,"K1: LCD / SAVE RESET",C_GRAY);return;
    }
    if(timing_page==5){
        uint32_t now=plat_ms();if(timing_draw&&now-timing_draw<250u)return;timing_draw=now;
        uint32_t f=plat_lock(),peak=timing.peak,cost[3];rdx_cost_profile(cost);plat_unlock(f);char row[40];
        draw_text_row(4,&FONT_M,"AUDIO ISOLATION",C_WHITE);
        timing_format(row,"CTL CPU%lu JOBS %lu",control_last_core,control_jobs,"");draw_text_row(46,&FONT_B,row,C_WHITE);
        timing_format(row,"ERR %lu FAULT %lu",control_errors,plat_cpu2_gave_up(),"");draw_text_row(72,&FONT_B,row,C_WHITE);
        timing_format(row,"CTL %lu us STK %lu",(control_peak+23u)/24u,plat_cpu2_stack(),"");draw_text_row(98,&FONT_B,row,C_WHITE);
        timing_format(row,"APP %lu SYN %lu us",(peak+23u)/24u,(cost[0]+23u)/24u,"");draw_text_row(124,&FONT_B,row,C_WHITE);
        timing_format(row,"XRUN %lu / %lu s",plat_xruns()-timing_xruns,(now-timing_started)/1000u,"");draw_text_row(150,&FONT_B,row,C_WHITE);
        draw_text_row(174,&FONT_B,control_failed?"FAILED / CPU0 CONTROL":gui_enabled?"SELECT: CPU1 CONTROL":"SELECT: CPU0 CONTROL",C_WHITE);
        draw_text_row(198,&FONT_B,"SAVE RESET / HOME TEST",C_HI);
        timing_format(row,"MIDI %lu DROP %lu",control_midi_count,control_drops,"");draw_text_row(216,&FONT_B,row,C_GRAY);return;
    }
    if(timing_page==4){
        uint32_t now=plat_ms();if(timing_draw&&now-timing_draw<250u)return;timing_draw=now;
        uint32_t f=plat_lock(),peak=timing.peak,over=timing.over90,cost[3];rdx_cost_profile(cost);plat_unlock(f);char row[40];
        draw_text_row(4,&FONT_M,"CONTROL CORE TEST",C_WHITE);
        timing_format(row,"CORE %lu FAULT %lu",plat_cpu2_on(),plat_cpu2_gave_up(),"");draw_text_row(46,&FONT_B,row,C_WHITE);
        timing_format(row,"OK %lu ERR %lu",core_done,core_errors,"");draw_text_row(72,&FONT_B,row,C_WHITE);
        timing_format(row,"ROUND %lu ms LATE %lu",core_roundtrip,core_late,"");draw_text_row(98,&FONT_B,row,C_WHITE);
        timing_format(row,"APP %lu SYN %lu us",(peak+23u)/24u,(cost[0]+23u)/24u,"");draw_text_row(124,&FONT_B,row,C_WHITE);
        timing_format(row,"XRUN %lu >90%% %lu",plat_xruns()-timing_xruns,over,"");draw_text_row(150,&FONT_B,row,C_WHITE);
        timing_format(row,"%s / %lu s",(now-timing_started)/1000u,0,core_mode==CORE_TEST_BUSY?"BUSY":core_mode==CORE_TEST_IDLE?"IDLE":"OFF");draw_text_row(174,&FONT_B,row,C_WHITE);
        draw_text_row(198,&FONT_B,core_selection==CORE_TEST_BUSY?"SELECT: BUSY / SAVE RUN":"SELECT: IDLE / SAVE RUN",C_HI);
#ifdef PLAT_HOST
        draw_text_row(216,&FONT_B,"HOST: SIMULATED CORE",C_GRAY);
#else
        draw_text_row(216,&FONT_B,"SAVE STOP/RESET / HOME",C_GRAY);
#endif
        return;
    }
    if(timing_page==3){
        uint32_t now=plat_ms();if(timing_draw&&now-timing_draw<500u)return;timing_draw=now;
        uint32_t p[9],fine[4],f=plat_lock();rdx_detail_profile(p);rdx_detail_fine(fine);plat_unlock(f);char row[40];
        if(profile_fine){
            draw_text_row(4,&FONT_M,"FINE RENDER",C_WHITE);
            timing_format(row,"ENV STEP %lu us",(fine[0]+23u)/24u,0,"");draw_text_row(46,&FONT_B,row,C_WHITE);
            timing_format(row,"ENV GAIN %lu us",(fine[1]+23u)/24u,0,"");draw_text_row(72,&FONT_B,row,C_WHITE);
            timing_format(row,"PITCH %lu  WAVE %lu us",(fine[2]+23u)/24u,(fine[3]+23u)/24u,"");draw_text_row(98,&FONT_B,row,C_WHITE);
            timing_format(row,"MOD %lu  MIX %lu us",(p[1]+23u)/24u,(p[4]+23u)/24u,"");draw_text_row(124,&FONT_B,row,C_WHITE);
            timing_format(row,"TOTAL %lu us VOICES %lu",(p[5]+23u)/24u,p[8],"");draw_text_row(150,&FONT_B,row,C_WHITE);
            draw_text_row(174,&FONT_B,p[7]?"ON / SAVE: STOP":"OFF / SAVE: START",C_HI);
            draw_text_row(198,&FONT_B,"SELECT: COARSE / HOME",C_WHITE);
#ifdef PLAT_HOST
            draw_text_row(216,&FONT_B,"HOST: NO TIMING",C_GRAY);
#else
            draw_text_row(216,&FONT_B,"WORST TOTAL BLOCK",C_GRAY);
#endif
            return;
        }
        draw_text_row(4,&FONT_M,"RENDER PROFILE",C_WHITE);
        timing_format(row,"PREP %lu  MOD %lu us",(p[0]+23u)/24u,(p[1]+23u)/24u,"");draw_text_row(46,&FONT_B,row,C_WHITE);
        timing_format(row,"ENV %lu  OPS %lu us",(p[2]+23u)/24u,(p[3]+23u)/24u,"");draw_text_row(72,&FONT_B,row,C_WHITE);
        timing_format(row,"MIX %lu  TOTAL %lu us",(p[4]+23u)/24u,(p[5]+23u)/24u,"");draw_text_row(98,&FONT_B,row,C_WHITE);
        timing_format(row,"VOICES %lu BLOCKS %lu",p[8],p[6],"");draw_text_row(124,&FONT_B,row,C_WHITE);
        draw_text_row(150,&FONT_B,p[7]?"ON / SAVE: STOP":"OFF / SAVE: START",C_HI);
        draw_text_row(174,&FONT_B,"WORST TOTAL BLOCK",C_GRAY);
        draw_text_row(198,&FONT_B,"SELECT: FINE / HOME",C_WHITE);
#ifdef PLAT_HOST
        draw_text_row(216,&FONT_B,"HOST: NO TIMING",C_GRAY);
#else
        draw_text_row(216,&FONT_B,"SPLIT PATH / IRQ ON",C_GRAY);
#endif
        return;
    }
    if(timing_page==2){
        rdx_bench_tick();
        uint32_t now=plat_ms();if(timing_draw&&now-timing_draw<200u)return;timing_draw=now;
        uint32_t result[4];rdx_bench_result(bench_selection,result);char row[40];
        draw_text_row(4,&FONT_M,"DSP BENCHMARK",C_WHITE);
        draw_text_row(46,&FONT_B,rdx_bench_name(bench_selection),C_WHITE);
        timing_format(row,"MIN %lu MAX %lu ticks",result[0],result[1],"");draw_text_row(72,&FONT_B,row,C_WHITE);
        timing_format(row,"MIN %lu ns / op",result[0]*1000u/(24u*256u),0,"");draw_text_row(98,&FONT_B,row,C_WHITE);
        timing_format(row,"CASE %lu/15 REP %lu",result[2],result[3],"");draw_text_row(124,&FONT_B,row,C_WHITE);
        draw_text_row(150,&FONT_B,!bench_started?"SAVE: START":result[2]>=BENCH_CASES_COUNT?"DONE / SAVE: RERUN":"RUNNING / SAVE: RERUN",C_HI);
        draw_text_row(174,&FONT_B,"SELECT: RESULT",C_WHITE);
        draw_text_row(198,&FONT_B,"256 OPS / IRQ ON",C_GRAY);
#ifdef PLAT_HOST
        draw_text_row(216,&FONT_B,"HOST: NO TIMING",C_GRAY);
#else
        draw_text_row(216,&FONT_B,"24MHz TICKS / HOME EXIT",C_GRAY);
#endif
        return;
    }
    if(timing_page){
        uint32_t now=plat_ms();if(timing_draw&&now-timing_draw<500u)return;timing_draw=now;
        uint32_t f=plat_lock();
        uint32_t peak=timing.peak,over=timing.over90,cost[3];rdx_cost_profile(cost);
        plat_unlock(f);
        char row[40];
        draw_text_row(4,&FONT_M,"AUDIO TIMING",C_WHITE);
        timing_format(row,"APP MAX %lu us",(peak+23u)/24u,0,"");draw_text_row(46,&FONT_B,row,C_WHITE);
        timing_format(row,"SYN %lu  FX %lu us",(cost[0]+23u)/24u,(cost[1]+23u)/24u,"");draw_text_row(72,&FONT_B,row,C_WHITE);
        timing_format(row,"6V %lu us CPU %lu%%",(cost[2]+23u)/24u,plat_cpu_pct(),"");draw_text_row(98,&FONT_B,row,C_WHITE);
        timing_format(row,"XRUN %lu  >90%% %lu",plat_xruns()-timing_xruns,over,"");draw_text_row(124,&FONT_B,row,C_WHITE);
        timing_format(row,"%lu s  P%lu  FX %s",(now-timing_started)/1000u,(uint32_t)preset,rdx_fx_status()?"ON":"OFF");draw_text_row(150,&FONT_B,row,C_WHITE);
        f=plat_lock();uint32_t voices=rdx_voice_status();plat_unlock(f);
        timing_format(row,"VOICES %lu/%lu  MAX 6",(voices>>8)&255u,voices&255u,"");draw_text_row(174,&FONT_B,row,C_WHITE);
        draw_text_row(198,&FONT_B,"SAVE RESET / HOME EXIT",C_HI);
        uint32_t khz=plat_cpu_khz();
        timing_format(row,khz?"%s CLK %lu MHz":"%s CLK ?",khz/1000u,0,rdx_build_label());draw_text_row(216,&FONT_B,row,C_GRAY);return;
    }
    uint32_t lock=plat_lock();rdx_editor_snapshot(&editor_view);plat_unlock(lock);
    static uint32_t last_revision=0,last_fx=99;static uint8_t last_page=255,last_op=255,last_tune=255,last_notice=255;
    uint8_t notice=save_notice_until&&plat_ms()<save_notice_until;
    if(!editor_redraw&&last_revision==editor_view.revision&&last_fx==editor_view.fx&&last_page==editor_page&&last_op==editor_op&&last_tune==editor_tune&&last_notice==notice)return;
    if(gui_ready&&(gui_request.page!=editor_page||gui_request.op!=editor_op||gui_request.tune!=editor_tune))gui_ready=0;
    if(!gui_ready){
        gui_start(&editor_view,editor_page,editor_op,editor_tune);
        if(!gui_ready)return;
    }
    // A completed immutable snapshot may be one frame old. Display it, then
    // revision comparison schedules the newer state on the next frame.
    editor_view=gui_request.view;gui_ready=0;
    editor_redraw=0;
    last_revision=editor_view.revision;last_fx=editor_view.fx;last_page=editor_page;last_op=editor_op;last_tune=editor_tune;last_notice=notice;
    // Each strip below includes its background. Never blank the whole LCD
    // before drawing: the physical panel exposes that intermediate black frame.
    draw_text_row(4,&FONT_M,rdx_editor_title(editor_page),C_WHITE);
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
    for(uint32_t band=0;band<FONT_B.h;band+=8u) {
        cv_begin(240,8,C_BLACK);
        for(uint8_t k=0;k<4;++k)cv_text(k*60u+3u,-(int32_t)band,&FONT_B,gui_reply.labels[k],C_HI);
        cv_blit(0,184u+band);
    }
    char status[30]="OP 1  FX ON";status[3]=(char)('1'+editor_op);
    if(!editor_view.fx){status[9]='O';status[10]='F';status[11]='F';status[12]=0;}
    if(editor_view.dirty){uint32_t n=0;while(status[n])++n;status[n]='*';status[n+1]=0;}
    draw_text_row(216,&FONT_B,notice?"RAM ONLY - NOT SAVED":editor_view.error?"FX MEMORY LIMIT":status,C_GRAY);
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
#undef cv_text
#undef lcd_fill
void app_say(const char* a,const char* b) {(void)a;(void)b;}
void app_ui_init(void) {gui_collect(1);gui_ready=0;editor_redraw=1;rdx_editor_boot();lcd_fill(0,0,240,240,C_BLACK);}

#include "control_dispatch.h"
#ifdef PLAT_HOST
int app_expect(const char* what,int* got) {
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
