// These are CPU0 platform entry points. Peripheral input rings are drained here;
// decoding, menus, protocol parsing, drawing and LCD submission run in the job.
static void control_midi(void){
    for(uint32_t i=0;i<control_request.midi_count;++i){
        uint32_t packet=control_request.midi[i],st=(packet>>8)&255u,d1=(packet>>16)&127u,d2=(packet>>24)&127u;
        if(control_request.channel&&(st&15u)!=control_request.channel-1u)continue;
        ++control_midi_count;
        switch(st&240u){
        case 144:event_push(EVENT_ON,d1,d2);break;
        case 128:event_push(EVENT_OFF,d1,0);break;
        case 192:
            if(d1<rdx_preset_count()){preset=d1==32u?0:(int32_t)d1+1;event_push(EVENT_PRESET,preset,0);preset_home();}
            break;
        case 224:control_push((rdx_event_t){EVENT_BEND,0,0,0,(int16_t)(d1|(d2<<7))});break;
        case 176:event_push(EVENT_CC,d1,d2);break;
        default:break;
        }
    }
}
static void control_probe(void){
    // Legacy diagnostic is now a local check on the actual control core, not
    // another asynchronous job competing with that core's owner.
    if(!core_mode)return;
    core_mail.seq=++core_seq;core_mail.mode=core_mode;
    uint32_t hash=core_seq;
    for(uint32_t i=0;i<32u;++i){core_mail.input[i]=core_seq^(i*0x9e3779b9u);hash=(hash<<5|hash>>27)^core_mail.input[i];}
    PLAT_FAR(core_worker)(0);
    if(core_mail.reply==core_seq&&core_mail.result==(hash^(core_mode==CORE_TEST_BUSY?core_expected:0x13579bdfu)))++core_done;
    else ++core_errors;
}
static void control_job(uint32_t unused){
    (void)unused;
    audio_checkpoint();
    uint32_t start=plat_ticks24();
    memset(&control_reply,0,sizeof control_reply);
    if(gui_hash(&control_request,sizeof control_request)!=control_input_hash||control_request.kind>=CONTROL_JOBS_COUNT)return;
    control_active=1;
    if(control_request.kind==CONTROL_SYSEX){
        audio_checkpoint_at(DRY_AT_PROTOCOL);
        if(control_request.sysex_size<=640u)control_reply.sysex_result=control_sysex(control_request.sysex,control_request.sysex_size);
        audio_checkpoint_at(DRY_AT_PROTOCOL);
    }else{
        if(control_request.kind==CONTROL_FRAME)control_frame();else control_input();
        control_midi();control_probe();
    }
    control_active=0;
    control_reply.sequence=control_request.sequence;control_reply.core=plat_core();
    control_reply.ticks=plat_ticks24()-start;
    control_reply.hash=gui_hash(&control_reply,(uint32_t)((uint8_t*)&control_reply.hash-(uint8_t*)&control_reply));
    control_reply.valid=1;
    audio_checkpoint();
}
static void control_recover(void){
    ++control_errors;control_failed=1;control_active=0;gui_failed=1;gui_ready=gui_pending=0;
    control_reply.valid=0;
    // The BSP has completed or held CPU1 before this is called. A failed GUI
    // may have left a panel DMA in flight; drain it before touching its source.
    lcd_sync();rdx_editor_boot();editor_page=EDITOR_HOME;timing_page=0;editor_redraw=1;
    uint32_t owned=audio_control_lock();rdx_dry_retired();plat_unlock(owned);
    keys_before=buttons_before=0;sm_reset();
    uint32_t f=plat_lock();panic_requested=1;plat_unlock(f);
}
static void control_run(uint32_t kind,const uint8_t* sysex,uint32_t size){
    uint32_t f=audio_control_lock();
    memset(&control_request,0,sizeof control_request);
    control_request.sequence=++control_sequence;control_request.kind=kind;
    control_request.keys=plat_keys();control_request.buttons=plat_buttons();
    if(kind!=CONTROL_SYSEX){
        for(uint32_t i=0;i<NE;++i)control_request.enc[i]=plat_enc(i);
        int port;while(control_request.midi_count<32u&&plat_midi_in(&control_request.midi[control_request.midi_count],&port))++control_request.midi_count;
    }else{control_request.sysex_size=size;if(size<=640u)memcpy(control_request.sysex,sysex,size);}
#ifdef PLAT_HOST
    control_request.exact=plat_host_exact;
#endif
    // CPU0's interrupt exclusion protects capture/publication, never CPU1 work.
    control_request.home=preset_home_requested;preset_home_requested=0;
    control_request.channel=rdx_midi_receive_channel();
    rdx_editor_snapshot(&control_request.editor);
    rdx_cost_profile(control_request.cost);rdx_detail_profile(control_request.profile);rdx_detail_fine(control_request.fine);
    rdx_measure_snapshot(control_request.measure);
    rdx_dry_snapshot(control_request.dry);
    rdx_dry_detail(control_request.dry_detail);
    control_request.timing=timing;control_request.voices=rdx_voice_status();
    if(kind==CONTROL_SYSEX){
        if(sm_patch_pending)memcpy(control_request.patch,sm_patch,150);else rdx_patch_export(control_request.patch);
        if(sm_system_pending)memcpy(control_request.system,sm_system,32);else rdx_system_export(control_request.system);
    }
    plat_unlock(f);
    control_request.xruns=plat_xruns();control_request.cpu=plat_cpu_pct();control_request.khz=plat_cpu_khz();
    control_input_hash=gui_hash(&control_request,sizeof control_request);plat_cpu2_sync();
    int dispatched=0,rc=0;
    f=plat_lock();
    if(gui_enabled&&!control_failed&&plat_cpu2_on())dispatched=plat_cpu2_run(control_job,0);
    plat_unlock(f);
    if(dispatched){
        // CPU0 main loop only. Serializes application LCD/USB ownership against
        // platform main-loop operations. Audio IRQs remain enabled throughout.
        // The time limit uses the free-running timer, not delayed GUI ticks.
        uint32_t start=plat_ticks24();
        // Cooperative audio service extends UI wall time. Only the main-loop
        // control timeout changes; the callback still never waits.
        const uint32_t timeout=control_request.dry[0]?12000000u:2400000u;
        while(plat_cpu2_busy()&&(uint32_t)(plat_ticks24()-start)<timeout){
            for(volatile uint32_t pause=0;pause<128u;++pause){}
        }
        rc=plat_cpu2_wait();plat_cpu2_sync();
    }else control_job(0);
    if(rc||!control_reply.valid||control_reply.sequence!=control_sequence||control_reply.count>CONTROL_COMMANDS||
       control_reply.hash!=gui_hash(&control_reply,(uint32_t)((uint8_t*)&control_reply.hash-(uint8_t*)&control_reply))){control_recover();return;}
    ++control_jobs;control_last_core=control_reply.core;
    if(control_reply.ticks>control_peak)control_peak=control_reply.ticks;
    f=audio_control_lock();
    if(control_reply.overflow||event_write-event_read+control_reply.count>128u){panic_requested=1;++control_drops;}
    else for(uint32_t i=0;i<control_reply.count;++i){events[event_write&127u]=control_reply.commands[i];++event_write;}
    if(control_reply.detail_changed)rdx_detail_set(control_reply.detail);
    if(control_reply.patch_pending){memcpy(sm_patch,control_reply.patch,150);sm_patch_full=(sm_patch_pending&&sm_patch_full)||control_reply.patch_full;sm_patch_pending=1;}
    if(control_reply.system_pending){memcpy(sm_system,control_reply.system,32);sm_system_pending=1;}
    plat_unlock(f);
    if(control_reply.reset_timing)timing_reset();
}
void app_input_only(void){control_run(CONTROL_INPUT,0,0);}
void app_frame(void){control_run(CONTROL_FRAME,0,0);}
int app_sysex(const uint8_t* data,uint32_t size){
    if(size>640u)return 0;
    control_run(CONTROL_SYSEX,data,size);
    return control_reply.valid?control_reply.sysex_result:0;
}
