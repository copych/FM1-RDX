#include "engine_api.h"
#include "editor_api.h"
#include "../src/rdx_encoder.h"

enum eEventType { EVENT_ON, EVENT_OFF, EVENT_PANIC, EVENT_PRESET, EVENT_FX, EVENT_EDITOR, EVENTS_COUNT };
typedef struct {uint8_t type,note,velocity,control;int16_t delta;} rdx_event_t;
static rdx_event_t events[32];
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
static struct {uint32_t blocks,last,peak,control_peak,render_peak,output_peak,over90;} timing;
static uint8_t timing_page;
static volatile uint8_t preset_home_requested;
#include "soundmondo.h"
static uint32_t timing_started,timing_xruns,timing_draw;
void app_overload(void){if(timing.over90!=UINT32_MAX)++timing.over90;rdx_budget_overload(plat_ms());}
static void timing_reset(void){
    uint32_t f=plat_lock();memset(&timing,0,sizeof(timing));plat_unlock(f);
    timing_started=plat_ms();timing_xruns=plat_xruns();timing_draw=0;
}
static RdxEncoderMotion knob_motion[4];
static void reset_knob_motion(void){for(uint8_t k=0;k<4;++k)rdx_encoder_reset(&knob_motion[k]);}
static void preset_home(void){editor_page=EDITOR_HOME;timing_page=0;editor_redraw=1;reset_knob_motion();}
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
    uint32_t f=plat_lock(),w=event_write;
    if(w-event_read==32u) panic_requested=1;
    else {events[w&31u]=(rdx_event_t){type,note,velocity};event_write=w+1u;}
    plat_unlock(f);
}
static void editor_event(uint8_t page,uint8_t op,uint8_t control,int32_t delta,uint8_t fine) {
    if(delta>127)delta=127;if(delta<-127)delta=-127;
    uint32_t f=plat_lock(),w=event_write;
    if(w-event_read==32u)panic_requested=1;
    else {events[w&31u]=(rdx_event_t){EVENT_EDITOR,page,op,(uint8_t)(control|(fine?128u:0u)),(int16_t)delta};event_write=w+1u;}
    plat_unlock(f);
}
void app_init(void) {
    event_read=event_write=panic_requested=0;keys_before=buttons_before=0;
    octave=0;preset=1;master_gain=0.0f;rdx_boot();
    plat_usb_audio_full(1); // Record at fixed level; MASTER controls local monitoring.
    editor_page=EDITOR_HOME;editor_op=editor_tune=selector_used=0;save_notice_until=0;
    editor_redraw=1;
    reset_knob_motion();
    timing_page=0;preset_home_requested=0;timing_reset();
    sm_reset();
}
void app_panic(void) {
    uint32_t f=plat_lock();
    rdx_panic();event_read=event_write;panic_requested=0;
    plat_unlock(f);
}
void app_render(int32_t* lr,uint32_t n,uint32_t master_q12) {
    uint32_t i,pkt;int port;
    if(n!=APP_BLOCK) {for(i=0;i<n*2u;++i)lr[i]=0;return;}
    const uint32_t t0=plat_ticks24();
    sm_audio();
    if(panic_requested) {rdx_panic();event_read=event_write;panic_requested=0;}
    for(i=0;i<32u && event_read!=event_write;++i) {
        rdx_event_t e=events[event_read&31u];++event_read;
        if(e.type==EVENT_ON)rdx_note_on(e.note,e.velocity);
        else if(e.type==EVENT_OFF)rdx_note_off(e.note);
        else if(e.type==EVENT_PANIC)rdx_panic();
        else if(e.type==EVENT_PRESET)rdx_preset_request(e.note);
        else if(e.type==EVENT_FX)rdx_fx_toggle();
        else if(e.type==EVENT_EDITOR)rdx_editor_adjust(e.note,e.velocity,e.control&127u,e.delta,(e.control&128u)!=0);
    }
    // Bound MIDI work even when the sender continuously fills its ring.
    for(i=0;i<32u && plat_midi_in(&pkt,&port);++i) {
        uint32_t st=(pkt>>8)&255u,d1=(pkt>>16)&127u,d2=(pkt>>24)&127u;
        uint32_t channel=rdx_midi_receive_channel();
        if(channel&&(st&15u)!=channel-1u)continue;
        if((st&240u)==144u)rdx_note_on((uint8_t)d1,(uint8_t)d2);
        else if((st&240u)==128u)rdx_note_off((uint8_t)d1);
        else if((st&240u)==192u && d1<rdx_preset_count()) {
            // Programs 0..31 are bank sounds 1..32; program 32 is Init.
            preset=d1==32u?0:(int32_t)d1+1;rdx_preset_request((uint32_t)preset);
            preset_home_requested=1;
        }
        else if((st&240u)==224u)rdx_pitch_bend((uint16_t)(d1|(d2<<7)));
        else if((st&240u)==176u)rdx_control_change((uint8_t)d1,(uint8_t)d2);
    }
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
}
void app_input_only(void) {
    uint32_t home_lock=plat_lock();uint8_t go_home=preset_home_requested;preset_home_requested=0;plat_unlock(home_lock);
    if(go_home)preset_home();
    const uint8_t previous_page=editor_page,previous_op=editor_op;
    uint32_t k,keys=plat_keys(),buttons=plat_buttons();
    uint32_t pressed=buttons&~buttons_before;
    uint32_t released=buttons_before&~buttons;
    if((pressed&(1u<<B_REC))&&(buttons&(1u<<B_SEL))){
        selector_used=1;timing_page=!timing_page;editor_redraw=1;timing_draw=0;
        if(timing_page)lcd_fill(0,0,240,240,C_BLACK);
        pressed&=~(1u<<B_REC);
    }
    if(timing_page){
        if(pressed&(1u<<B_SAVE))timing_reset();
        if(pressed&(1u<<B_HOME)){timing_page=0;editor_redraw=1;}
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
    if(move&&!timing_page){
        if(buttons&(1u<<B_SEL))selector_used=1;
        select_in_menu(clamp(move,-127,127));
    }
    move=plat_enc(EN_ALGO);if(move&&!timing_page)editor_event(EDITOR_ALGO,0,0,move,0);
    if(previous_page!=editor_page||previous_op!=editor_op)reset_knob_motion();
    const uint32_t now=plat_ms();
    for(uint8_t k=0;k<4;++k) {
        move=plat_enc(EN_K1+k);if(!move)continue;
        if(timing_page)continue;
        move=clamp(move,-127,127);
        if(editor_page==EDITOR_HOME)editor_page=EDITOR_LEVEL;
        uint8_t fine=(buttons&(1u<<B_SEL))!=0;if(fine)selector_used=1;
        if(editor_page==EDITOR_LAB&&k==0){editor_tune=(uint8_t)clamp((int32_t)editor_tune+move,0,4);reset_knob_motion();continue;}
        int accelerate=continuous_parameter(editor_page,k)&&!fine;
#ifdef PLAT_HOST
        if(plat_host_exact)accelerate=0; // host "spin" explicitly requests exact steps
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
void app_say(const char* a,const char* b) {(void)a;(void)b;}
void app_ui_init(void) {rdx_editor_boot();lcd_fill(0,0,240,240,C_BLACK);}
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
void app_frame(void) {
    app_input_only();
    if(timing_page){
        uint32_t now=plat_ms();if(timing_draw&&now-timing_draw<500u)return;timing_draw=now;
        uint32_t f=plat_lock();
        uint32_t peak=timing.peak,control=timing.control_peak,render=timing.render_peak,output=timing.output_peak,over=timing.over90;
        plat_unlock(f);
        char row[40];
        draw_text_row(4,&FONT_M,"AUDIO TIMING",C_WHITE);
        timing_format(row,"APP MAX %lu us",(peak+23u)/24u,0,"");draw_text_row(46,&FONT_B,row,C_WHITE);
        timing_format(row,"CTRL %lu  DSP %lu",(control+23u)/24u,(render+23u)/24u,"");draw_text_row(72,&FONT_B,row,C_WHITE);
        timing_format(row,"OUT %lu us  CPU %lu%%",(output+23u)/24u,plat_cpu_pct(),"");draw_text_row(98,&FONT_B,row,C_WHITE);
        timing_format(row,"XRUN %lu  >90%% %lu",plat_xruns()-timing_xruns,over,"");draw_text_row(124,&FONT_B,row,C_WHITE);
        timing_format(row,"%lu s  P%lu  FX %s",(now-timing_started)/1000u,(uint32_t)preset,rdx_fx_status()?"ON":"OFF");draw_text_row(150,&FONT_B,row,C_WHITE);
        f=plat_lock();uint32_t voices=rdx_voice_status();plat_unlock(f);
        timing_format(row,"VOICES %lu/%lu  MAX 8",(voices>>8)&255u,voices&255u,"");draw_text_row(174,&FONT_B,row,C_WHITE);
        draw_text_row(198,&FONT_B,"SAVE RESET / HOME EXIT",C_HI);
        draw_text_row(216,&FONT_B,"DEADLINE 2902 us",C_GRAY);return;
    }
    uint32_t lock=plat_lock();rdx_editor_snapshot(&editor_view);plat_unlock(lock);
    static uint32_t last_revision=0,last_fx=99;static uint8_t last_page=255,last_op=255,last_tune=255,last_notice=255;
    uint8_t notice=save_notice_until&&plat_ms()<save_notice_until;
    if(!editor_redraw&&last_revision==editor_view.revision&&last_fx==editor_view.fx&&last_page==editor_page&&last_op==editor_op&&last_tune==editor_tune&&last_notice==notice)return;
    editor_redraw=0;
    last_revision=editor_view.revision;last_fx=editor_view.fx;last_page=editor_page;last_op=editor_op;last_tune=editor_tune;last_notice=notice;
    // Each strip below includes its background. Never blank the whole LCD
    // before drawing: the physical panel exposes that intermediate black frame.
    draw_text_row(4,&FONT_M,rdx_editor_title(editor_page),C_WHITE);
    const uint8_t* pixels=rdx_editor_draw(&editor_view,editor_page,editor_op,editor_tune);
    for(uint32_t y=0;y<120;y+=8) {
        cv_begin(240,8,C_BLACK);
        for(uint32_t row=0;row<8;++row)for(uint32_t x=0;x<240;++x) {
            uint32_t sx=x*128u/240u,sy=(y+row)*64u/120u;
            if((pixels[(sy>>3)*128u+sx]>>(sy&7u))&1u)cv_pset(x,row,C_WHITE);
        }
        cv_blit(0,44u+y);
    }
    cv_begin(240,8,C_BLACK);cv_blit(0,174);
    for(uint32_t band=0;band<FONT_B.h;band+=8u) {
        cv_begin(240,8,C_BLACK);
        for(uint8_t k=0;k<4;++k)cv_text(k*60u+3u,-(int32_t)band,&FONT_B,rdx_editor_label(editor_page,k),C_HI);
        cv_blit(0,184u+band);
    }
    char status[30]="OP 1  FX ON";status[3]=(char)('1'+editor_op);
    if(!editor_view.fx){status[9]='O';status[10]='F';status[11]='F';status[12]=0;}
    if(editor_view.dirty){uint32_t n=0;while(status[n])++n;status[n]='*';status[n+1]=0;}
    draw_text_row(216,&FONT_B,notice?"RAM ONLY - NOT SAVED":editor_view.error?"FX MEMORY LIMIT":status,C_GRAY);
}
#ifdef PLAT_HOST
int app_expect(const char* what,int* got) {
    if(!strcmp(what,"preset"))*got=(int)preset;
    else if(!strcmp(what,"timing"))*got=timing_page;
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
