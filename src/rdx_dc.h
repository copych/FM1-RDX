#pragma once
// 2 Hz at 44.1 kHz, unity gain at Nyquist. State is owned by audio CPU0.
// A leaky DC estimate avoids large cancellation in the output recurrence.
typedef struct {float mean[2];} RdxDc;
static inline float rdx_dc_sample(float x,float* mean){
    if(!(x>=-3.4028234e38f&&x<=3.4028234e38f))x=0.0f;
    const float d=x-*mean;*mean+=0.0002849111f*d;
    if(*mean<1.0e-20f&&*mean>-1.0e-20f)*mean=0.0f;
    return d*0.9998575445f;
}
static inline void rdx_dc_process(RdxDc* s,float* l,float* r,unsigned n){
    for(unsigned i=0;i<n;++i){l[i]=rdx_dc_sample(l[i],&s->mean[0]);r[i]=rdx_dc_sample(r[i],&s->mean[1]);}
}
