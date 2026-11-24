#pragma once
#include <stdint.h>

// One-block stereo-linked lookahead, after effects/click and before MASTER.
// Start gain is already safe for the delayed block; the ramp endpoint is safe
// for both that block and the arriving block. No sample can exceed the ceiling.
enum { RDX_OUTPUT_FRAMES=128 };
typedef struct {float delay[2][RDX_OUTPUT_FRAMES],gain,previous_required;} RdxOutput;
static void rdx_output_init(RdxOutput* s){memset(s,0,sizeof *s);s->gain=s->previous_required=1.0f;}
static void rdx_output_process(RdxOutput* s,float* l,float* r,uint32_t n){
    const float ceiling=0.8912509f;
    if(n!=RDX_OUTPUT_FRAMES){for(uint32_t i=0;i<n;++i)l[i]=r[i]=0.0f;return;}
    float peak=0.0f;
    for(uint32_t i=0;i<n;++i){
        float a=l[i],b=r[i];
        if(!(a>=-3.4028234e38f&&a<=3.4028234e38f))a=0.0f;
        if(!(b>=-3.4028234e38f&&b<=3.4028234e38f))b=0.0f;
        l[i]=a*0.90f;r[i]=b*0.90f;
        const float x=l[i]<0.0f?-l[i]:l[i],y=r[i]<0.0f?-r[i]:r[i];
        if(x>peak)peak=x;if(y>peak)peak=y;
    }
    const float required=peak>ceiling?ceiling/peak:1.0f;
    const float target=required<s->previous_required?required:s->previous_required;
    float end=s->gain+(1.0f-s->gain)*0.0144f;if(end>target)end=target;
    const float step=(end-s->gain)*(1.0f/128.0f);float gain=s->gain;
    for(uint32_t i=0;i<n;++i){
        const float a=l[i],b=r[i];
        l[i]=s->delay[0][i]*gain;r[i]=s->delay[1][i]*gain;
        s->delay[0][i]=a;s->delay[1][i]=b;gain+=step;
    }
    s->gain=end;s->previous_required=required;
}
