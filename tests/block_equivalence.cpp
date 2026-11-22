#include "rdx_engine.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::abort();}}while(0)
template<bool Mono> static void compare(RDX_Voice& reference,RDX_Voice& block){
    RdxVoiceBlockScratch scratch;
    for(unsigned n=0;n<=RDX_RENDER_CHUNK;++n){
        if(n==9){reference.noteOff();block.noteOff();}
        if(n==17){reference.noteOn(65,110);block.noteOn(65,110);}
        if(n==11||n==23||n==25){
            auto& p=RDX_State::getState().workingPatch;
            p.ops[0].enable=n!=23;p.ops[1].pegEnable^=1;p.ops[2].lfoPMDEnable^=1;
            reference.cacheParams();block.cacheParams();
        }
        reference.updateLfo();block.updateLfo();
        for(unsigned op=0;op<4;++op)for(unsigned i=0;i<RDX_RENDER_CHUNK;++i){scratch.amp[op][i]=3.125f;scratch.ratio[op][i]=3.125f;scratch.wave[op][i]=3.125f;}
        for(float& v:scratch.input)v=3.125f;
        float expected[RDX_RENDER_CHUNK];
        for(unsigned i=0;i<n;++i)expected[i]=reference.stepRouted<Mono>();
        block.renderChunk(scratch,n,Mono);
        CHECK(!std::memcmp(expected,scratch.wave[0],n*sizeof(float)));
        for(unsigned op=0;op<4;++op)for(unsigned i=n;i<RDX_RENDER_CHUNK;++i)
            CHECK(scratch.amp[op][i]==3.125f&&scratch.ratio[op][i]==3.125f&&scratch.wave[op][i]==3.125f);
        for(unsigned i=n;i<RDX_RENDER_CHUNK;++i)CHECK(scratch.input[i]==3.125f);
    }
}
int main(){
    RdxEngine engine;unsigned cases=0;
    for(unsigned preset=0;preset<33;++preset)for(unsigned alg=0;alg<12;++alg)for(unsigned mode=0;mode<3;++mode){
        RDX_Patch p;CHECK(RdxEngine::presetPatch(preset,p));p.common.algorithm=alg;p.common.monoPoly=mode;
        if(p.common.lfoWave>=5)p.common.lfoWave=0; // independent-process streams cover original PRNG patches
        CHECK(engine.applyPatch(p));RDX_Voice reference,block;
        reference.init();block.init();reference.cacheParams();block.cacheParams();
        reference.noteOn(48,100);block.noteOn(48,100);
        if(mode)compare<true>(reference,block);else compare<false>(reference,block);
        ++cases;
    }
    std::printf("PASS: %u exact block cases, chunk lengths 0..32, tail bounds, live modes/gates/operator changes\n",cases);
}
