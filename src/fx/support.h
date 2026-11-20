#pragma once
#include "../core/RDX_Constants.h"
#include "../core/RDX_State.h"
#include "coefficients.h"
constexpr float RDX_PI=3.14159265358979323846f;
constexpr float RDX_TWO_PI=6.28318530717958647692f;
template<class T> inline T rdxMin(T a,T b) {return a<b ? a : b;}
inline float saturate_cubic(float x) {
    if(x>1.0f)return 1.0f;
    if(x<-1.0f)return -1.0f;
    return 1.5f*(x-(x*x*x)*0.3334f);
}
