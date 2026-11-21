#pragma once
#include <stdint.h>
// UI-only motion history. Empty polls retain history; pauses/reversals reset it.
typedef struct {uint32_t last_ms;int8_t direction;uint8_t runs;} RdxEncoderMotion;
static inline void rdx_encoder_reset(RdxEncoderMotion* s){s->last_ms=0;s->direction=0;s->runs=0;}
static inline int32_t rdx_encoder_delta(RdxEncoderMotion* s,int32_t delta,uint32_t now,int accelerate){
    if(delta>127)delta=127;
    if(delta<-127)delta=-127;
    if(!delta)return 0;
    if(!accelerate){rdx_encoder_reset(s);return delta;}
    const int8_t direction=delta>0?1:-1;
    const uint32_t elapsed=now-s->last_ms;
    const int continuous=s->direction==direction&&elapsed<=120u;
    s->runs=continuous?(s->runs<3?s->runs+1:3):1;
    s->last_ms=now;s->direction=direction;
    int32_t factor=1;
    if(s->runs>=3)factor=elapsed<=35u?8:elapsed<=70u?4:2;
    delta*=factor;
    return delta>127?127:delta<-127?-127:delta;
}
