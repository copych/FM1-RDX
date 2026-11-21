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
void rdx_fx_toggle(void);
uint32_t rdx_fx_status(void);
uint32_t rdx_preset_count(void);
void rdx_preset_request(uint32_t index);
void rdx_preset_name(uint32_t index,char* name11);
#ifdef __cplusplus
}
#endif
