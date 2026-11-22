#pragma once
#include "core/RDX_Voice.h"

// One engine per process: inherited voices refer to RDX_State's singleton.
// All calls require the same owner. A device adapter must publish controls to
// that owner, rather than mutate voices concurrently from the UI.
class RdxEngine {
public:
    static constexpr int voiceCapacity = 8;
    void setVoiceLimit(uint8_t limit);
    uint8_t voiceLimit() const;
    uint8_t activeVoices() const;
    uint8_t renderedVoices() const {return rendered_;}
    bool applyPatch(const RDX_Patch& patch);
    bool updatePatch(const RDX_Patch& patch);
    void noteOn(uint8_t note, uint8_t velocity);
    void noteOff(uint8_t note);
    void sustain(bool down);
    void allNotesOff();
    void panic();
    RDX_RAM_CODE void renderBlock(float* left, float* right);
    // Exclusive voice owner generates individual dry buses; consumer mixes
    // only after publication. Original voice order and sample arithmetic remain.
    RDX_RAM_CODE void renderDry(float (*dry)[DMA_BUFFER_LEN]);
    static RDX_RAM_CODE void mixDry(const float (*dry)[DMA_BUFFER_LEN],float gain,float* left,float* right);
    float outputGain() const {return outputGain_;}
    static RDX_Patch initPatch();
    static bool validPatch(const RDX_Patch& patch);
    static uint32_t presetCount();
    static bool presetPatch(uint32_t index, RDX_Patch& patch);
private:
    friend struct RdxProfile;
    void updateDerived();
    int cheapestVoice(bool skipRetiring);
    RDX_Voice voices_[voiceCapacity];
    uint8_t retiring_[voiceCapacity]{};
    uint8_t limit_=4,rendered_=0;
    uint8_t voiceAge_[voiceCapacity]{};
    uint8_t heldNotes_[16]{};
    uint8_t heldVelocity_[16]{};
    uint8_t heldCount_ = 0;
    float outputGain_ = 0.1f;
    bool ready_ = false;
    bool mono_ = false;
};
