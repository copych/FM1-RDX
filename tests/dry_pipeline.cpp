#define PLAT_HOST
#include "../app/engine.cpp"
#include <cassert>
#include <cstdio>
static float l[128],r[128];
static unsigned kicks;
static void delayed_kick(){++kicks;}
int main(){
    rdx_boot();rdx_preset_request(0);
    for(unsigned i=0;i<128;++i)rdx_render(l,r);
    assert(fade==PRESET_STEADY);
    rdx_dry_mode(1);rdx_note_on(60,100);
    rdx_render(l,r);assert(rdx_dry_acquire());
    for(float x:l)assert(x==0.0f); // pipeline priming
    assert(rdx_dry_publish());assert(!rdx_dry_acquire());
    assert(!rdx_dry_publish()); // no descriptor reuse
    rdx_dry_late();rdx_dry_mode(0);assert(dry.enabled); // owner unavailable
    rdx_dry_service();assert(rdx_dry_acquire()&&dry.jobs==1);
    rdx_dry_service();assert(dry.jobs==1); // no duplicate generation
    assert(engine->activeVoices()==1);
    for(unsigned block=0;block<600;++block){
        if(block==100)rdx_note_off(60);
        if(block==200)rdx_note_on(67,110);
        if(block==300)rdx_preset_request(8);
        if(block==400)rdx_panic();
        rdx_render(l,r);rdx_budget_observe(0,block*3);
        if(rdx_dry_publish()) {assert(!rdx_dry_acquire());rdx_dry_service();}
        assert(rdx_dry_acquire());
        for(float x:l)assert(__builtin_isfinite(x));
    }
    assert(dry.jobs>100&&dry.late==1);
    rdx_dry_set_kick(delayed_kick);
    rdx_render(l,r);assert(kicks==1&&!rdx_dry_acquire());
    unsigned char before[sizeof(RdxEngine)];memcpy(before,engine,sizeof before);
    rdx_budget_observe(RdxBudget::deadline,3000);
    rdx_budget_overload(3000);assert(!dry.cpu0_overload); // same CPU0 block counted once
    assert(!memcmp(before,engine,sizeof before)); // admission waits for next ownership window
    assert(!rdx_dry_publish());rdx_dry_service();
    rdx_dry_set_kick(nullptr);
    rdx_dry_mode(0);rdx_render(l,r);assert(!rdx_dry_publish());
    rdx_dry_mode(1);rdx_render(l,r);assert(rdx_dry_publish());
    // BSP retirement simulation: worker held before ownership is recovered.
    rdx_dry_retired();assert(rdx_dry_acquire()&&!dry.enabled);
    rdx_panic();rdx_render(l,r);
    std::puts("PASS dry pipeline ownership, priming, delayed/duplicate job, live events, patch, panic, mode and retirement");
}
