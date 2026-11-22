#pragma once
#include "rdx_platform.h"
#include "control_tables.h"

inline float fclamp(float x, float lo, float hi) {
    return x > hi ? hi : x >= lo ? x : lo;
}
inline __attribute__((always_inline)) float fast_floorf(float x) {
    const int i = static_cast<int>(x);
    return static_cast<float>(i - (static_cast<float>(i) > x));
}
inline __attribute__((always_inline)) float wrap01(float x) { return x - fast_floorf(x); }
// Oscillator increments are nonnegative. Most advances need no integer
// conversion; retain the general wrap for extreme pitch/frequency settings.
inline __attribute__((always_inline)) float wrapPositive01(float x){
    if(x<1.0f)return x;
    if(x<2.0f)return x-1.0f;
    return wrap01(x);
}

// Note-on only, before rendering. Fractional transposition is preserved.
inline float midiNoteToHz(float note) {
    const float index=fclamp(note+24.0f,0.0f,176.0f);
    const int i=static_cast<int>(index);
    if(i==176) return noteHz[176];
    return noteHz[i]+(index-i)*(noteHz[i+1]-noteHz[i]);
}
