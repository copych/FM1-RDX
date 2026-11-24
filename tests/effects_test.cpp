#include "rdx_engine.h"
#include "rdx_effects.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static RdxEffects fx;
struct GuardedArena {float before[16];float samples[RdxEffects::arenaFloats];float after[16];};
static GuardedArena memory;
static float left[128],right[128];
static void prepare(const RDX_Patch& p) {
    RDX_State::getState().workingPatch=p;
    CHECK(fx.begin(p));int count=0;
    while(!fx.serviceClear() && count++<24){}
    CHECK(count<24);
}
static void checkBlock() {
    fx.process(left,right);
    for(int i=0;i<128;++i)CHECK(std::isfinite(left[i])&&std::isfinite(right[i]));
    for(int i=0;i<16;++i)CHECK(memory.before[i]==1234.0f&&memory.after[i]==5678.0f);
}
int main() {
    for(int i=0;i<16;++i){memory.before[i]=1234.0f;memory.after[i]=5678.0f;}
    fx.init(memory.samples);
    uint32_t maximum=0;
    for(uint32_t i=0;i<RdxEngine::presetCount();++i) {
        RDX_Patch p{};CHECK(RdxEngine::presetPatch(i,p));
        uint32_t n=RdxEffects::required(p);if(n>maximum)maximum=n;
        prepare(p);
        for(int b=0;b<128;++b) {
            for(int j=0;j<128;++j){left[j]=b<16 ? 0.2f : 0.0f;right[j]=b<16 ? -0.1f : 0.0f;}
            checkBlock();
        }
    }
    // Exercise every effect, including touch wah, and parameter extremes.
    for(int id=EFFECT_THRU;id<EFFECTS_COUNT;++id)for(int param: {0,64,127}) {
        auto p=RdxEngine::initPatch();p.common.effects[0][0]=id;
        p.common.effects[0][1]=param;p.common.effects[0][2]=param;
        if(RdxEffects::required(p)>RdxEffects::arenaFloats){CHECK(!fx.begin(p));continue;}
        prepare(p);
        for(int b=0;b<384;++b){for(int j=0;j<128;++j){left[j]=(j&1)?0.9f:-0.9f;right[j]=left[j];}checkBlock();}
    }
    // Full-scale serial chains and tails, including calibrated makeup feeding slot 2.
    for(int a=EFFECT_THRU;a<EFFECTS_COUNT;++a)for(int b=EFFECT_THRU;b<EFFECTS_COUNT;++b){
        auto p=RdxEngine::initPatch();p.common.effects[0][0]=a;p.common.effects[1][0]=b;
        for(int k=0;k<2;++k)p.common.effects[k][1]=p.common.effects[k][2]=64;
        if(RdxEffects::required(p)>RdxEffects::arenaFloats){CHECK(!fx.begin(p));continue;}
        prepare(p);for(int block=0;block<384;++block){for(int i=0;i<128;++i){left[i]=block<32?((i&1)?4.0f:-4.0f):0.0f;right[i]=left[i]*0.5f;}checkBlock();}
    }
    auto bad=RdxEngine::initPatch();bad.common.effects[0][0]=255;CHECK(!fx.begin(bad));
    CHECK(RdxEffects::required(bad)>RdxEffects::arenaFloats);
    bad=RdxEngine::initPatch();bad.common.effects[0][0]=bad.common.effects[1][0]=EFFECT_DELAY;
    bad.common.effects[0][2]=bad.common.effects[1][2]=127;
    CHECK(!fx.begin(bad)); // reject; never shorten a delay to hide exhaustion
    std::printf("PASS: 33 preset FX chains, all 8 effect IDs, parameter bounds, arena canaries, budget rejection; max %u/%u floats\n",maximum,RdxEffects::arenaFloats);
}
