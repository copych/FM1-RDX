// Adapted from the user snapshot; original notices below are retained.
#pragma once
#include "support.h"
#include <stdint.h>
#include <stddef.h>

class  FXBase {
public:
    ~FXBase() {}
    void init(float sampleRate, uint8_t slot) {
        sampleRate_ = sampleRate; 
        slotId_ = slot; 
    }
    inline void reset() { 
        if (prepared_) {            
            enabled_ = true;
        } 
    }
    inline void   processBlock(float* left, float* right, uint32_t n) {}
    void setParam(uint8_t idx, float value) { (void)idx; (void)value; }
    bool prepare(float* scratchFast, uint32_t fastSize, float* scratchSlow, uint32_t slowSize, int sampleRate) {
        (void)scratchFast;
        (void)fastSize;
        (void)scratchSlow;
        (void)slowSize;
        (void)sampleRate;
        return true;
    }
    inline void enable(bool s) { enabled_ = s; }
    inline bool enabled() const { return enabled_; }

protected:
    bool enabled_ = false;
    bool prepared_ = false;
    float sampleRate_ = (float)SAMPLE_RATE;
    uint8_t slotId_ = 0;
};


class FxThru : public FXBase {
public:
    inline void processBlock(float* l, float* r, uint32_t n) {
        // pass thru
    }

};
 