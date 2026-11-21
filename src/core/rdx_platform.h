#pragma once
#include <stdint.h>
#if defined(RDX_TEST_UNROLL) && defined(__clang__)
#define RDX_UNROLL4 _Pragma("clang loop unroll_count(4)")
#elif defined(RDX_TEST_UNROLL) && defined(__GNUC__)
#define RDX_UNROLL4 _Pragma("GCC unroll 4")
#else
#define RDX_UNROLL4
#endif
#if defined(RDX_FM1) && !defined(PLAT_HOST)
#define RDX_RAM_CODE __attribute__((section(".app_ram"),noinline))
template<class Fn> inline __attribute__((always_inline)) Fn rdx_far(Fn fn){Fn volatile held=fn;return held;}
#define RDX_FAR(fn) rdx_far(&(fn))
#else
#define RDX_RAM_CODE
#define RDX_FAR(fn) (&(fn))
#endif
#if defined(RDX_FM1) && !defined(PLAT_HOST)
// fm1-base is freestanding; its BSP supplies memcpy. No target C++ runtime.
extern "C" void* memcpy(void*, const void*, unsigned int);
extern "C" void* memset(void*, int, unsigned int);
inline __attribute__((always_inline)) float fabsf(float x) { return __builtin_fabsf(x); }
inline __attribute__((always_inline)) float copysignf(float x, float y) { return __builtin_copysignf(x,y); }
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
