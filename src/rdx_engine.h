#pragma once
#include "core/RDX_Voice.h"

// One shared voice pool; live and captured phrase contexts bind at allocation.
// All calls require the same owner. A device adapter must publish controls to
// that owner, rather than mutate voices concurrently from the UI.
class RdxEngine {
public:
    static constexpr int voiceCapacity = 8;
    enum eVoiceSource { VOICE_LIVE,VOICE_PHRASE,VOICE_SOURCES_COUNT };
    bool capturePhrase(const RDX_Patch& patch);
    void clearPhrase();
    bool hasPhrase()const{return phraseReady_;}
    void phraseNoteOn(uint8_t note,uint8_t velocity){noteOnSource(VOICE_PHRASE,note,velocity);}
    void phraseNoteOff(uint8_t note){noteOffSource(VOICE_PHRASE,note);}
    void phraseSustain(bool down){sustainSource(VOICE_PHRASE,down);}
    void phrasePerformance(float bend,float mod){phraseControls_.pitchbendSemitones=bend;phraseControls_.modWheelFactor=mod;}
    int phraseBendRange()const{return (int)phrasePatch_.common.pbRange-64;}
    void releasePhrase();
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
    void derive(RDX_Patch& patch,RDX_Controls& controls);
    void panicLive();
    void noteOnSource(eVoiceSource source,uint8_t note,uint8_t velocity);
    void noteOffSource(eVoiceSource source,uint8_t note);
    void sustainSource(eVoiceSource source,bool down);
    RDX_Patch& patchFor(eVoiceSource source){return source==VOICE_PHRASE?phrasePatch_:RDX_State::getState().workingPatch;}
    RDX_Controls& controlsFor(eVoiceSource source){return source==VOICE_PHRASE?phraseControls_:RDX_State::getState().controls;}
    inline __attribute__((always_inline)) bool slotMono(int slot)const{return sources_[slot]==VOICE_PHRASE?phrasePatch_.common.monoPoly!=RDX_MODE_POLY:mono_;}
    inline __attribute__((always_inline)) uint8_t slotCount()const{return mono_&&!phraseReady_?1:voiceCapacity;}
    RDX_Patch phrasePatch_{};
    RDX_Controls phraseControls_{};
    bool phraseReady_=false;
    float phraseRatio_=1.0f;
    eVoiceSource sources_[voiceCapacity]{};
    uint8_t phraseHeld_[16]{},phraseVelocity_[16]{},phraseCount_=0;
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
