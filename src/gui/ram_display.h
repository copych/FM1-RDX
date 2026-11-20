#pragma once
#include "../core/RDX_Types.h"
#include "UI_Display.h"
class UiRamDisplay : public UI_Display {
public:
    void begin(){clear();}
    int getWidth() const override{return 128;}
    int getHeight() const override{return 64;}
    void clear() override{memset(pixels,0,sizeof pixels);}
    void update() override{}
    void setPixel(int x,int y,bool on) override {
        if((unsigned)x>=128||(unsigned)y>=64)return;
        uint8_t& b=pixels[(y>>3)*128+x];uint8_t mask=(uint8_t)(1u<<(y&7));
        if(on)b|=mask;else b&=(uint8_t)~mask;
    }
    bool getPixel(int x,int y) const override{return (unsigned)x<128&&(unsigned)y<64&&(pixels[(y>>3)*128+x]>>(y&7)&1u);}
    uint8_t pixels[1024];
};
class UiPhaserView {
public:
    enum {TUNE_PHASER_MIX,TUNE_PHASER_FB,TUNE_FLANGER_MIX,TUNE_FLANGER_DEPTH_SCALE,TUNE_FLANGER_OFFSET_MS,TUNE_COUNT};
    float values[5]{};
    float getTuneParam(uint8_t i) const{return i<5?values[i]:0.0f;}
};
UiRamDisplay& ui_display();
unsigned ui_preset_index();
