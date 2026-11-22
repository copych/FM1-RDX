#include <stdint.h>
#include <assert.h>
#include <stdio.h>
static uint32_t clock_ticks;
static uint32_t plat_ticks24(void){return clock_ticks;}
#include "../app/ui_profile.h"
int main(void){
    ui_profile_reset();clock_ticks=1000;
    ui_record(UI_DMA_WAIT,100);ui_record(UI_TEXT,800);
    assert(ui_peaks[UI_DMA_WAIT]==900&&ui_peaks[UI_TEXT]==200);
    ui_record(UI_DMA_WAIT,900);assert(ui_peaks[UI_DMA_WAIT]==900&&ui_calls[UI_DMA_WAIT]==2);
    clock_ticks=20;ui_record(UI_SUBMIT,0xfffffff0u);assert(ui_peaks[UI_SUBMIT]==36);
    uint32_t snapshot[UI_PARTS_COUNT];ui_profile_snapshot(snapshot);
    assert(snapshot[UI_DMA_WAIT]==900&&snapshot[UI_SUBMIT]==36&&snapshot[UI_CLEAR]==0);
    ui_record(UI_PARTS_COUNT,0);ui_record(~0u,0); // invalid indices must not touch storage
    ui_calls[UI_EDITOR]=UINT32_MAX;ui_record(UI_EDITOR,0);assert(ui_calls[UI_EDITOR]==UINT32_MAX);
    ui_profile_reset();for(unsigned i=0;i<UI_PARTS_COUNT;++i)assert(!ui_calls[i]&&!ui_peaks[i]);
    puts("PASS UI timing: independent peaks/counts, snapshot, timer wrap, invalid indices, saturation and reset");
}
