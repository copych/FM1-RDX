#pragma once
#include "plat.h"
#define APP_NAME "FM1-RDX"
#define APP_SPLASH_RGB RGB(64,160,220)
#define APP_BLOCK 128u
#define APP_CANVAS_PX (240u*8u)
// Retain the platform template's reserved object layout. No app writes yet.
enum {OBJ_SETTINGS,OBJ_PANEL,OBJ_BLE,OBJ_NOBJ};
#define APP_STORE_MAP {0xFC000u,0xDC000u,0xDE000u}
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
#define APP_HOST_EXPECT
int app_expect(const char*,int*);
