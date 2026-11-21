#include "rdx_engine.h"
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static RdxEngine engine;
static float left[128],right[128];
static float block() {
    engine.renderBlock(left,right);
    float energy=0.0f;
    for(int i=0;i<128;++i) {
        CHECK(std::isfinite(left[i]));CHECK(left[i]==right[i]);CHECK(fabsf(left[i])<=1.0f);
        energy+=left[i]*left[i];
    }
    return energy;
}
int main() {
    CHECK(block()==0.0f);
    CHECK(fast_floorf(-0.1f)==-1.0f);
    CHECK(fast_floorf(-1.0f)==-1.0f);
    CHECK(wrap01(-1.25f)==0.75f);
    CHECK(wrap01(100.25f)==0.25f);
    CHECK(rdxGain(-1.0f)==0.0f);
    CHECK(rdxGain(191.0f)==levelLUT.forward[191]);
    CHECK(std::isfinite(rdxGainInv(1000.0f)));
    CHECK(std::isfinite(semitonesToRatio(-1000.0f)));
    CHECK(std::isfinite(semitonesToRatio(1000.0f)));
    auto p=RdxEngine::initPatch();
    auto bad=p;bad.common.algorithm=12;CHECK(!engine.applyPatch(bad));
    bad=p;bad.ops[0].freqFine=100;CHECK(!engine.applyPatch(bad));
    CHECK(engine.applyPatch(p));
    CHECK(block()==0.0f);
    engine.noteOn(69,100);
    float energy=0.0f;
    for(int i=0;i<100;++i) energy+=block();
    CHECK(energy>0.01f);
    engine.panic();CHECK(block()==0.0f);
    // All routing paths, modulation extremes, highest notes and voice stealing.
    for(int algo=0;algo<12;++algo) {
        p=RdxEngine::initPatch();p.common.algorithm=algo;
        p.common.lfoPMD=127;p.common.pegLevel[0]=127;
        for(auto& op:p.ops) {op.outLevel=127;op.feedback=127;op.freqCoarse=31;op.freqFine=99;op.pegEnable=1;op.lfoPMDEnable=1;}
        CHECK(engine.applyPatch(p));
        for(int n=116;n<128;++n) engine.noteOn(n,127);
        for(int b=0;b<128;++b) block();
        engine.panic();CHECK(block()==0.0f);
    }
    // Release eventually reaches exact silence, including glide modes.
    for(int mode=0;mode<3;++mode) {
        p=RdxEngine::initPatch();p.common.monoPoly=mode;p.common.portaTime=80;
        p.ops[0].egRate[3]=127;
        CHECK(engine.applyPatch(p));
        engine.noteOn(60,100);for(int i=0;i<20;++i) block();
        engine.sustain(true);engine.noteOff(60);
        CHECK(block()>0.0f);
        engine.sustain(false);
        for(int i=0;i<1000;++i) block();
        CHECK(block()==0.0f);
    }
    // Identical reset + sequence must produce bit-identical samples.
    float reference[128];p=RdxEngine::initPatch();
    CHECK(engine.applyPatch(p));engine.noteOn(69,100);block();memcpy(reference,left,sizeof left);
    CHECK(engine.applyPatch(p));engine.noteOn(69,100);block();CHECK(memcmp(reference,left,sizeof left)==0);
    for(uint32_t i=0;i<RdxEngine::presetCount();++i) {
        CHECK(RdxEngine::presetPatch(i,p));CHECK(engine.applyPatch(p));
        engine.noteOn(60,100);engine.noteOn(67,120);
        for(int b=0;b<128;++b)block();
        engine.noteOff(60);engine.noteOff(67);
        for(int b=0;b<128;++b)block();
    }
    CHECK(!RdxEngine::presetPatch(RdxEngine::presetCount(),p));
    // Newest-note priority protects new slow attacks from a loud sustained note.
    p=RdxEngine::initPatch();
    for(int i=0;i<4;++i){p.ops[0].egRate[i]=127;p.ops[0].egLevel[i]=i==3?0:127;}
    CHECK(engine.applyPatch(p));engine.noteOn(60,127);for(int b=0;b<5;++b)block();
    p.ops[0].egRate[0]=0;CHECK(engine.updatePatch(p));
    engine.noteOn(62,127);engine.noteOn(64,127);engine.noteOn(65,127);for(int b=0;b<5;++b)block();
    engine.noteOn(67,127);engine.noteOff(62);engine.noteOff(64);engine.noteOff(65);engine.noteOff(67);
    for(int b=0;b<1000;++b)block();
    CHECK(block()==0.0f);
    // Outside attack, prefer a quiet voice over an older, louder sustained one.
    p=RdxEngine::initPatch();p.ops[0].velSens=127;
    for(int i=0;i<4;++i){p.ops[0].egRate[i]=127;p.ops[0].egLevel[i]=i==3?0:127;}
    CHECK(engine.applyPatch(p));engine.noteOn(60,127);engine.noteOn(62,1);engine.noteOn(64,127);engine.noteOn(65,127);
    for(int b=0;b<100;++b)block();
    engine.noteOn(67,127);engine.noteOff(60);engine.noteOff(64);engine.noteOff(65);engine.noteOff(67);
    for(int b=0;b<1000;++b)block();
    CHECK(block()==0.0f);
    std::printf("PASS: core bounds, 12 algorithms, 4-voice stress, release/sustain, determinism, %u source presets; engine=%zu bytes\n",RdxEngine::presetCount(),sizeof engine);
}
