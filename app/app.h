#pragma once
#include "plat.h"
#define APP_NAME "FM1-RDX"
#ifndef APP_VERSION
#define APP_VERSION "1.0.2"
#endif
#define APP_SPLASH_RGB RGB(64,160,220)
#define APP_BLOCK 128u
#define APP_CANVAS_PX (240u*8u)
// Preserve reserved IDs; user banks occupy four sectors below panel/loader data.
enum {OBJ_SETTINGS,OBJ_PANEL,OBJ_BLE,OBJ_USER_BANK0,OBJ_USER_BANK1,OBJ_DRAFT_BASE,OBJ_NOBJ=OBJ_DRAFT_BASE+28};
// Four full-capacity drafts, two generations each, at 98000..CFFFF.
#define DRAFT_PAIR_MAP(b) b,b+0x2000u,b+0x4000u,b+0x6000u,b+0x8000u,b+0xA000u,b+0xC000u
#define APP_STORE_MAP {0xFC000u,0xDC000u,0xDE000u,0xD8000u,0xDA000u,DRAFT_PAIR_MAP(0x98000u),DRAFT_PAIR_MAP(0xA6000u),DRAFT_PAIR_MAP(0xB4000u),DRAFT_PAIR_MAP(0xC2000u)}
void app_init(void);
void app_render(int32_t*,uint32_t,uint32_t);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char*,const char*);
void app_overload(void);
int app_sysex(const uint8_t*,uint32_t);
void app_loop(void);
#define APP_SYSEX
#define APP_LOOP
#define APP_OVERLOAD app_overload
#define APP_CPU_KHZ
#define APP_HOST_EXPECT
int app_expect(const char*,int*);
