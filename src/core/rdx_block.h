#pragma once
#include "rdx_platform.h"
constexpr uint32_t RDX_RENDER_CHUNK=32;
// Single audio-owner scratch, reused across voices/chunks. No control-rate hold:
// each lane contains an independently evaluated sample's modulation or output.
struct RdxVoiceBlockScratch {
    float ratio[4][RDX_RENDER_CHUNK];
    float amp[4][RDX_RENDER_CHUNK];
    float wave[4][RDX_RENDER_CHUNK];
    float input[RDX_RENDER_CHUNK];
};
