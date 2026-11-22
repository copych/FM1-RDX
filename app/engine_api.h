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
void rdx_budget_observe(uint32_t ticks,uint32_t now);
void rdx_budget_overload(uint32_t now);
uint32_t rdx_voice_status(void);
void rdx_cost_reset(void);
void rdx_cost_profile(uint32_t* ticks3);
enum eVoiceCostState { COST_IDLE, COST_ARMED, COST_SETTLE, COST_MEASURE, COST_DONE, COST_STOPPED, COST_LIMIT, COST_MISMATCH, COST_NEED_POLY, COST_CHANGED, COST_STATES_COUNT };
enum eVoiceCostCommand { COST_START, COST_STOP, COST_COMMANDS_COUNT };
enum { COST_ROWS=9, COST_ROW_WORDS=11, COST_HEADER_WORDS=6, COST_SNAPSHOT_WORDS=COST_HEADER_WORDS+COST_ROWS*COST_ROW_WORDS };
enum eVoiceCostField { COST_APP, COST_SYN, COST_FX, COST_BLOCKS, COST_STEADY_BLOCKS, COST_PAIRED_SYN, COST_PAIRED_FX, COST_ACTUAL, COST_QUEUE, COST_TURN, COST_LEAD, COST_FIELDS_COUNT };
void rdx_measure_command(uint8_t command,uint32_t now);
void rdx_measure_prepare(uint32_t now);
void rdx_measure_recover(void);
int rdx_measure_active(void);
void rdx_measure_snapshot(uint32_t* words);
enum eDetailMode { DETAIL_OFF, DETAIL_COARSE, DETAIL_FINE, DETAIL_MODES_COUNT };
void rdx_detail_set(uint8_t on);
void rdx_detail_profile(uint32_t* ticks9);
void rdx_detail_fine(uint32_t* ticks4);
const char* rdx_build_label(void);
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

#ifdef __cplusplus
extern "C" {
#endif
// Dry-render pipeline: access only after acquire succeeds. CPU0 publishes after
// consuming the previous bus; CPU1 services at UI checkpoints or as a BSP job.
int rdx_dry_acquire(void);
enum eDryCheckpoint {DRY_AT_IDLE,DRY_AT_CONTROL,DRY_AT_LCD,DRY_AT_TEXT,DRY_AT_SCALE,DRY_AT_EDITOR,DRY_AT_PROTOCOL,DRY_AT_WAIT,DRY_AT_CLEAR,DRY_AT_SUBMIT,DRY_CHECKPOINTS_COUNT};
void rdx_dry_service(void);
void rdx_dry_service_at(uint32_t checkpoint);
void rdx_dry_block_start(uint32_t ticks);
void rdx_dry_detail(uint32_t* words8);
int rdx_dry_publish(void);
void rdx_dry_mode(uint8_t on);
void rdx_dry_snapshot(uint32_t* words8);
void rdx_dry_late(void);
void rdx_dry_retired(void); // main only, after BSP has completed/held CPU1
void rdx_dry_reset_stats(void);
void rdx_dry_set_kick(void (*kick)(void));
#ifdef __cplusplus
}
#endif
