#include "rdx_engine.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <initializer_list>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::abort();}}while(0)
template<bool Mono> static void compare(RDX_Voice& reference,RDX_Voice& routed){
    for(int b=0;b<160;++b){
        if(Mono&&b==20){reference.noteOn(65,110);routed.noteOn(65,110);}
        if(b==30){auto& p=RDX_State::getState().workingPatch;p.common.algorithm=(p.common.algorithm+1)%12;reference.cacheParams();routed.cacheParams();}
        if(b==45){uint8_t next=(RDX_State::getState().workingPatch.common.algorithm+1)%12;reference.setAlgorithm(next);routed.setAlgorithm(next);}
        if(b==55){auto& p=RDX_State::getState().workingPatch;p.ops[0].pegEnable^=1;p.ops[2].lfoPMDEnable^=1;p.ops[1].lfoAMD=127;reference.updateModParams();routed.updateModParams();}
        if(b==65){auto& p=RDX_State::getState().workingPatch;for(int n=0;n<4;++n){p.ops[n].pegEnable=1;p.ops[n].lfoPMDEnable=1;}reference.updateModParams();routed.updateModParams();}
        if(b==80){reference.noteOff();routed.noteOff();}
        reference.updateLfo();routed.updateLfo();
        for(int i=0;i<128;++i){float a=reference.step<Mono>(),v=routed.stepRouted<Mono>();CHECK(std::isfinite(a)&&std::isfinite(v));CHECK(!memcmp(&a,&v,sizeof a));}
    }
}
int main(){
    for(unsigned i=0;i<1000000;++i){float x=i*(36000.0f/1000000.0f);float a=wrap01(x),b=wrapPositive01(x);CHECK(!memcmp(&a,&b,sizeof a));}
    for(float x:{0.0f,0.99999994f,1.0f,1.00000012f,1.99999988f,2.0f,2.00000024f}){float a=wrap01(x),b=wrapPositive01(x);CHECK(!memcmp(&a,&b,sizeof a));}
    RdxEngine engine;unsigned cases=0;
    for(uint32_t preset=0;preset<RdxEngine::presetCount();++preset)for(int algorithm=0;algorithm<12;++algorithm)for(int mode=0;mode<3;++mode){
        RDX_Patch patch;CHECK(RdxEngine::presetPatch(preset,patch));patch.common.algorithm=algorithm;patch.common.monoPoly=mode;
        // PRNG sequence is shared process-wide. Continuous LFO provides the
        // same per-voice inputs; graph equivalence is independent of its shape.
        if(patch.common.lfoWave>=5)patch.common.lfoWave=0;
        CHECK(engine.applyPatch(patch));
        RDX_Voice reference,routed;reference.init();routed.init();reference.cacheParams();routed.cacheParams();
        reference.noteOn(48+(preset%3)*12,100);routed.noteOn(48+(preset%3)*12,100);
        if(mode)compare<true>(reference,routed);else compare<false>(reference,routed);
        ++cases;
    }
    std::printf("PASS: %u exact routing comparisons (33 presets x 12 algorithms x 3 modes), attack/hold/release\n",cases);
}
