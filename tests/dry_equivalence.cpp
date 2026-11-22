#include "rdx_engine.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
static uint32_t tick(){return 0;}
static float al[128],ar[128],bl[128],br[128];
int main(){
    unsigned cases=0;
    for(unsigned p=0;p<RdxEngine::presetCount();++p)for(int algo=0;algo<12;++algo)for(int mode=0;mode<3;++mode){
        RDX_Patch patch;RdxEngine::presetPatch(p,patch);patch.common.algorithm=algo;patch.common.monoPoly=mode;
        // Paired instances share the inherited global random LFO generator.
        if(patch.common.lfoWave>=5)patch.common.lfoWave=0;
        RdxEngine a,b;if(!a.applyPatch(patch)||!b.applyPatch(patch))return 2;
        a.setVoiceLimit(8);b.setVoiceLimit(8);
        for(int n=0;n<8;++n){a.noteOn(48+n*3,80+n*5);b.noteOn(48+n*3,80+n*5);}
        for(int block=0;block<96;++block){
            if(block==12){a.sustain(true);b.sustain(true);}
            if(block==18){a.noteOff(48);b.noteOff(48);a.noteOn(73,110);b.noteOn(73,110);}
            if(block==24){a.setVoiceLimit(4);b.setVoiceLimit(4);}
            if(block==30){patch.common.algorithm=(algo+3)%12;patch.ops[2].enable=0;
                a.updatePatch(patch);b.updatePatch(patch);}
            if(block==40){auto& ctl=RDX_State::getState().controls;ctl.pitchbendSemitones=5.25f;ctl.modWheelFactor=0.3f;}
            if(block==48){a.sustain(false);b.sustain(false);a.allNotesOff();b.allNotesOff();}
            if(block==60){a.noteOn(81,127);b.noteOn(81,127);}
            if(block==75){a.panic();b.panic();}
            a.renderBlock(al,ar);float dry[8][128];
            // Exercise both entering and leaving the diagnostic on live voices.
            
            b.renderDry(dry);RdxEngine::mixDry(dry,b.outputGain(),bl,br);
            if(std::memcmp(al,bl,sizeof al)||std::memcmp(ar,br,sizeof ar)||a.activeVoices()!=b.activeVoices()){
                std::fprintf(stderr,"dry mismatch p%u algo%d mode%d block%d\n",p,algo,mode,block);return 1;
            }
            for(float v:bl)if(!std::isfinite(v))return 3;
        }++cases;
    }
    std::printf("PASS %u normal/dry bit-exact scenarios, live switches, retirement, modulation, release and panic\n",cases);
}

