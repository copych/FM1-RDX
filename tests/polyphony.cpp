#include "rdx_engine.h"
#include "rdx_budget.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::abort();}}while(0)
static float l[128],r[128];
static void block(RdxEngine& e){e.renderBlock(l,r);for(int i=0;i<128;++i)CHECK(std::isfinite(l[i])&&fabsf(l[i])<=1.0f);}
int main(){
    RdxEngine engine;auto patch=RdxEngine::initPatch();patch.ops[0].egRate[3]=127;
    CHECK(engine.applyPatch(patch));block(engine);CHECK(engine.renderedVoices()==0);
    engine.setVoiceLimit(8);for(int n=60;n<68;++n)engine.noteOn(n,100);
    CHECK(engine.activeVoices()==8);block(engine);CHECK(engine.renderedVoices()==8);
    engine.noteOn(72,100);CHECK(engine.activeVoices()==8);block(engine);
    // Runtime reductions retire selected voices, not arbitrary upper slots.
    engine.setVoiceLimit(3);block(engine);CHECK(engine.activeVoices()==3);
    engine.setVoiceLimit(8);for(int n=80;n<85;++n)engine.noteOn(n,100);CHECK(engine.activeVoices()==8);
    engine.allNotesOff();for(int b=0;b<1000;++b)block(engine);CHECK(engine.activeVoices()==0);block(engine);CHECK(engine.renderedVoices()==0);
    engine.noteOn(69,100);engine.sustain(true);engine.noteOff(69);block(engine);CHECK(engine.activeVoices()==1);
    engine.sustain(false);for(int b=0;b<1000;++b)block(engine);CHECK(engine.activeVoices()==0);
    // Modulator activity does not keep an inaudible, finished carrier allocated.
    patch.common.algorithm=0;patch.ops[1].egRate[3]=0;CHECK(engine.applyPatch(patch));engine.noteOn(60,100);block(engine);engine.noteOff(60);
    for(int b=0;b<1000;++b)block(engine);CHECK(engine.activeVoices()==0);
    // Quiet attacks remain allocated even before any appreciable output.
    patch.ops[0].egRate[0]=0;CHECK(engine.applyPatch(patch));engine.setVoiceLimit(8);
    for(int n=60;n<68;++n)engine.noteOn(n,100);block(engine);CHECK(engine.activeVoices()==8&&engine.renderedVoices()==8);
    engine.panic();block(engine);CHECK(engine.renderedVoices()==0);
    // A budget reduction must prefer an established voice over seven new attacks.
    patch=RdxEngine::initPatch();patch.common.algorithm=0;
    for(int i=0;i<4;++i){patch.ops[0].egRate[i]=127;patch.ops[0].egLevel[i]=i==3?0:127;}
    CHECK(engine.applyPatch(patch));engine.setVoiceLimit(8);engine.noteOn(60,127);for(int b=0;b<5;++b)block(engine);
    patch.ops[0].egRate[0]=0;CHECK(engine.updatePatch(patch));
    for(int n=62;n<69;++n)engine.noteOn(n,127);for(int b=0;b<5;++b)block(engine);
    engine.setVoiceLimit(7);block(engine);CHECK(engine.activeVoices()==7);
    for(int n=62;n<69;++n)engine.noteOff(n);for(int b=0;b<1000;++b)block(engine);
    CHECK(engine.activeVoices()==0); // the older completed attack was retired
    for(int algorithm=0;algorithm<12;++algorithm){
        patch=RdxEngine::initPatch();patch.common.algorithm=algorithm;
        CHECK(engine.applyPatch(patch));engine.setVoiceLimit(8);
        for(int n=100;n<108;++n)engine.noteOn(n,127);
        for(int b=0;b<100;++b)block(engine);CHECK(engine.activeVoices()==8);
    }
    RdxBudget budget;budget.reset(0);CHECK(budget.limit==6);
    for(unsigned ms=0;ms<10000;ms+=3)budget.observe(ms,12000,10000,2,true);
    CHECK(budget.limit==6);
    CHECK(budget.prediction(6)==32000u);
    budget.observe(10001,RdxBudget::target+1,10000,6,true);CHECK(budget.limit==5);
    budget.overload(10002);CHECK(budget.limit==4);
    for(unsigned i=0;i<20;++i)budget.overload(10003+i);CHECK(budget.limit==1);
    budget.reset(0);for(unsigned ms=0;ms<20000;ms+=3)budget.observe(ms,100,0,0,true);CHECK(budget.limit==6); // no idle extrapolation
    budget.observe(20000,45000,40000,2,true);CHECK(budget.limit==2); // predict full occupancy, not just current chord
    budget.reset(0xfffffff0u);budget.limit=4;budget.observe(1600,12000,10000,2,true);CHECK(budget.limit==5); // timer wrap recovery
    budget.reset(0);for(unsigned ms=0;ms<10000;ms+=3)budget.observe(ms,12000,10000,2,false);CHECK(budget.limit==6); // transitions excluded
    std::puts("PASS: eight voices, admission/retirement/release/sustain/slow-attack/idle skipping and predictive budget limits/recovery/wrap");
}
