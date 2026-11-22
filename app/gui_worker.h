// CPU0 captures synth state under its interrupt lock. CPU1 receives only this
// immutable request and owns the GUI renderer until the BSP retires the job.
// Neither the worker nor its callees touch LCD/USB/audio/platform canvas state.
typedef struct {EditorSnapshot view;uint32_t sequence;uint8_t page,op,tune;} GuiRequest;
typedef struct {uint8_t pixels[3600];char labels[4][16];uint32_t sequence,core,ticks,hash,valid;} GuiReply;
static GuiRequest gui_request;
static GuiReply gui_reply;
static uint32_t gui_request_hash,gui_sequence,gui_pending,gui_sent,gui_ready;
static uint32_t gui_enabled=1,gui_failed,gui_jobs,gui_errors,gui_peak,gui_last_core;
static uint32_t gui_hash(const void* data,uint32_t size){
    const uint8_t* p=(const uint8_t*)data;uint32_t h=2166136261u;
    for(uint32_t i=0;i<size;++i)h=(h^p[i])*16777619u;
    return h;
}
static void gui_job(uint32_t unused){
    (void)unused;
    const uint32_t start=plat_ticks24();
    GuiRequest request;memcpy(&request,&gui_request,sizeof request);
    gui_reply.valid=0;
    if(gui_hash(&request,sizeof request)!=gui_request_hash||request.page>=EDITOR_PAGES_COUNT||request.op>=4||request.tune>=5)return;
    const uint8_t* pixels=rdx_editor_draw(&request.view,request.page,request.op,request.tune);
    audio_checkpoint();
    memset(gui_reply.pixels,0,sizeof gui_reply.pixels);
    // Same nearest-neighbour scaling and bit order as the original CPU0 path.
    for(uint32_t y=0;y<120;++y){if(!(y&7u))audio_checkpoint();for(uint32_t x=0;x<240;++x){
        uint32_t sx=x*128u/240u,sy=y*64u/120u,at=y*240u+x;
        if((pixels[(sy>>3)*128u+sx]>>(sy&7u))&1u)gui_reply.pixels[at>>3]|=(uint8_t)(1u<<(at&7u));
    }
    }
    for(uint32_t k=0;k<4;++k){
        const char* label=rdx_editor_label(request.page,k);uint32_t n=0;
        memset(gui_reply.labels[k],0,16);
        while(n<15u&&label[n]){gui_reply.labels[k][n]=label[n];++n;}
    }
    gui_reply.sequence=request.sequence;gui_reply.core=plat_core();
    gui_reply.ticks=plat_ticks24()-start;
    gui_reply.hash=gui_hash(gui_reply.pixels,sizeof gui_reply.pixels+sizeof gui_reply.labels);
    gui_reply.valid=1;
}
static void gui_failure(void){
    ++gui_errors;gui_failed=1;gui_ready=0;
    // Called only after the worker completed or the BSP has held it.
    // A partially executed GUI job may have modified the GUI's private state.
    rdx_editor_boot();
}
static int gui_collect(int force){
    if(!gui_pending)return 1;
    if(!force&&plat_cpu2_busy()&&(uint32_t)(plat_ms()-gui_sent)<100u)return 0;
    // CPU0 main loop only. Never called from app_render or app_overload.
    int rc=plat_cpu2_wait();plat_cpu2_sync();gui_pending=0;
    if(rc||!gui_reply.valid||gui_reply.sequence!=gui_sequence||
       gui_reply.hash!=gui_hash(gui_reply.pixels,sizeof gui_reply.pixels+sizeof gui_reply.labels)){
        gui_failure();return 1;
    }
    gui_last_core=gui_reply.core;
    if(gui_reply.ticks>gui_peak)gui_peak=gui_reply.ticks;
    ++gui_jobs;gui_ready=1;return 1;
}
static void gui_start(const EditorSnapshot* view,uint8_t page,uint8_t op,uint8_t tune){
    memset(&gui_request,0,sizeof gui_request);
    gui_request.view=*view;gui_request.page=page;gui_request.op=op;gui_request.tune=tune;
    gui_request.sequence=++gui_sequence;gui_request_hash=gui_hash(&gui_request,sizeof gui_request);
    gui_ready=0;plat_cpu2_sync();gui_sent=plat_ms();
#ifndef RDX_CONTROL_ISOLATED
    if(plat_core()==0&&gui_enabled&&!gui_failed&&plat_cpu2_on()){
        if(plat_cpu2_run(gui_job,0)){gui_pending=1;return;}
        // Another diagnostic may still be draining. Do not share the GUI with it.
        // Synthetic jobs do not touch GUI state; this frame can run locally.
    }
#endif
    gui_job(0);gui_last_core=gui_reply.core;gui_ready=gui_reply.valid;
    if(gui_ready){++gui_jobs;if(gui_reply.ticks>gui_peak)gui_peak=gui_reply.ticks;}
}
