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
    void (*kick)(void);
    float gain;
    float samples[RdxEngine::voiceCapacity][DMA_BUFFER_LEN];
} dry;
extern "C" void rdx_dry_set_kick(void (*kick)(void)){dry.kick=kick;}
static RDX_RAM_CODE void dry_core(RdxEngine* synth,float (*samples)[DMA_BUFFER_LEN]){synth->renderDry(samples);}
extern "C" int rdx_dry_acquire(){
    const uint32_t done=dry.complete;plat_cpu2_sync();
    return done==dry.request;
}
extern "C" void rdx_dry_service(){
    const uint32_t request=dry.request;plat_cpu2_sync();
    if(request==dry.complete)return;
    const uint32_t start=budget_ticks();
    RDX_FAR(dry_core)(engine,dry.samples);
    dry.worker_ticks=budget_ticks()-start;
    dry.turn_ticks=budget_ticks()-dry.started;
#ifdef PLAT_HOST
    dry.worker_ticks=dry.turn_ticks=0; // host clocks are not FM1 measurements
#endif
    if(dry.worker_ticks>dry.worker_peak)dry.worker_peak=dry.worker_ticks;
    if(dry.turn_ticks>dry.turn_peak)dry.turn_peak=dry.turn_ticks;
    ++dry.jobs;dry.valid=1;
    plat_cpu2_sync();dry.complete=request;plat_cpu2_sync();
}
extern "C" int rdx_dry_publish(){
    if(!dry.enabled||!dry.prepared)return 0;
    dry.prepared=0;dry.gain=engine->outputGain();dry.started=budget_ticks();
    plat_cpu2_sync();++dry.request;plat_cpu2_sync();return 1;
}
extern "C" void rdx_dry_mode(uint8_t on){
    // Called only with ownership, through the audio command queue.
    if(!rdx_dry_acquire())return;
    dry.enabled=on!=0;dry.valid=dry.prepared=0;
    if(on){detail_enabled=DETAIL_OFF;if(voice_measure.active())rdx_measure_command(COST_STOP,0);}
    budget_dirty=true;budget_keep_limit=false;
}
extern "C" void rdx_dry_late(){++dry.late;}
extern "C" void rdx_dry_snapshot(uint32_t* out){
    out[0]=dry.enabled;out[1]=dry.jobs;out[2]=dry.worker_peak;out[3]=dry.turn_peak;
    out[4]=dry.late;out[5]=dry.request-dry.complete;out[6]=dry.worker_ticks;out[7]=dry.turn_ticks;
}
extern "C" void rdx_dry_reset_stats(){dry.worker_peak=dry.turn_peak=dry.jobs=dry.late=0;}
extern "C" void rdx_dry_retired(){
    dry.complete=dry.request;dry.enabled=dry.prepared=dry.valid=0;plat_cpu2_sync();
}
