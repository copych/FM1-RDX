#define PLAT_HOST
#include "../app/engine.cpp"
#include <cassert>
#include <initializer_list>
static float l[128],r[128];
int main(){
    rdx_boot();rdx_preset_request(0);
    for(unsigned i=0;i<128;++i)rdx_render(l,r);
    assert(fade==PRESET_STEADY);
    budget.reset(0);budget.observe(0,50000,42000,4,true);assert(budget.limit==4);engine->setVoiceLimit(4);
    rdx_measure_command(COST_START,0);rdx_measure_prepare(0);assert(voice_measure.state==COST_SETTLE&&engine->activeVoices()==0);
    for(uint32_t ms:{100u,2001u,5002u}){rdx_render(l,r);rdx_budget_observe(1000,ms);}
    assert(voice_measure.state==COST_ARMED&&voice_measure.target==1);
    rdx_measure_prepare(5003);assert(engine->activeVoices()==1&&engine->voiceLimit()==1);
    rdx_render(l,r);rdx_budget_observe(RdxVoiceMeasure::guard+1,5003);
    rdx_budget_overload(5003); // explicit platform overload: host wall time is intentionally not a device measurement
    assert(voice_measure.state==COST_LIMIT&&!rdx_measure_active()&&engine->activeVoices()==0&&engine->voiceLimit()==4);
    rdx_budget_overload(5003);assert(budget.limit==4); // same measurement block must not shed normal budget
    rdx_measure_prepare(5004);rdx_budget_overload(5004);assert(budget.limit==3); // protection resumes next normal block
    auto p=RDX_State::getState().workingPatch;p.common.monoPoly=1;assert(engine->applyPatch(p));
    rdx_measure_command(COST_START,6000);rdx_measure_prepare(6000);assert(voice_measure.state==COST_NEED_POLY&&!rdx_measure_active());
    p.common.monoPoly=0;assert(engine->applyPatch(p));
    rdx_measure_command(COST_START,7000);rdx_measure_prepare(7000);
    rdx_editor_adjust(EDITOR_LEVEL,0,0,1,0);rdx_measure_prepare(7001);
    assert(voice_measure.state==COST_CHANGED&&!rdx_measure_active()&&engine->activeVoices()==0);
    rdx_render(l,r);rdx_sustain(1);assert(pedal_down);
    rdx_measure_command(COST_START,8000);rdx_measure_prepare(8000);
    rdx_measure_command(COST_STOP,8001);assert(RDX_State::getState().controls.sustain&&pedal_down);
    rdx_sustain(0);
    // Actual CPU1 adapter: force 6/7/8 while normal admission remains <=6.
    rdx_panic();for(unsigned i=0;i<128;++i)rdx_render(l,r);
    rdx_dry_mode(1);rdx_measure_command(COST_START,10000);
    unsigned seen=0;
    for(uint32_t t=10000;t<27000;t+=100){
        rdx_measure_recover();rdx_measure_prepare(t);
        if(voice_measure.active())seen|=1u<<voice_measure.target;
        rdx_render(l,r);rdx_budget_observe(0,t);
        if(rdx_dry_publish())rdx_dry_service();
    }
    assert(voice_measure.state==COST_DONE&&voice_measure.target==8);
    assert((seen&(7u<<6))==(7u<<6)&&engine->activeVoices()==0&&engine->voiceLimit()<=6);
    for(unsigned n=6;n<=8;++n)assert(voice_measure.rows[n].v[COST_ACTUAL]==n&&voice_measure.rows[n].v[COST_STEADY_BLOCKS]>0);
    // Late job: stop without touching worker-owned voices, record after release.
    rdx_measure_command(COST_START,28000);rdx_measure_prepare(28000);
    rdx_render(l,r);rdx_budget_observe(0,28000);assert(rdx_dry_publish()&&!rdx_dry_acquire());
    rdx_dry_late();assert(!voice_measure.active()&&dry.restore_pending&&voice_measure.state==COST_LIMIT);
    rdx_dry_service();rdx_measure_recover();
    assert(!dry.restore_pending&&engine->activeVoices()==0&&engine->voiceLimit()<=6);
    assert(voice_measure.rows[6].v[COST_ACTUAL]==6);
    // Platform overload is deferred when a forced worker job is in flight.
    rdx_measure_command(COST_START,29000);rdx_measure_prepare(29000);
    rdx_render(l,r);rdx_budget_observe(0,29000);assert(rdx_dry_publish());
    rdx_budget_overload(29000);assert(dry.restore_pending&&!voice_measure.active());
    rdx_dry_service();rdx_measure_recover();assert(engine->activeVoices()==0&&engine->voiceLimit()<=6);
    // Manual cancel must not train normal admission from the last forced job.
    rdx_measure_command(COST_START,30000);rdx_measure_prepare(30000);
    rdx_render(l,r);rdx_budget_observe(0,30000);assert(rdx_dry_publish());rdx_dry_service();
    const unsigned normal_limit=pipeline_budget.limit;
    dry.turn_ticks=RdxVoiceMeasure::guard+1;dry.lead_ticks=0;
    rdx_measure_command(COST_STOP,30001);rdx_measure_prepare(30001);
    rdx_render(l,r);rdx_budget_observe(0,30001);assert(pipeline_budget.limit==normal_limit);
    rdx_dry_mode(0);
    dry.lead_ticks=UINT32_MAX;dry.turn_ticks=2;assert(measure_service()==UINT32_MAX);
    dry.lead_ticks=10;dry.turn_ticks=20;dry.callback_ticks=40;assert(measure_service()==40);
    return 0;
}
