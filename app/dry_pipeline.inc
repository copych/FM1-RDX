// One live voice owner at a time. CPU0 owns voices while request==complete;
// CPU1 alone owns them between publication and completion. No audio waits.
// The BSP owns job lifetime, check words, flash exclusion and worker faults.
#if defined(RDX_FM1)
extern "C" void plat_cpu2_sync(void);
#else
static void plat_cpu2_sync(){}
#endif
static struct {
    volatile uint32_t request,complete;
    uint32_t enabled,prepared,valid,started,worker_ticks,turn_ticks;
    uint32_t worker_peak,turn_peak,jobs,late;
    uint32_t observed_turn,observed_voices;
    uint32_t block_start,lead_ticks,queue_ticks,observed_lead,observed_queue;
    uint32_t queue_peak,queue_work,queue_turn,queue_at,queue_voices,last_at;
    uint32_t late_signal,cpu0_overload,cpu0_accounted,seen_complete,observed_new;
    void (*kick)(void);
    float gain;
    float samples[RdxEngine::voiceCapacity][DMA_BUFFER_LEN];
} dry;
extern "C" void rdx_dry_set_kick(void (*kick)(void)){dry.kick=kick;}
extern "C" void rdx_dry_block_start(uint32_t ticks){dry.block_start=ticks;}
static RDX_RAM_CODE void dry_core(RdxEngine* synth,float (*samples)[DMA_BUFFER_LEN]){synth->renderDry(samples);}
extern "C" int rdx_dry_acquire(){
    const uint32_t done=dry.complete;plat_cpu2_sync();
    return done==dry.request;
}
extern "C" void rdx_dry_service(){rdx_dry_service_at(DRY_AT_IDLE);}
extern "C" void rdx_dry_service_at(uint32_t checkpoint){
    const uint32_t request=dry.request;plat_cpu2_sync();
    if(request==dry.complete)return;
    const uint32_t start=budget_ticks();
    RDX_FAR(dry_core)(engine,dry.samples);
    const uint32_t end=budget_ticks();
    dry.worker_ticks=end-start;dry.queue_ticks=start-dry.started;
    dry.turn_ticks=end-dry.started;
    dry.last_at=checkpoint<DRY_CHECKPOINTS_COUNT?checkpoint:DRY_AT_IDLE;
#ifdef PLAT_HOST
    dry.worker_ticks=dry.turn_ticks=dry.queue_ticks=dry.lead_ticks=0;
#endif
    if(dry.worker_ticks>dry.worker_peak)dry.worker_peak=dry.worker_ticks;
    if(dry.turn_ticks>dry.turn_peak)dry.turn_peak=dry.turn_ticks;
    if(dry.queue_ticks>=dry.queue_peak){
        dry.queue_peak=dry.queue_ticks;dry.queue_work=dry.worker_ticks;
        dry.queue_turn=dry.turn_ticks;dry.queue_at=dry.last_at;
        dry.queue_voices=engine->renderedVoices();
    }
    ++dry.jobs;dry.valid=1;
    plat_cpu2_sync();dry.complete=request;plat_cpu2_sync();
}
extern "C" int rdx_dry_publish(){
    if(!dry.enabled||!dry.prepared)return 0;
    dry.prepared=0;dry.gain=engine->outputGain();dry.started=budget_ticks();
    dry.lead_ticks=dry.started-dry.block_start;
    plat_cpu2_sync();++dry.request;plat_cpu2_sync();return 1;
}
extern "C" void rdx_dry_mode(uint8_t on){
    // Called only with ownership, through the audio command queue.
    if(!rdx_dry_acquire())return;
    dry.enabled=on!=0;dry.valid=dry.prepared=0;
    dry.late_signal=dry.cpu0_overload=0;
    dry.cpu0_accounted=0;dry.seen_complete=dry.complete;dry.observed_new=0;
    if(on){
        pipeline_budget.reset(0);
        dry.worker_ticks=dry.turn_ticks=dry.queue_ticks=dry.lead_ticks=0;
    }
    if(on){detail_enabled=DETAIL_OFF;if(voice_measure.active())rdx_measure_command(COST_STOP,0);}
    budget_dirty=true;budget_keep_limit=false;
}
extern "C" void rdx_dry_late(){++dry.late;dry.late_signal=1;}
extern "C" void rdx_dry_snapshot(uint32_t* out){
    out[0]=dry.enabled;out[1]=dry.jobs;out[2]=dry.worker_peak;out[3]=dry.turn_peak;
    out[4]=dry.late;out[5]=dry.request-dry.complete;out[6]=dry.worker_ticks;out[7]=dry.turn_ticks;
}
extern "C" void rdx_dry_detail(uint32_t* out){
    out[0]=dry.queue_peak;out[1]=dry.queue_work;out[2]=dry.queue_turn;
    out[3]=dry.queue_at;out[4]=dry.queue_voices;out[5]=dry.queue_ticks;
    out[6]=dry.last_at;out[7]=pipeline_budget.reason;
}
extern "C" void rdx_dry_reset_stats(){
    dry.worker_peak=dry.turn_peak=dry.jobs=dry.late=0;
    dry.queue_peak=dry.queue_work=dry.queue_turn=dry.queue_at=dry.queue_voices=0;
}
extern "C" void rdx_dry_retired(){
    dry.complete=dry.request;dry.enabled=dry.prepared=dry.valid=0;plat_cpu2_sync();
}
