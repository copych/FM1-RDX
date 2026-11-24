#include <cassert>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <limits>
#include "../src/rdx_output.h"
#include "../src/rdx_level_math.h"
static RdxOutput s;
static float l[128],r[128];
static void fill(float x){for(int i=0;i<128;++i){l[i]=x;r[i]=-x*0.5f;}}
static void check(){for(int i=0;i<128;++i){assert(std::isfinite(l[i])&&std::isfinite(r[i]));assert(std::fabs(l[i])<=0.89126f&&std::fabs(r[i])<=0.89126f);assert(std::fabs(r[i]+l[i]*0.5f)<0.00001f);}}
int main(){
 for(int i=0;i<=100000;++i){float x=0.0625f+(16.0f-0.0625f)*(float)i/100000.0f;float reference=1.0f/std::sqrt(x);assert(std::fabs(rdx_level_inverse_sqrt(x)/reference-1.0f)<0.00001f);}
 rdx_output_init(&s);fill(0.25f);rdx_output_process(&s,l,r,128);assert(l[64]==0.0f);
 fill(0.25f);rdx_output_process(&s,l,r,128);assert(std::fabs(l[64]-0.225f)<0.000001f);check();
 for(int b=0;b<2000;++b){float a=(b%7==0?30.0f:b%3==0?1.7f:0.03f);fill(a);rdx_output_process(&s,l,r,128);check();}
 for(int b=0;b<3000;++b){fill(0.1f);rdx_output_process(&s,l,r,128);check();}assert(s.gain>0.9999f);
 rdx_output_init(&s);fill(0.0f);l[0]=std::numeric_limits<float>::infinity();r[0]=std::numeric_limits<float>::quiet_NaN();rdx_output_process(&s,l,r,128);fill(0.0f);rdx_output_process(&s,l,r,128);check();assert(l[0]==0.0f&&r[0]==0.0f);
 fill(0.5f);rdx_output_process(&s,l,r,64);for(int i=0;i<64;++i)assert(l[i]==0.0f&&r[i]==0.0f);
 puts("PASS output lookahead delay, unity-region scaling, stereo linking, step/sustained overload ceiling, release, nonfinite and frame bounds");
}
