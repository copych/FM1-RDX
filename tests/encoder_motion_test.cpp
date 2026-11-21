#include "../src/rdx_encoder.h"
#include <cassert>
#include <climits>
#include <cstdio>
int main(){
    RdxEncoderMotion state{};
    for(uint32_t t=0;t<2000;t+=200)assert(rdx_encoder_delta(&state,1,t,1)==1);
    rdx_encoder_reset(&state);
    assert(rdx_encoder_delta(&state,1,0,1)==1);
    assert(rdx_encoder_delta(&state,1,20,1)==1);
    assert(rdx_encoder_delta(&state,1,40,1)==8);
    assert(rdx_encoder_delta(&state,-1,60,1)==-1);
    assert(rdx_encoder_delta(&state,-1,80,1)==-1);
    assert(rdx_encoder_delta(&state,-1,100,1)==-8);
    assert(rdx_encoder_delta(&state,-1,300,1)==-1);
    rdx_encoder_reset(&state);
    rdx_encoder_delta(&state,1,0,1);rdx_encoder_delta(&state,1,50,1);
    assert(rdx_encoder_delta(&state,1,100,1)==4);
    rdx_encoder_reset(&state);
    rdx_encoder_delta(&state,1,0,1);rdx_encoder_delta(&state,1,100,1);
    assert(rdx_encoder_delta(&state,1,200,1)==2);
    assert(rdx_encoder_delta(&state,1,220,0)==1);
    assert(rdx_encoder_delta(&state,1,240,1)==1);
    rdx_encoder_reset(&state);
    rdx_encoder_delta(&state,1,UINT32_MAX-40,1);rdx_encoder_delta(&state,1,UINT32_MAX-20,1);
    assert(rdx_encoder_delta(&state,1,0,1)==8);
    assert(rdx_encoder_delta(&state,INT_MAX,20,1)==127);
    assert(rdx_encoder_delta(&state,INT_MIN,40,1)==-127);
    RdxEncoderMotion other{};assert(rdx_encoder_delta(&other,1,40,1)==1);
    rdx_encoder_reset(&state);
    assert(rdx_encoder_delta(&state,10,0,1)==10);
    assert(rdx_encoder_delta(&state,10,20,1)==10);
    assert(rdx_encoder_delta(&state,10,40,1)==80);
    std::puts("PASS: slow/2x/4x/8x motion, reversal, pause, fine/discrete override, independent history, batching, bounds and timer wrap.");
}
