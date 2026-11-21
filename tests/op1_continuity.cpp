#include "rdx_engine.h"
#include <cmath>
#include <cstdio>
#include <vector>
static std::vector<float> render(bool modulation){
    RdxEngine engine;RDX_Patch p{};RdxEngine::presetPatch(2,p);
    for(int i=1;i<4;++i)p.ops[i].enable=0;
    if(!modulation)p.ops[0].lfoAMD=0;
    engine.applyPatch(p);engine.noteOn(48,100);
    std::vector<float> out;float l[128],r[128];
    for(int b=0;b<1100;++b){engine.renderBlock(l,r);out.insert(out.end(),l,l+128);}
    return out;
}
int main(){
    const auto unmod=render(false),mod=render(true);float worst=0;unsigned at=0;
    for(unsigned i=1;i<mod.size();++i){
        // The carrier is unchanged: divide it out away from zero crossings.
        if(fabsf(unmod[i])<0.01f||fabsf(unmod[i-1])<0.01f)continue;
        const float jump=fabsf(mod[i]/unmod[i]-mod[i-1]/unmod[i-1]);
        if(jump>worst){worst=jump;at=i;}
    }
    std::printf("Isolated WobbleBass OP1 dry: maximum gain step %.9f at %.6fs (sample %u)\n",worst,at/44100.0,at);
    return worst>0.002f;
}
