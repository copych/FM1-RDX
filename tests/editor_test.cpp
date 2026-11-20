#include "editor_api.h"
#include "engine_api.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static float left[128],right[128];
static EditorSnapshot snapshot;
static float run(int blocks) {
    float peak=0.0f;
    for(int b=0;b<blocks;++b){rdx_render(left,right);for(int i=0;i<128;++i){CHECK(std::isfinite(left[i])&&std::isfinite(right[i]));peak=fmaxf(peak,fabsf(left[i]));}}
    return peak;
}
static void read(){rdx_editor_snapshot(&snapshot);}
int main() {
    rdx_boot();rdx_editor_boot();run(30);read();
    for(uint8_t page=0;page<EDITOR_PAGES_COUNT;++page)for(uint8_t op=0;op<4;++op) {
        const uint8_t* bitmap=rdx_editor_draw(&snapshot,page,op,0);int lit=0;
        for(int i=0;i<1024;++i)lit+=bitmap[i]!=0;CHECK(lit>0);
    }
    rdx_fx_toggle();rdx_note_on(60,100);CHECK(run(100)>0.0001f);
    rdx_editor_adjust(EDITOR_LEVEL,0,0,-127,0);run(1);read();CHECK(snapshot.patch[56]==0);CHECK(run(5)==0.0f);
    rdx_editor_adjust(EDITOR_LEVEL,0,0,127,0);run(1);read();CHECK(snapshot.patch[56]==127);CHECK(run(10)>0.0001f);
    CHECK(snapshot.dirty==1);
    rdx_editor_adjust(EDITOR_ALGO,0,0,127,0);run(1);read();CHECK(snapshot.patch[16]==11);
    rdx_editor_adjust(EDITOR_OP1,2,2,127,1);run(1);read();CHECK(snapshot.patch[38+2*28+23]==99);
    rdx_editor_adjust(EDITOR_AEG_RATES,1,3,-127,0);run(1);read();CHECK(snapshot.patch[38+28+4]==0);
    rdx_editor_adjust(EDITOR_LFO1,0,0,127,0);run(1);read();CHECK(snapshot.patch[17]==6);
    rdx_editor_adjust(EDITOR_OP3,0,0,-127,0);run(1);read();CHECK(snapshot.patch[38+10]<=127);
    // Editing frequency/mode/FX cannot publish an invalid memory layout.
    rdx_preset_request(1);run(30);read();CHECK(snapshot.dirty==0);
    rdx_editor_adjust(EDITOR_FX1,0,0,2,0);run(30);read();CHECK(snapshot.patch[29]==6);
    const uint8_t old_time=snapshot.patch[31];
    rdx_editor_adjust(EDITOR_FX1,0,3,127,0);run(30);read();CHECK(snapshot.error==1);CHECK(snapshot.patch[31]==old_time);
    rdx_preset_request(25);run(30);
    read();const float previous=snapshot.phaser[0];rdx_editor_adjust(EDITOR_LAB,0,1,1,0);run(1);read();CHECK(snapshot.phaser[0]>previous);
    rdx_panic();run(30);CHECK(run(5)==0.0f);
    std::puts("PASS: all 19 source/editor pages, live audio level editing, parameter bounds, pending FX edits, budget rejection, phaser tuning.");
}
