#include <cstdio>
#include <cstdlib>
#include <cstring>
#if RDX_REFERENCE
#include "RDX_Voice.h"
#include "fx_distortion.h"
#include "fx_touch_wah.h"
#include "fx_chorus.h"
#include "fx_flanger.h"
#include "fx_phaser.h"
#include "fx_delay.h"
#include "fx_reverb.h"
struct FxSet {
    FxThru thru;FxDistortion distortion;FxTouchWah wah;FxChorus chorus;
    FxFlanger flanger;FxPhaser phaser;FxDelay delay;FxReverb reverb;
    FXBase* get(int id) {
        switch(id) {case 1:return &distortion;case 2:return &wah;case 3:return &chorus;
        case 4:return &flanger;case 5:return &phaser;case 6:return &delay;case 7:return &reverb;default:return &thru;}
    }
};
static FxSet effects[2];
static float buffers[2][2][100000];
static FXBase* selected[2];
static RDX_Voice voices[4];
#else
#include "rdx_engine.h"
#ifdef RDX_PROFILE_COMPARE
#include "rdx_profile.h"
static uint32_t profile_clock(){return 0;}
#endif
#include "rdx_effects.h"
static RdxEngine engine;
static RdxEffects effects;
static float arena[RdxEffects::arenaFloats];
#endif
#include "preset_bank.h"
static float left[128],right[128];
static float out[256];
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    const bool wet=std::strcmp(argv[1],"wet")==0;
    FILE* f=std::fopen(argv[2],"wb");if(!f)return 2;
#if !RDX_REFERENCE
    effects.init(arena);
#endif
    for(unsigned p=0;p<33;++p) {
        RDX_Patch patch{};std::memcpy(&patch,presetData[p],sizeof patch);
#if RDX_REFERENCE
        auto& s=RDX_State::getState();s.controls=RDX_Controls{};s.workingPatch=patch;
        s.controls.portaTimeS=AM_DEPTH[patch.common.portaTime]*2.5f;
        for(int op=0;op<4;++op) {
            const auto& q=patch.ops[op];
            s.controls.freqCoef[op]=q.freqMode==0 ? (q.freqCoarse ? q.freqCoarse+q.freqFine*0.01f : 0.5f+q.freqFine*0.005f)
                : powf(10.0f,fclamp(q.freqCoarse>>3,0.0f,3.0f))*powf(9.772f,q.freqFine*0.01010101f);
            s.controls.detuneCoef[op]=powf(1.00033913f,(float)((int)q.freqDetune-64));
        }
        for(auto& v:voices){v.init();v.cacheParams();v.syncLFO();}
        const int algo=patch.common.algorithm;
        const float gain=0.2f/sqrtf(8.0f)*(algo>=5&&algo<=7 ? ONE_DIV_SQRT2 : algo>=8&&algo<=10 ? ONE_DIV_SQRT3 : algo==11 ? 0.5f : 1.0f);
        if(wet)for(int slot=0;slot<2;++slot) {
            std::memset(buffers[slot],0,sizeof buffers[slot]);
            // Reconstruct the reference effect for reproducible first-load state.
            effects[slot].~FxSet();new(&effects[slot]) FxSet;
            selected[slot]=effects[slot].get(patch.common.effects[slot][0]);
            selected[slot]->init(44100.0f,slot);
            if(!selected[slot]->prepare(buffers[slot][0],100000,buffers[slot][1],100000,44100))return 3;
            selected[slot]->reset();selected[slot]->enable(true);
        }
#else
        if(!engine.applyPatch(patch))return 3;
        // Compare effects separately from oscillator/envelope approximation.
        if(wet) {
            if(!effects.begin(patch)){std::fprintf(stderr,"FX memory failure on preset %u\n",p);return 4;}
            int cleared=0;while(!effects.serviceClear()&&cleared++<24){}
            if(cleared>=24){std::fprintf(stderr,"FX prepare failure on preset %u\n",p);return 4;}
        }
#endif
        for(int b=0;b<768;++b) {
            if(wet) {
                for(int i=0;i<128;++i) {int n=b*128+i;left[i]=n<4096 ? ((n%37)-18)*0.007f : 0.0f;right[i]=n<4096 ? ((n%29)-14)*0.006f : 0.0f;}
#if RDX_REFERENCE
                for(int slot=0;slot<2;++slot)selected[slot]->processBlock(left,right,128);
#else
                effects.process(left,right);
#endif
            } else {
#if RDX_REFERENCE
                if(b==0)voices[0].noteOn(60,100);
                const bool mono=patch.common.monoPoly!=0;const int count=mono?1:4;
                for(int i=0;i<count;++i)voices[i].updateLfo();
                for(int i=0;i<128;++i) {float sample=0.0f;for(int v=0;v<count;++v)sample+=(mono?voices[v].step<true>():voices[v].step<false>())*gain;
                    const float a=fabsf(sample);if(a>0.9f)sample=copysignf(0.9f+0.1f*(a-0.9f)/(0.1f+a-0.9f),sample);
                    left[i]=right[i]=sample;
                }
#else
                if(b==0)engine.noteOn(60,100);
#ifdef RDX_PROFILE_COMPARE
                uint32_t parts[RENDER_PARTS_COUNT];
#ifdef RDX_FINE_COMPARE
                uint32_t fine[RENDER_FINE_PARTS_COUNT];RdxProfile::render(engine,left,right,parts,profile_clock,fine);
#else
                RdxProfile::render(engine,left,right,parts,profile_clock);
#endif
#else
                engine.renderBlock(left,right);
#endif
#endif
            }
            for(int i=0;i<128;++i){out[2*i]=left[i];out[2*i+1]=right[i];}
            if(std::fwrite(out,sizeof(float),256,f)!=256)return 5;
        }
    }
    std::fclose(f);return 0;
}
