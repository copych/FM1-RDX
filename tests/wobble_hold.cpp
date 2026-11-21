#include "engine_api.h"
#include "../src/core/RDX_Constants.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
static float l[128],r[128];
static void block(){rdx_render(l,r);for(int i=0;i<128;++i)if(!std::isfinite(l[i])||!std::isfinite(r[i])||fabsf(l[i])>1.0f||fabsf(r[i])>1.0f)std::abort();}
int main(){
    // wrap01(-tiny) rounds to 1.0f: interpolation must wrap, not read [1025].
    if(sin01(1.0f)!=sin01(0.0f)||sin01(wrap01(-1.0e-9f))!=sin01(0.0f))std::abort();
    for(int fx=0;fx<2;++fx)for(int note=36;note<=72;note+=12){
        rdx_boot();rdx_preset_request(2);for(int b=0;b<100;++b)block();
        if(!fx)rdx_fx_toggle();rdx_note_on(note,100);
        float previous=0,peak=0,jump=0,boundary=0;unsigned at=0;
        for(unsigned b=0;b<10336;++b){block();for(unsigned i=0;i<128;++i){
            float d=fabsf(l[i]-previous);previous=l[i];if(fabsf(l[i])>peak)peak=fabsf(l[i]);
            if(d>jump){jump=d;at=b*128+i;}if(i==0&&d>boundary)boundary=d;
        }}
        std::printf("WobbleBass note=%d FX=%d 30s peak=%.6f max_delta=%.6f at=%.3fs block_boundary_delta=%.6f\n",note,fx,peak,jump,at/44100.0,boundary);
        rdx_note_off(note);for(int b=0;b<1000;++b)block();
    }
}
