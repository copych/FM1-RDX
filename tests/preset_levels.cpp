#include "engine_api.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
int main(){
  float l[128],r[128];
  std::puts("preset,name,wet,rms,peak");
  for(unsigned p=0;p<rdx_preset_count();++p)for(int wet=0;wet<2;++wet){
    double energy=0;float peak=0;unsigned count=0;char name[11];
    for(unsigned note: {48u,60u,72u}){
      rdx_boot();rdx_preset_request(p);if(!wet)rdx_fx_toggle();
      for(int b=0;b<30;++b)rdx_render(l,r);
      rdx_note_on(note,100);
      for(int b=0;b<345;++b){rdx_render(l,r);for(int i=0;i<128;++i){
        if(!std::isfinite(l[i])||!std::isfinite(r[i]))return 2;
        if(b>=69){energy+=(double)l[i]*l[i]+(double)r[i]*r[i];count+=2;}
        peak=fmaxf(peak,fmaxf(fabsf(l[i]),fabsf(r[i])));
      }}
    }
    rdx_preset_name(p,name);std::printf("%u,%s,%d,%.9g,%.9g\n",p,name,wet,std::sqrt(energy/count),peak);
  }
}
