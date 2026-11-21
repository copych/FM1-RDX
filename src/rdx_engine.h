#pragma once
#include "core/RDX_Voice.h"

// One engine per process: inherited voices refer to RDX_State's singleton.
// All calls require the same owner. A device adapter must publish controls to
// that owner, rather than mutate voices concurrently from the UI.
class RdxEngine {
public:
    static constexpr int voiceCapacity = 4; // provisional; measure on FM-1
    bool applyPatch(const RDX_Patch& patch);
    bool updatePatch(const RDX_Patch& patch);
    void noteOn(uint8_t note, uint8_t velocity);
    void noteOff(uint8_t note);
    void sustain(bool down);
    void allNotesOff();
    void panic();
    void renderBlock(float* left, float* right);
    static RDX_Patch initPatch();
    static bool validPatch(const RDX_Patch& patch);
    static uint32_t presetCount();
    static bool presetPatch(uint32_t index, RDX_Patch& patch);
private:
    void updateDerived();
    RDX_Voice voices_[voiceCapacity];
    uint8_t voiceAge_[voiceCapacity]{};
    uint8_t heldNotes_[16]{};
    uint8_t heldVelocity_[16]{};
    uint8_t heldCount_ = 0;
    float outputGain_ = 0.1f;
    bool ready_ = false;
};
