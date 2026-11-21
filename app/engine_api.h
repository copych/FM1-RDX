#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void rdx_boot(void);
void rdx_note_on(uint8_t note, uint8_t velocity);
void rdx_note_off(uint8_t note);
void rdx_sustain(uint8_t down);
void rdx_panic(void);
void rdx_all_notes_off(void);
void rdx_pitch_bend(uint16_t value);
void rdx_control_change(uint8_t controller,uint8_t value);
uint32_t rdx_midi_receive_channel(void);
void rdx_render(float* left, float* right);
int rdx_patch_valid(const uint8_t* patch150);
void rdx_patch_export(uint8_t* patch150);
void rdx_patch_import(const uint8_t* patch150);
void rdx_patch_edit(const uint8_t* patch150);
void rdx_system_export(uint8_t* system32);
void rdx_system_import(const uint8_t* system32);
void rdx_fx_toggle(void);
uint32_t rdx_fx_status(void);
uint32_t rdx_preset_count(void);
void rdx_preset_request(uint32_t index);
void rdx_preset_name(uint32_t index,char* name11);
#ifdef __cplusplus
}
#endif
