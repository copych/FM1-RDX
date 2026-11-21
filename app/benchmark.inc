// Main-loop diagnostic only. One 256-operation kernel per tick, IRQs enabled.
// No changes to the renderer, clock, interrupt masks or allocation policy.
#include "benchmark.h"
static float bench_ram[256];
static uint32_t bench_min[BENCH_CASES_COUNT],bench_max[BENCH_CASES_COUNT];
static uint32_t bench_case=BENCH_CASES_COUNT,bench_repeat;
static volatile uint32_t bench_sink;
#ifdef PLAT_HOST
// Browser/native hosts exercise controls and bounds, never target timings.
#define BENCH_OPAQUE(x) ((void)(x))
#else
#define BENCH_OPAQUE(x) __asm__ volatile("" : "+r"(x))
#endif
#define BENCH_FOUR(op) {op;} {op;} {op;} {op;}
static RDX_RAM_CODE uint32_t bench_kernel(uint32_t which,uint32_t seed) {
    float a=1.125f,b=2.25f,c=3.375f,d=4.5f,k=0.9999f;
    BENCH_OPAQUE(a);BENCH_OPAQUE(b);BENCH_OPAQUE(c);BENCH_OPAQUE(d);BENCH_OPAQUE(k);
    const float* table=which==BENCH_RAM||which==BENCH_RAM_INTERP?bench_ram:sinTable.values;
    BENCH_OPAQUE(table);
    uint32_t at=seed&255u,total=0;
    switch(which){
    case BENCH_EMPTY:
        for(uint32_t n=0;n<64;++n){BENCH_FOUR(BENCH_OPAQUE(a));}break;
    case BENCH_ADD_DEP:
        for(uint32_t n=0;n<64;++n){BENCH_FOUR(a+=k;BENCH_OPAQUE(a));}break;
    case BENCH_ADD_INDEP:
        for(uint32_t n=0;n<64;++n){a+=k;b+=k;c+=k;d+=k;BENCH_OPAQUE(a);BENCH_OPAQUE(b);BENCH_OPAQUE(c);BENCH_OPAQUE(d);}break;
    case BENCH_MUL_DEP:
        for(uint32_t n=0;n<64;++n){BENCH_FOUR(a*=k;BENCH_OPAQUE(a));}break;
    case BENCH_MUL_INDEP:
        for(uint32_t n=0;n<64;++n){a*=k;b*=k;c*=k;d*=k;BENCH_OPAQUE(a);BENCH_OPAQUE(b);BENCH_OPAQUE(c);BENCH_OPAQUE(d);}break;
    case BENCH_DIV_DEP:
        for(uint32_t n=0;n<64;++n){BENCH_FOUR(a/=k;BENCH_OPAQUE(a));}break;
    case BENCH_DIV_INDEP:
        for(uint32_t n=0;n<64;++n){a/=k;b/=k;c/=k;d/=k;BENCH_OPAQUE(a);BENCH_OPAQUE(b);BENCH_OPAQUE(c);BENCH_OPAQUE(d);}break;
    case BENCH_FLOAT_INT:
        for(uint32_t n=0;n<64;++n){BENCH_FOUR(int32_t v=(int32_t)a;BENCH_OPAQUE(v);total+=(uint32_t)v;BENCH_OPAQUE(a));}break;
    case BENCH_INT_FLOAT:
        for(uint32_t n=0;n<64;++n){BENCH_FOUR(float v=(float)at;BENCH_OPAQUE(v);a+=v;BENCH_OPAQUE(at));}break;
    case BENCH_FLASH:case BENCH_RAM:
        for(uint32_t n=0;n<64;++n){BENCH_FOUR(at=(at+73u)&255u;a+=table[at];BENCH_OPAQUE(a));}break;
    case BENCH_FLASH_INTERP:case BENCH_RAM_INTERP:
        for(uint32_t n=0;n<64;++n){BENCH_FOUR(at=(at+73u)&255u;float v=table[at];a+=v+0.375f*(table[(at+1u)&255u]-v);BENCH_OPAQUE(a));}break;
    case BENCH_PHASE_WRAP:
        a=0.123f;
        for(uint32_t n=0;n<64;++n){BENCH_FOUR(float x=a+k;int32_t i=(int32_t)x;a=x-(float)(i-((float)i>x));BENCH_OPAQUE(a));}break;
    case BENCH_PITCH:
        a=0.123f;
        for(uint32_t n=0;n<64;++n){BENCH_FOUR(a=semitonesToRatio(a);BENCH_OPAQUE(a));}break;
    default:break;
    }
    union {float f;uint32_t u;} bits;
    bits.f=a+b+c+d;return bits.u^total;
}
#undef BENCH_FOUR
#undef BENCH_OPAQUE
extern "C" void rdx_bench_start(void){
    for(uint32_t i=0;i<256;++i)bench_ram[i]=sinTable[i];
    for(uint32_t i=0;i<BENCH_CASES_COUNT;++i){bench_min[i]=UINT32_MAX;bench_max[i]=0;}
    bench_case=bench_repeat=0;
}
extern "C" void rdx_bench_tick(void){
    if(bench_case>=BENCH_CASES_COUNT)return;
    const uint32_t start=budget_ticks();
    const uint32_t value=RDX_FAR(bench_kernel)(bench_case,bench_repeat);
#ifdef PLAT_HOST
    (void)start;
    const uint32_t elapsed=0; // Host clocks cannot measure pi32v2 kernels.
#else
    const uint32_t elapsed=budget_ticks()-start;
#endif
    bench_sink=value;
    // First invocation warms each kernel; retain 32 subsequent samples.
    if(bench_repeat){
        if(elapsed<bench_min[bench_case])bench_min[bench_case]=elapsed;
        if(elapsed>bench_max[bench_case])bench_max[bench_case]=elapsed;
    }
    if(++bench_repeat==33u){bench_repeat=0;++bench_case;}
}
extern "C" const char* rdx_bench_name(uint32_t index){
    static const char* const names[]={"EMPTY LOOP","ADD DEP","ADD INDEP","MUL DEP","MUL INDEP","DIV DEP","DIV INDEP","FLOAT TO INT","INT TO FLOAT","FLASH READ","RAM READ","FLASH INTERP","RAM INTERP","PHASE WRAP","PITCH LUT"};
    return index<BENCH_CASES_COUNT?names[index]:"?";
}
extern "C" void rdx_bench_result(uint32_t index,uint32_t* out){
    out[0]=index<BENCH_CASES_COUNT&&bench_min[index]!=UINT32_MAX?bench_min[index]:0;
    out[1]=index<BENCH_CASES_COUNT?bench_max[index]:0;
    out[2]=bench_case;out[3]=bench_repeat;
}
