#pragma once
#include <stdint.h>
#if defined(RDX_FM1) && !defined(PLAT_HOST)
// fm1-base is freestanding; its BSP supplies memcpy. No target C++ runtime.
extern "C" void* memcpy(void*, const void*, unsigned int);
extern "C" void* memset(void*, int, unsigned int);
inline float fabsf(float x) { return __builtin_fabsf(x); }
inline float copysignf(float x, float y) { return __builtin_copysignf(x,y); }
inline void* operator new(decltype(sizeof(0)), void* p) noexcept {return p;}
#else
#include <string.h>
#include <math.h>
#include <new>
#endif

// A plain aggregate avoids a target C++ standard-library dependency.
template<class T, uint32_t N> struct RdxArray {
    T values[N];
    constexpr const T& operator[](uint32_t i) const { return values[i]; }
    T& operator[](uint32_t i) { return values[i]; }
};

// Engine configuration; these are not a claim about the hardware DMA cadence.
constexpr uint32_t SAMPLE_RATE = 44100;
constexpr uint32_t DMA_BUFFER_LEN = 128;
