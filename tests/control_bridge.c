// Deferred/failing BSP job simulation around the actual bridge/decoder/commit.
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "../app/editor_api.h"
#include "../app/engine_api.h"
enum {NE=7,EVENT_ON,EVENT_OFF,EVENT_PANIC,EVENT_PRESET,EVENT_FX,EVENT_EDITOR,EVENT_BEND,EVENT_CC};
typedef struct {uint8_t type,note,velocity,control;int16_t delta;} rdx_event_t;
typedef struct {uint32_t blocks,last,peak,control_peak,render_peak,output_peak,over90;} RdxTiming;
static RdxTiming timing;
#include "../app/control_bridge.h"
static uint32_t clock_ticks,core,lock_depth,cpu_on=1,fail_wait,corrupt,stall;
static uint32_t queue[40],queue_count,queue_read,capture_channel,emit_count;
static uint32_t event_read,event_write,panic_requested,preset_home_requested;
static rdx_event_t events[128];
static uint32_t gui_enabled=1,gui_failed,gui_ready,gui_pending,timing_page,editor_page,editor_redraw,keys_before,buttons_before,preset;
static uint32_t gui_sequence,gui_sent,gui_last_core,gui_errors,gui_peak,gui_jobs;
static uint8_t sm_patch[150],sm_system[32];
static uint32_t sm_patch_pending,sm_patch_full,sm_system_pending;
static int plat_cpu2_set(int on){cpu_on=on;return 0;}
static int plat_cpu2_on(void){return cpu_on;}
static uint32_t plat_core(void){return core;}
static uint32_t plat_ms(void){return clock_ticks/24000u;}
static uint32_t plat_ticks24(void){clock_ticks+=24000;return clock_ticks;}
static void plat_cpu2_sync(void){}
static uint32_t plat_lock(void){assert(!core);++lock_depth;return 1;}
static void plat_unlock(uint32_t f){(void)f;assert(!core&&lock_depth);--lock_depth;}
static uint32_t plat_keys(void){assert(!core);return 0;}
static uint32_t plat_buttons(void){assert(!core);return 0;}
static int32_t plat_enc(int n){(void)n;assert(!core);return 0;}
static int plat_midi_in(uint32_t* p,int* port){assert(!core);*port=0;if(queue_read==queue_count)return 0;*p=queue[queue_read++];return 1;}
void rdx_measure_snapshot(uint32_t* p){assert(!core&&lock_depth);memset(p,0,COST_SNAPSHOT_WORDS*4);}
static uint32_t plat_xruns(void){return 0;}
static uint32_t plat_cpu_pct(void){return 0;}
static uint32_t plat_cpu_khz(void){return 360000;}
uint32_t rdx_midi_receive_channel(void){assert(!core&&lock_depth);return capture_channel;}
void rdx_editor_snapshot(EditorSnapshot* p){assert(!core&&lock_depth);memset(p,0,sizeof *p);}
void rdx_cost_profile(uint32_t* p){assert(!core&&lock_depth);memset(p,0,12);}
void rdx_detail_profile(uint32_t* p){assert(!core&&lock_depth);memset(p,0,36);}
void rdx_detail_fine(uint32_t* p){assert(!core&&lock_depth);memset(p,0,16);}
uint32_t rdx_voice_status(void){assert(!core&&lock_depth);return 0;}
void rdx_patch_export(uint8_t* p){assert(!core&&lock_depth);memset(p,0,150);}
void rdx_system_export(uint8_t* p){assert(!core&&lock_depth);memset(p,0,32);}
void rdx_detail_set(uint8_t n){(void)n;assert(!core&&lock_depth);}
uint32_t rdx_preset_count(void){return 33;}
void rdx_editor_boot(void){assert(!core);}
static void lcd_sync(void){assert(!core);}
static void sm_reset(void){assert(!core);}
static void timing_reset(void){assert(!core);}
static uint32_t gui_hash(const void* data,uint32_t n){const uint8_t* p=data;uint32_t h=2166136261u;for(uint32_t i=0;i<n;++i)h=(h^p[i])*16777619u;return h;}
static void event_push(uint8_t t,uint8_t n,uint8_t v){control_push((rdx_event_t){t,n,v,0,0});}
static void preset_home(void){assert(control_active);}
static void control_input(void){assert(control_active);for(uint32_t i=0;i<emit_count;++i)event_push(EVENT_ON,60,100);}
static void control_frame(void){assert(control_active);}
static int control_sysex(const uint8_t* p,uint32_t n){assert(control_active);if(n){control_reply.system[1]=p[0];control_reply.system_pending=1;}return 1;}
#define PLAT_HOST
#define PLAT_FAR(fn) (&fn)
static int plat_host_exact;
static int plat_cpu2_run(void (*fn)(uint32_t),uint32_t arg);
static int plat_cpu2_busy(void);
static int plat_cpu2_wait(void);
#include "../app/core_test.h"
static void (*held_job)(uint32_t);
static uint32_t dispatched_pending;
static int plat_cpu2_run(void (*fn)(uint32_t),uint32_t arg){(void)arg;assert(!core&&lock_depth&&!dispatched_pending);held_job=fn;dispatched_pending=1;return 1;}
static int plat_cpu2_busy(void){
    if(dispatched_pending&&!stall){core=1;held_job(0);core=0;dispatched_pending=0;if(corrupt)control_reply.hash^=1;}
    return dispatched_pending;
}
static int plat_cpu2_wait(void){assert(!core);if(fail_wait){dispatched_pending=cpu_on=0;return -1;}assert(!dispatched_pending);return 0;}
static void audio_checkpoint(void){}
static uint32_t audio_control_lock(void){return plat_lock();}
void rdx_dry_snapshot(uint32_t* p){assert(!core&&lock_depth);memset(p,0,32);}
void rdx_dry_retired(void){assert(!core&&lock_depth);}
#include "../app/control_dispatch.h"
static uint32_t packet(uint32_t status,uint32_t a,uint32_t b){return (status<<8)|(a<<16)|(b<<24);}
int main(void){
    queue[queue_count++]=packet(0x90,60,100);queue[queue_count++]=packet(0xe0,0,96);
    queue[queue_count++]=packet(0xb0,1,64);queue[queue_count++]=packet(0x80,60,0);
    control_run(CONTROL_INPUT,0,0);assert(control_last_core==1&&event_write==4&&!control_errors&&!lock_depth);
    assert(events[0].type==EVENT_ON&&events[1].type==EVENT_BEND&&events[1].delta==12288&&events[2].type==EVENT_CC&&events[3].type==EVENT_OFF);
    capture_channel=2;queue_read=0;control_run(CONTROL_INPUT,0,0);assert(event_write==4);
    capture_channel=0;queue_read=0;event_write=128;event_read=0;
    control_run(CONTROL_INPUT,0,0);assert(control_drops==1&&panic_requested);
    event_write=event_read=0;panic_requested=0;queue_count=queue_read=0;
    emit_count=97;control_run(CONTROL_INPUT,0,0);assert(control_reply.count==96&&control_reply.overflow&&panic_requested&&event_write==0);
    emit_count=0;panic_requested=0;
    uint8_t sx=7;app_sysex(&sx,1);assert(sm_system_pending&&sm_system[1]==7);
    corrupt=1;control_run(CONTROL_INPUT,0,0);assert(control_failed&&control_errors==1&&panic_requested&&!control_reply.valid);
    corrupt=0;control_failed=0;stall=fail_wait=1;control_run(CONTROL_INPUT,0,0);
    assert(control_failed&&control_errors==2&&!cpu_on&&!dispatched_pending&&!control_active);
    stall=fail_wait=0;control_run(CONTROL_INPUT,0,0);assert(control_last_core==0&&!lock_depth);
    return 0;
}
