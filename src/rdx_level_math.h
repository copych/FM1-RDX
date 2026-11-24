#pragma once
#include <stdint.h>
// Bounded calibration helper, called once per wah block for a clamped
// power ratio in [1/16,16]. IEEE binary32 seed + two Newton iterations.
static inline float rdx_level_inverse_sqrt(float ratio){
    uint32_t bits;memcpy(&bits,&ratio,sizeof bits);bits=0x5f3759dfu-(bits>>1);
    float y;memcpy(&y,&bits,sizeof y);const float half=0.5f*ratio;
    y*=1.5f-half*y*y;y*=1.5f-half*y*y;return y;
}
