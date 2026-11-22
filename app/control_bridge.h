// CPU0 owns audio and the platform's interrupt-facing rings. One CPU1 job at a
// time owns application control/UI/protocol state. Main-loop handoff only;
// app_render never waits for this worker or reads an unfinished reply.
enum eControlJob { CONTROL_INPUT, CONTROL_FRAME, CONTROL_SYSEX, CONTROL_JOBS_COUNT };
enum { CONTROL_COMMANDS=96, AUDIO_COMMANDS=128 };
typedef struct {
    uint32_t sequence,kind,keys,buttons,home,exact;
    int32_t enc[NE];
    uint32_t midi[32],midi_count,channel;
    EditorSnapshot editor;
    uint32_t cost[3],profile[9],fine[4],voices,xruns,cpu,khz;
    uint32_t measure[COST_SNAPSHOT_WORDS];
    uint32_t dry[8];
    uint32_t dry_detail[8];
    uint32_t ui[5];
    RdxTiming timing;
    uint8_t patch[150],system[32],sysex[640];uint32_t sysex_size;
} ControlRequest;
typedef struct {
    uint32_t sequence,core,ticks,count,overflow,reset_timing,detail_changed,detail;
    rdx_event_t commands[CONTROL_COMMANDS];
    uint8_t patch[150],system[32];
    uint32_t patch_pending,patch_full,system_pending,sysex_result,hash,valid;
} ControlReply;
static ControlRequest control_request;
static ControlReply control_reply;
static uint32_t control_active,control_sequence,control_input_hash,control_failed;
static uint32_t control_jobs,control_errors,control_last_core,control_peak,control_midi_count,control_drops;
static void control_run(uint32_t kind,const uint8_t* sysex,uint32_t size);
static void control_push(rdx_event_t event){
    if(control_reply.count<CONTROL_COMMANDS)control_reply.commands[control_reply.count++]=event;
    else control_reply.overflow=1;
}
static uint32_t control_lock(void){return 0;}
static void control_unlock(uint32_t flags){(void)flags;}
static uint32_t control_keys(void){return control_request.keys;}
static uint32_t control_buttons(void){return control_request.buttons;}
static int32_t control_enc(int role){int32_t v=control_request.enc[role];control_request.enc[role]=0;return v;}
static void control_snapshot(EditorSnapshot* out){*out=control_request.editor;}
static void control_cost(uint32_t* out){memcpy(out,control_request.cost,sizeof control_request.cost);}
static void control_profile(uint32_t* out){memcpy(out,control_request.profile,sizeof control_request.profile);}
static void control_fine(uint32_t* out){memcpy(out,control_request.fine,sizeof control_request.fine);}
static void control_detail(uint8_t mode){
    control_reply.detail_changed=1;control_reply.detail=mode;
    control_request.profile[7]=mode;
}
static uint32_t control_voices(void){return control_request.voices;}
static uint32_t control_fx(void){return control_request.editor.fx;}
static uint32_t control_xruns(void){return control_request.xruns;}
static uint32_t control_cpu(void){return control_request.cpu;}
static uint32_t control_khz(void){return control_request.khz;}
