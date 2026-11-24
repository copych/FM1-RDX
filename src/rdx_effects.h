#pragma once
#include "fx/fx_distortion.h"
#include "fx/fx_touch_wah.h"
#include "fx/fx_chorus.h"
#include "fx/fx_flanger.h"
#include "fx/fx_phaser.h"
#include "fx/fx_delay.h"
#include "fx/fx_reverb.h"

enum eEffectId { EFFECT_THRU, EFFECT_DISTORTION, EFFECT_TOUCH_WAH, EFFECT_CHORUS,
    EFFECT_FLANGER, EFFECT_PHASER, EFFECT_DELAY, EFFECT_REVERB, EFFECTS_COUNT };

// One fixed arena shared by two serial slots. Delay rings use only the selected
// tap length; they retain the source's float storage and up-to-one-second timing.
class RdxEffects {
public:
    static constexpr uint32_t arenaFloats=82432;
    void init(float* arena);
    bool begin(const RDX_Patch& patch);
    bool serviceClear();
    void process(float* left,float* right,bool matchLevels=true);
    bool valid() const {return valid_;}
    uint32_t used() const {return used_;}
    static uint32_t required(const RDX_Patch& patch);
    float phaserValue(uint8_t parameter) const;
    void adjustPhaser(uint8_t parameter,int delta);
private:
    struct Slot {
        uint8_t id=EFFECT_THRU,levelP1=255,levelP2=255;
        float levelAt[4]={1.0f,1.0f,1.0f,1.0f},levelPower=0.0f,levelWetPower=0.0f,levelGain=1.0f;
        uint32_t offset=0,size=0;
        FxDistortion distortion;
        FxTouchWah wah;
        FxChorus chorus;
        FxFlanger flanger;
        FxPhaser phaser;
        FxDelay delay;
        FxReverb reverb;
    } slots_[2];
    float* arena_=nullptr;
    uint32_t used_=0,cleared_=0;
    bool prepared_=false,valid_=false;
    static uint32_t slotSize(uint8_t id,uint8_t time);
    bool prepare();
};
