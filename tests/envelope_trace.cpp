#include <cstdio>
#include <cstdlib>
#include <cstring>
#if RDX_REFERENCE
#include "RDX_Envelope.h"
#include "RDX_PEG.h"
#else
#include "core/RDX_Envelope.h"
#include "core/RDX_PEG.h"
#endif
#include "preset_bank.h"
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    FILE* out=std::fopen(argv[1],"w");if(!out)return 2;
    const int holds[]={44,1323,13230};
    for(int patch=0;patch<33;++patch)for(int op=0;op<4;++op)for(int mode=0;mode<3;++mode)for(int profile=0;profile<3;++profile) {
        auto& state=RDX_State::getState();std::memcpy(&state.workingPatch,presetData[patch],150);
        state.workingPatch.common.monoPoly=mode;
        RDX_Envelope aeg;RDX_PEG peg;
        const auto& p=state.workingPatch;
        aeg.initAEG(p.ops[op].egRate,p.ops[op].egLevel);
        peg.initPEG(p.common.pegRate,p.common.pegLevel);
        aeg.gate(true);peg.gate(true);
        int aBefore=-1,pBefore=-1;
        const int id=(((patch*4+op)*3+mode)*3+profile);
        for(int sample=0;sample<holds[profile]+176400;++sample) {
            if(sample==holds[profile]){aeg.gate(false);peg.gate(false);}
            const float a=aeg.processAEG(),v=peg.processPEG();
            const int as=(int)aeg.getStage(),ps=(int)peg.getStage();
            if(as!=aBefore)std::fprintf(out,"A,%d,%d,%d,%.9g\n",id,sample,as,a);
            if(ps!=pBefore)std::fprintf(out,"P,%d,%d,%d,%.9g\n",id,sample,ps,v);
            if((sample&2047)==0)std::fprintf(out,"S,%d,%d,%.9g,%.9g\n",id,sample,a,v);
            aBefore=as;pBefore=ps;
        }
    }
    std::fclose(out);return 0;
}
