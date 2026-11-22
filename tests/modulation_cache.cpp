// Compile against either checkpoint/current engine; compare separate-process
// streams so the original integer-flag/LUT path is an independent reference.
#include "rdx_engine.h"
#include <cstdio>
#include <cstdlib>
static RdxEngine engine;
int main(int argc,char** argv){
    if(argc!=2)return 2;
    FILE* out=std::fopen(argv[1],"wb");if(!out)return 3;
    float left[128],right[128];
    for(unsigned preset=0;preset<33;++preset)for(unsigned mode=0;mode<3;++mode){
        RDX_Patch p;if(!RdxEngine::presetPatch(preset,p))return 4;
        p.common.monoPoly=mode;if(!engine.applyPatch(p))return 5;
        engine.noteOn(48,100);engine.noteOn(55,110);engine.noteOn(60,90);
        for(unsigned block=0;block<64;++block){
            if(block==8||block==16||block==24||block==32||block==40){
                for(unsigned op=0;op<4;++op){
                    p.ops[op].pegEnable=block==24?1:block==16?(op&1u):0;
                    p.ops[op].lfoPMDEnable=block==24?1:block==16?((op+1u)&1u):0;
                    p.ops[op].lfoAMD=block==24?127:block==16?34+op*31:block==40?1:0;
                }
                if(!engine.updatePatch(p))return 6;
            }
            auto& controls=RDX_State::getState().controls;
            controls.modWheelFactor=block<20?0.0f:0.75f;
            controls.pitchbendSemitones=block<30?0.0f:-7.25f;
            if(block==48)engine.allNotesOff();
            if(block==56)engine.noteOn(67,105);
            engine.renderBlock(left,right);
            if(std::fwrite(left,sizeof left,1,out)!=1||std::fwrite(right,sizeof right,1,out)!=1)return 7;
        }
    }
    std::fclose(out);return 0;
}
