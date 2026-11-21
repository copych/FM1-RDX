// Synthetic control-core test. CPU0 main loop owns submission and collection.
// The audio callback never accesses this state. One job owns the mailbox at a time.
enum eCoreTestMode { CORE_TEST_OFF, CORE_TEST_IDLE, CORE_TEST_BUSY, CORE_TEST_MODES_COUNT };
static struct { volatile uint32_t seq,mode,input[32],result,reply; } core_mail __attribute__((aligned(32)));
static uint32_t core_mode,core_pending,core_seq,core_done,core_errors,core_late,core_sent,core_roundtrip,core_expected;
#ifndef PLAT_HOST
__attribute__((section(".app_ram"),noinline))
#endif
static void core_worker(uint32_t unused){
    (void)unused;
    uint32_t seq=core_mail.seq,mode=core_mail.mode,hash=seq;
    for(uint32_t i=0;i<32u;++i)hash=(hash<<5|hash>>27)^core_mail.input[i];
    // Volatile local keeps a real, bounded integer workload in the target code.
    volatile uint32_t work=0x13579bdfu;
    if(mode==CORE_TEST_BUSY)for(uint32_t i=0;i<8192u;++i)work=work*1664525u+1013904223u;
    core_mail.result=hash^work;
    core_mail.reply=seq;
}
static void core_test_init(void){
    core_mode=core_pending=core_seq=core_done=core_errors=core_late=core_roundtrip=0;
    core_expected=0x13579bdfu;
    for(uint32_t i=0;i<8192u;++i)core_expected=core_expected*1664525u+1013904223u;
    plat_cpu2_set(1); // Only legal here, during app_init before timers/audio start.
}
static void core_test_tick(void){
    if(core_pending){
        if(plat_cpu2_busy() && (uint32_t)(plat_ms()-core_sent)<10u)return;
        // Main loop only: retire faults/timeouts through the BSP's guarded API.
        // No mailbox reuse until completion or the BSP has held the worker.
        int rc=plat_cpu2_wait();plat_cpu2_sync();
        uint32_t elapsed=plat_ms()-core_sent;
        if(elapsed>core_roundtrip)core_roundtrip=elapsed;
        if(elapsed>=10u)++core_late;
        uint32_t hash=core_seq;
        for(uint32_t i=0;i<32u;++i)hash=(hash<<5|hash>>27)^core_mail.input[i];
        uint32_t expected=hash^(core_mail.mode==CORE_TEST_BUSY?core_expected:0x13579bdfu);
        if(rc||core_mail.reply!=core_seq||core_mail.result!=expected)++core_errors;
        else ++core_done;
        core_pending=0;
        if(rc)core_mode=CORE_TEST_OFF;
    }
    if(!core_mode||!plat_cpu2_on())return;
    core_mail.seq=++core_seq;core_mail.mode=core_mode;
    for(uint32_t i=0;i<32u;++i)core_mail.input[i]=core_seq^(i*0x9e3779b9u);
    plat_cpu2_sync();core_sent=plat_ms();
    if(plat_cpu2_run(core_worker,0))core_pending=1;
    else {++core_errors;core_mode=CORE_TEST_OFF;}
}
