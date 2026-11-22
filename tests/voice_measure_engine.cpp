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
    return 0;
}
