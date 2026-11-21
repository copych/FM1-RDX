#pragma once
#include "rdx_engine.h"
enum eRenderPart { RENDER_PREP, RENDER_MOD, RENDER_ENV, RENDER_OP, RENDER_MIX, RENDER_PARTS_COUNT };
enum eRenderFinePart { RENDER_ENV_STEP, RENDER_ENV_GAIN, RENDER_OP_PITCH, RENDER_OP_WAVE, RENDER_FINE_PARTS_COUNT };
// Alternate diagnostic path. One static scratch set, owned by the audio caller.
struct RdxProfile {
    static void render(RdxEngine& engine,float* left,float* right,uint32_t* ticks,uint32_t(*clock)(),uint32_t* fine=nullptr);
private:
    static RDX_RAM_CODE uint32_t prepare(RdxEngine*,uint8_t*,float*);
    static RDX_RAM_CODE void mods(RDX_Voice*,uint32_t,bool);
    static RDX_RAM_CODE void envelopes(RDX_Voice*,uint32_t);
    static RDX_RAM_CODE void waves(RDX_Voice*,uint32_t);
    static RDX_RAM_CODE void envelopeSteps(RDX_Voice*,uint32_t);
    static RDX_RAM_CODE void envelopeGains(uint32_t);
    static RDX_RAM_CODE void pitches(RDX_Voice*,uint32_t);
    static RDX_RAM_CODE void preparedWaves(RDX_Voice*,uint32_t);
    static RDX_RAM_CODE void mix(RdxEngine*,uint32_t,uint32_t,float*);
    static RDX_RAM_CODE void finish(RdxEngine*,float*,float*);
};
