// Adapted from the user snapshot; original notices below are retained.
/*
 * FxDelay - Delay audio effect
 * Based on Michael Licence's delay code
 *
 * Author: Evgeny Aslovskiy AKA Copych
 * License: MIT
 */

#pragma once

#include "../core/RDX_Constants.h"
#include "../core/misc.h" 
#include "fx_base.h"

enum class DelayTimeDiv : uint8_t {
    Whole = 0,
    Half,
    Quarter,
    Eighth,
    Sixteenth,
    Triplet8th,
    Dotted8th,
    Custom = 255
};

enum class DelayMode : uint8_t {
    Normal = 0,
    PingPong
};

class  FxDelay : public FXBase {
public:
    FxDelay() = default;
    void setCapacity(uint32_t n) {MAX_DELAY=n;}

    bool prepare(float* scratchFast, uint32_t fastSize, float* scratchSlow, uint32_t slowSize, int sampleRate) {
        sampleRate_ = sampleRate;

        // need space for 2 * MAX_DELAY samples
        if (slowSize < MAX_DELAY * 2) return false;

        delayLine_l_ = scratchSlow;
        delayLine_r_ = scratchSlow + MAX_DELAY;

        delayIn_ = 0;
        delayFeedback_ = 0.2f;
        delayLen_ = SAMPLE_RATE / 4;
        mode_ = DelayMode::Normal;
        prepared_ = true;
        return true;
    }

    inline void reset() {
        if (prepared_) {
            delayIn_ = 0;
            delayFeedback_ = 0.2f;
            delayLen_ = SAMPLE_RATE / 4;
            mode_ = DelayMode::Normal;
        }
    }

    inline void processBlock(float* left, float* right, uint32_t frames) {
        if (!prepared_) return;

        setFbParam( st.effects[slotId_][1] ) ;
        setTimeParam( st.effects[slotId_][2] ); 

    //    setMode(modeParam > 63 ? DelayMode::PingPong : DelayMode::Normal);

        for (uint32_t i = 0; i < frames; ++i) {
            uint32_t outIndex = (delayIn_ + MAX_DELAY - delayLen_) ;
            if (outIndex >= MAX_DELAY) outIndex -= MAX_DELAY;
            const float outL = delayLine_l_[outIndex];
            const float outR = delayLine_r_[outIndex];

            if (mode_ == DelayMode::PingPong) {
                delayLine_l_[delayIn_] = left[i]  + outR * delayFeedback_;
                delayLine_r_[delayIn_] = right[i] + outL * delayFeedback_;
            } else {
                delayLine_l_[delayIn_] = left[i]  + outL * delayFeedback_;
                delayLine_r_[delayIn_] = right[i] + outR * delayFeedback_;
            }

            // simple dry-wet mix
            left[i]  = left[i]  * (1.0f - MIX) + outL * MIX;
            right[i] = right[i] * (1.0f - MIX) + outR * MIX;

            delayIn_++;
            if (delayIn_ >= MAX_DELAY) delayIn_ -= MAX_DELAY;
        }
    }

    inline void setslotId_(int idx) { slotId_ = idx; }

    inline void setFeedback(float value) { delayFeedback_ = fclamp(value, 0.0f, 0.95f); }

    inline void setDelayTime(DelayTimeDiv div, float bpm) {
        float secondsPerBeat = 60.0f / bpm;
        float seconds = secondsPerBeat;

        switch (div) {
            case DelayTimeDiv::Whole:      seconds *= 4.0f; break;
            case DelayTimeDiv::Half:       seconds *= 2.0f; break;
            case DelayTimeDiv::Quarter:    seconds *= 1.0f; break;
            case DelayTimeDiv::Eighth:     seconds *= 0.5f; break;
            case DelayTimeDiv::Sixteenth:  seconds *= 0.25f; break;
            case DelayTimeDiv::Triplet8th: seconds *= (1.0f / 3.0f); break;
            case DelayTimeDiv::Dotted8th:  seconds *= 0.75f; break;
            case DelayTimeDiv::Custom:     return;
        }

        setCustomLength(seconds);
    }

    inline void setCustomLength(float seconds) {
        delayLen_ = fclamp((uint32_t)(seconds * sampleRate_), 1, SAMPLE_RATE - 1);
    }

    inline void setMode(DelayMode m) { mode_ = m; }

    inline float getFeedback() const { return delayFeedback_; }
    inline float getDelayTimeSec() const { return delayLen_ / (float)sampleRate_; }
    inline DelayMode getMode() const { return mode_; }

    inline void setFbParam(int fb) {
        if (fb == fbParam_) return;
        setFeedback(fb * MIDI_NORM * 0.5f); // 0 .. 0.5f
        MIX = (0.1f + fb * MIDI_NORM * 0.15f); // 0.1f .. 0.25f
    }

    inline void setTimeParam(int t) {
        if (t == timeParam_) return;
        setCustomLength(DELAY_TIME_MS[t] * 0.001f); // LUTed
    }

private:
    int timeParam_ = 64;
    int fbParam_ = 64;
    float MIX = 0.14f;
    RDX_Common& st = RDX_State::getState().workingPatch.common ;

uint32_t MAX_DELAY = 2; // ring storage only; tap timing retains the source limit

    float* delayLine_l_ = nullptr;
    float* delayLine_r_ = nullptr;

    float delayFeedback_ = 0.2f;
    uint32_t delayLen_ = SAMPLE_RATE / 4;
    uint32_t delayIn_ = 0;
    DelayMode mode_ = DelayMode::Normal;
};
