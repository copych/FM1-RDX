#pragma once
#include <stdint.h>
// CPU0 block-boundary phrase engine. Four sequential takes, 512 events each.
// No sorting, allocation, I/O or variable-time search in the renderer.
enum eLoopEventType { LOOP_NOTE=0,LOOP_BEND=1,LOOP_CC=2,LOOP_EVENT_TYPES_COUNT,LOOP_RELEASE=255 };
enum eLoopState { LOOP_STOP,LOOP_ARMED,LOOP_RECORD,LOOP_PLAY,LOOP_OVERDUB_ARMED,LOOP_OVERDUB,LOOP_STATES_COUNT };
enum eLoopCommand { LOOP_REC,LOOP_TOGGLE,LOOP_CLEAR,LOOP_TEMPO,LOOP_GUIDE,LOOP_QUANTIZE,LOOP_COMMANDS_COUNT };
enum eLoopQuantize { LOOP_QUANT_OFF,LOOP_QUANT_4,LOOP_QUANT_4T,LOOP_QUANT_8,LOOP_QUANT_8T,LOOP_QUANT_16,LOOP_QUANT_16T,LOOP_QUANT_32,LOOP_QUANT_32T,LOOP_QUANT_MODES_COUNT };
static uint32_t loop_grid(uint8_t mode){static const uint8_t ticks[LOOP_QUANT_MODES_COUNT]={0,96,64,48,32,24,16,12,8};return ticks[mode<LOOP_QUANT_MODES_COUNT?mode:LOOP_QUANT_OFF];}
enum {LOOP_TRACKS=4,LOOP_EVENTS=512,LOOP_MAX_TICKS=57600};
typedef struct {uint16_t tick;uint8_t type,a,b,pad;} LoopEvent;
typedef struct {
    LoopEvent events[LOOP_TRACKS][LOOP_EVENTS];
    uint16_t count[LOOP_TRACKS],cursor[LOOP_TRACKS],position,length,tempo;
    uint32_t fraction;
    uint8_t state,tracks,take,full,quantize,deferred_count,deferred_off[32];
} RdxLooper;
static void loop_init(RdxLooper* l){memset(l,0,sizeof *l);l->tempo=120;}
static int loop_running(const RdxLooper* l){return l->state>=LOOP_RECORD;}
static void loop_stop(RdxLooper* l){l->state=LOOP_STOP;l->position=0;l->fraction=0;memset(l->cursor,0,sizeof l->cursor);l->deferred_count=0;}
static void loop_command(RdxLooper* l,uint8_t command,int value){
    if(command==LOOP_QUANTIZE){if(l->state!=LOOP_RECORD&&l->state!=LOOP_OVERDUB&&value>=0&&value<LOOP_QUANT_MODES_COUNT)l->quantize=(uint8_t)value;return;}
    if(command==LOOP_TEMPO){int t=(int)l->tempo+value;l->tempo=(uint16_t)(t<30?30:t>300?300:t);return;}
    if(command==LOOP_CLEAR){if(l->state==LOOP_STOP){uint16_t t=l->tempo;uint8_t q=l->quantize;loop_init(l);l->tempo=t;l->quantize=q;}return;}
    if(command==LOOP_TOGGLE){if(l->state==LOOP_STOP&&l->length){l->state=LOOP_PLAY;}else loop_stop(l);return;}
    if(command!=LOOP_REC)return;
    if(l->state==LOOP_RECORD){
        l->length=(uint16_t)((((uint32_t)l->position+95u)/96u)*96u);if(!l->length)l->length=96;if(l->length>LOOP_MAX_TICKS)l->length=LOOP_MAX_TICKS;
        l->tracks=1;loop_stop(l);l->state=LOOP_PLAY;
    }else if(l->state==LOOP_OVERDUB){l->tracks=(uint8_t)(l->take+1u);l->state=LOOP_PLAY;}
    else if(l->state==LOOP_ARMED||l->state==LOOP_OVERDUB_ARMED)loop_stop(l);
    else if(!l->length){l->state=LOOP_ARMED;l->take=0;l->count[0]=0;l->full=0;}
    else if(l->tracks<LOOP_TRACKS){l->take=l->tracks;l->count[l->take]=0;l->state=LOOP_OVERDUB_ARMED;}
    else l->full=1;
}
static void loop_capture(RdxLooper* l,uint8_t type,uint8_t a,uint8_t b){
    if(l->state==LOOP_ARMED&&type==LOOP_NOTE&&b){l->state=LOOP_RECORD;l->position=0;l->fraction=0;}
    if(l->state!=LOOP_RECORD&&l->state!=LOOP_OVERDUB)return;
    uint16_t* n=&l->count[l->take];
    if(*n>=LOOP_EVENTS){l->full=1;return;}
    l->events[l->take][(*n)++]=(LoopEvent){l->position,type,a,b,0};
}
// Playback projection only: recording and draft timestamps are never rewritten.
static uint16_t loop_play_tick(const RdxLooper* l,const LoopEvent* e){
    uint32_t grid=loop_grid(l->quantize);if(!grid||!l->length)return e->tick;
    uint32_t tick=((uint32_t)e->tick+grid/2u)/grid*grid;
    // Keep the final grid point within this iteration, retaining event order.
    if(tick>=l->length)tick=((uint32_t)l->length-1u)/grid*grid;
    return (uint16_t)tick;
}
typedef void (*LoopEmit)(uint8_t,uint8_t,uint8_t);
// Called once per 128-frame block at 44100 Hz. Integer musical clock has no
// cumulative block-rounding drift. Callback is capped at 32 events per block.
// On wrap the callback receives type 255 to release loop-owned held notes.
static void loop_advance(RdxLooper* l,LoopEmit emit){
    if(!loop_running(l))return;
    uint32_t budget=32;uint8_t started[16]={0};
    // A note whose on/off snap together must sound for at least one audio block.
    for(uint32_t i=0;i<l->deferred_count;++i){emit(LOOP_NOTE,l->deferred_off[i],0);--budget;}
    l->deferred_count=0;
    if(l->state!=LOOP_RECORD){
        for(uint32_t t=0;t<l->tracks;++t){
            while(l->cursor[t]<l->count[t]&&loop_play_tick(l,&l->events[t][l->cursor[t]])<=l->position){
                if(!budget--){l->full=1;loop_stop(l);emit(LOOP_RELEASE,0,0);return;}
                LoopEvent e=l->events[t][l->cursor[t]++];
                if(l->quantize!=LOOP_QUANT_OFF&&e.type==LOOP_NOTE&&e.a<128u){
                    if(e.b)started[e.a/8u]|=(uint8_t)(1u<<(e.a%8u));
                    else if(started[e.a/8u]&(1u<<(e.a%8u))){l->deferred_off[l->deferred_count++]=e.a;continue;}
                }
                emit(e.type,e.a,e.b);
            }
        }
    }
    l->fraction+=128u*l->tempo*96u;
    uint32_t steps=l->fraction/2646000u;l->fraction-=steps*2646000u;l->position=(uint16_t)(l->position+steps);
    if(l->state==LOOP_RECORD){
        if(l->position>=LOOP_MAX_TICKS){l->length=LOOP_MAX_TICKS;l->tracks=1;loop_stop(l);l->state=LOOP_PLAY;emit(LOOP_RELEASE,0,0);}return;
    }
    if(l->length&&l->position>=l->length){
        emit(LOOP_RELEASE,0,0);l->deferred_count=0;l->position=0;memset(l->cursor,0,sizeof l->cursor);
        if(l->state==LOOP_OVERDUB){l->tracks=(uint8_t)(l->take+1u);l->state=LOOP_PLAY;}
        else if(l->state==LOOP_OVERDUB_ARMED)l->state=LOOP_OVERDUB;
    }
}

