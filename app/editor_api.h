#pragma once
#include <stdint.h>
enum eEditorPage {
    EDITOR_HOME=0,EDITOR_LEVEL,EDITOR_FEEDBACK,EDITOR_FREQ,EDITOR_ALGO,EDITOR_VOICE,
    EDITOR_OP1,EDITOR_OP2,EDITOR_OP3,EDITOR_LFO1,EDITOR_LFO2,EDITOR_LFO3,EDITOR_LAB,
    EDITOR_FX1,EDITOR_FX2,EDITOR_AEG_RATES,EDITOR_AEG_LEVELS,EDITOR_PEG_RATES,EDITOR_PEG_LEVELS,EDITOR_PAGES_COUNT
};
typedef struct {uint8_t patch[150];float phaser[5];uint32_t revision,preset,fx,dirty,error;} EditorSnapshot;
#ifdef __cplusplus
extern "C" {
#endif
void rdx_editor_snapshot(EditorSnapshot* out);
void rdx_editor_adjust(uint8_t page,uint8_t op,uint8_t knob,int16_t delta,uint8_t fine);
void rdx_editor_boot(void);
const uint8_t* rdx_editor_draw(const EditorSnapshot* snapshot,uint8_t page,uint8_t op,uint8_t tune);
const char* rdx_editor_title(uint8_t page);
const char* rdx_editor_label(uint8_t page,uint8_t knob);
#ifdef __cplusplus
}
#endif
