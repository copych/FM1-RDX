/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint32_t spi[5], pc_out, pc_dir, pa_out, pa_dir, map;
static uint32_t *reg(uint32_t addr)
{
    switch (addr) {
    case 0x50080u: return &pc_out;
    case 0x50088u: return &pc_dir;
    case 0x50000u: return &pa_out;
    case 0x50008u: return &pa_dir;
    case 0x51020u: return &map;
    default: assert(addr >= 0x11D00u && addr <= 0x11D10u); return &spi[(addr - 0x11D00u) / 4u];
    }
}
#define FM1_LCD_REG(addr) (*reg(addr))
#include "fm1_lcd_hw.h"
static uint32_t service_calls,complete_after,clock_value,clock_step,service_delay,source_address,source_count;
static uint32_t test_clock(void){uint32_t now=clock_value;clock_value+=clock_step;return now;}
static void service_audio(void){
    assert(!(pc_out&FM1_LCD_CS)); // transfer/source ownership stays with LCD
    assert(spi[3]==source_address&&spi[4]==source_count);
    ++service_calls;clock_value+=service_delay;
    if(complete_after&&service_calls==complete_after)spi[0]|=0x8000u; // simulated DMA completes
}
int main(void)
{
    static uint8_t pixels[480];
    pc_out = pc_dir = pa_out = pa_dir = 0xFFFFFFFFu;
    fm1_lcd_hw_init();
    assert(spi[0] == 0x4021 && spi[1] == 4);
    assert(pc_out & FM1_LCD_CS);
    assert(!(pc_dir & (FM1_LCD_CS | FM1_LCD_DC | FM1_LCD_CLK | FM1_LCD_DO)));
    assert(!(pa_out & FM1_LCD_BL) && !(pa_dir & FM1_LCD_BL));
    fm1_lcd_send_cmd(0x2C);
    assert(!(pc_out & (FM1_LCD_CS | FM1_LCD_DC)) && spi[2] == 0x2C);
    spi[0] |= 0x8000u;
    assert(fm1_lcd_wait() == 0 && fm1_lcd_timeouts == 0);
    fm1_lcd_deselect();
    assert(pc_out & FM1_LCD_CS);
    fm1_lcd_send_data(pixels, sizeof pixels);
    assert(!(pc_out & FM1_LCD_CS) && (pc_out & FM1_LCD_DC));
    assert(spi[3] == (uint32_t)(uintptr_t)pixels && spi[4] == sizeof pixels);
    spi[0] &= ~0x8000u;
    spi[0] |= 1u;
    assert(fm1_lcd_wait() == -1 && fm1_lcd_timeouts == 1 && (spi[0] & 1u)); /* counted; SPI stays on */
    assert(spi[0] & 0x4000u); /* pending cleared either way */
    fm1_lcd_deselect();
    assert(pc_out & FM1_LCD_CS);
    spi[0] |= 0x8000u;
    assert(fm1_lcd_wait() == 0 && fm1_lcd_timeouts == 1); /* a caller that inits once keeps drawing */
    fm1_lcd_spi_off();
    assert(!(spi[0] & 1u));
    fm1_lcd_hw_init();
    assert(spi[0] & 1u); /* reinit recovers the disabled controller */
    fm1_lcd_backlight(0);
    assert(pa_out & FM1_LCD_BL);
    fm1_lcd_backlight(1);
    assert(!(pa_out & FM1_LCD_BL));
    fm1_lcd_send_data(pixels,sizeof pixels);spi[0]&=~0x8000u;
    source_address=spi[3];source_count=spi[4];
    service_calls=0;complete_after=3;clock_value=0;clock_step=1;
    assert(fm1_lcd_wait_service(service_audio,test_clock,1000)==0);
    assert(service_calls==3&&fm1_lcd_timeouts==1&&(spi[0]&0x4000u));
    assert(!(pc_out&FM1_LCD_CS)); // wait does not deselect behind the caller
    spi[0]&=~0x8000u;service_calls=complete_after=0;clock_value=0;clock_step=25;
    assert(fm1_lcd_wait_service(service_audio,test_clock,50)==-1);
    assert(service_calls==1&&fm1_lcd_timeouts==2&&(spi[0]&1u));
    spi[0]&=~0x8000u;service_calls=0;clock_value=0xfffffff0u;clock_step=10;
    assert(fm1_lcd_wait_service(service_audio,test_clock,30)==-1);
    assert(service_calls==2&&fm1_lcd_timeouts==3); // elapsed clock wraps
    spi[0]&=~0x8000u;service_calls=0;clock_value=clock_step=0;service_delay=100;
    assert(fm1_lcd_wait_service(service_audio,test_clock,50)==-1);
    assert(service_calls==1&&fm1_lcd_timeouts==4); // callback time counts toward wall bound
    spi[0]&=~0x8000u;service_calls=0;clock_value=service_delay=0;
    assert(fm1_lcd_wait_service(service_audio,test_clock,1)==-1);
    assert(service_calls==62500&&fm1_lcd_timeouts==5); // original 4M poll ceiling
    spi[0]|=0x8000u;service_calls=0;
    assert(fm1_lcd_wait_service(service_audio,test_clock,1)==0&&service_calls==0);
    assert(fm1_lcd_wait_service(0,test_clock,1)==0&&fm1_lcd_timeouts==5);
    puts("LCD transport: pins, DMA address/count, timeout, SPI off and recovery checks passed");
    puts("Cooperative LCD: service, completion/source ownership, elapsed/poll bounds, callback time, wrap and legacy fallback passed");
    return 0;
}
