#include "rdx_pipeline_budget.h"
#include <cassert>
#include <cstdio>
static constexpr uint32_t us(uint32_t n){return n*24u;}
int main(){
    RdxPipelineBudget b;b.reset(0);
    // Reproduce the reported work/turn scale, with an explicit publication lead.
    b.observe(0,us(399),us(50),us(1196),us(1083),3,true,false);
    assert(b.limit==3&&b.reason==b.REASON_PREDICT);
    assert(b.prediction(6)==us(3412));
    // GUI latency ages out; steady rendering remains fully accounted for.
    for(uint32_t t=3;t<12000;t+=3)b.observe(t,us(399),us(50),us(120),us(1083),3,true,false);
    assert(b.limit==6&&b.reason==b.REASON_GROW);
    assert(b.prediction(6)==us(2336));
    b.reset(0);b.observe(1,us(399),us(50),us(120),us(1083),3,true,true);
    assert(b.limit==5&&b.reason==b.REASON_LATE);
    b.reset(0);b.observe(1,us(2800),0,0,0,0,false,false);
    assert(b.limit==5&&b.reason==b.REASON_CPU0);
    b.reset(0);b.observe(1,us(399),us(100),us(1100),us(1500),4,true,false);
    assert(b.limit==5&&b.reason==b.REASON_TURN);
    // CPU0 FX just above the soft target cannot zero the worker's voice budget.
    b.reset(0);b.observe(1,us(2500),us(50),us(100),us(1000),4,true,false);assert(b.limit==6);
    b.reset(0);b.limit=3;
    for(uint32_t t=0;t<8000;t+=3)b.observe(t,us(2500),us(50),us(100),us(1000),4,true,false);
    assert(b.limit==3); // fixed CPU0 pressure prevents growth
    // Structural/invalid samples do not train a per-voice model.
    b.reset(0);b.observe(0,us(399),us(50),us(1000),us(1083),3,false,false);
    assert(b.limit==6&&b.prediction(6)==0);
    b.reset(0xfffffff0u);b.limit=3;
    for(uint32_t dt=0;dt<10000;dt+=3)b.observe(0xfffffff0u+dt,us(399),us(50),us(120),us(1083),3,true,false);
    assert(b.limit==6); // wrapping clock and recovery
    b.reset(0);b.observe(1,~0u,~0u,~0u,~0u,255,true,false);assert(b.limit==5);
    b.reset(0);for(unsigned i=0;i<20;++i)b.observe(i,0,0,0,0,0,true,true);assert(b.limit==1);
    std::puts("PASS pipeline budget: reported scale, queue aging, recovery, fixed FX, publication lead, late/CPU0/turn guards, invalid samples, wrap and bounds");
}
