#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum eBenchCase { BENCH_EMPTY, BENCH_ADD_DEP, BENCH_ADD_INDEP,
    BENCH_MUL_DEP, BENCH_MUL_INDEP, BENCH_DIV_DEP, BENCH_DIV_INDEP,
    BENCH_FLOAT_INT, BENCH_INT_FLOAT, BENCH_FLASH, BENCH_RAM,
    BENCH_FLASH_INTERP, BENCH_RAM_INTERP, BENCH_PHASE_WRAP, BENCH_PITCH,
    BENCH_CASES_COUNT };
void rdx_bench_start(void);
void rdx_bench_tick(void);
const char* rdx_bench_name(uint32_t index);
void rdx_bench_result(uint32_t index,uint32_t* result4);
#ifdef __cplusplus
}
#endif
