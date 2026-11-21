import fs from 'node:fs';
import crypto from 'node:crypto';
const names=['UI_Display.h','font_mo.h','bmp_fx.h','bmp_waves.h','bmp_slider.h','bmp_curves.h','UI_Algos.h','RDX_Sliders.h','RDX_Graph.h'];
const hashes={};fs.mkdirSync('src/gui',{recursive:true});
for(const name of names) {
  const path=`RDX_before_UAC/src/GUI/${name}`,raw=fs.readFileSync(path);hashes[path]=crypto.createHash('sha256').update(raw).digest('hex');
  let s=raw.toString().replace(/\r\n/g,'\n').replace('#include <Arduino.h>','#include "ui_support.h"');
  s=s.replace(/\bsnprintf\b/g,'ui_snprintf').replace(/\bstrlen\b/g,'ui_strlen').replace(/\bmin\(/g,'ui_min(').replace(/\babs\(/g,'ui_abs(').replace(/\bmap\(/g,'ui_map(');
  if(name==='UI_Display.h') {
    s=s.replace('virtual ~UI_Display() {}','~UI_Display() {}')
      .replace('virtual void setPixel(int x, int y, bool color) = 0;','virtual void setPixel(int, int, bool) {}')
      .replace('virtual bool getPixel(int x, int y) const = 0;','virtual bool getPixel(int, int) const {return false;}')
      .replace('virtual int getWidth() const = 0;','virtual int getWidth() const {return 128;}')
      .replace('virtual int getHeight() const = 0;','virtual int getHeight() const {return 64;}')
      .replace('virtual void update() = 0;','virtual void update() {}').replace('virtual void clear() = 0;','virtual void clear() {}')
      .replace('int e2 = err << 1;','int e2 = err * 2;');
  }
  fs.writeFileSync(`src/gui/${name}`,'// Adapted from the user\'s completed ESP GUI; source notices retained.\n'+s);
}
const path='RDX_before_UAC/RDX_GUI.h',raw=fs.readFileSync(path);hashes[path]=crypto.createHash('sha256').update(raw).digest('hex');
let s=raw.toString().replace(/\r\n/g,'\n');
const start=s.indexOf('class RDX_GUI');s=s.slice(start,s.lastIndexOf('#endif'));
s=s.replace('PHASER_LAB\n','PHASER_LAB, FX1, FX2, AEG_RATES, AEG_LEVELS, PEG_RATES, PEG_LEVELS, PEG_ON, MIDI, PAGES_COUNT\n')
 .replace('RDX_GUI () {};','RDX_GUI (RDX_Patch& patch) : patch_(patch) {};')
 .replace('RDX_Patch& patch_ = RDX_State::getState().workingPatch;','RDX_Patch& patch_;')
 .replace(/\bdisplay\b/g,'ui_display()').replace(/\bFxPhaser\b/g,'UiPhaserView')
 .replace(/pm.currentIndex\(\)/g,'ui_preset_index()').replace(/\bsnprintf\b/g,'ui_snprintf');
s=s.replace('case Page::HOME:\n        default:',`case Page::FX1: drawEffect_(0); break;
        case Page::FX2: drawEffect_(1); break;
        case Page::AEG_RATES: drawEnvelope_(false,false); break;
        case Page::AEG_LEVELS: drawEnvelope_(false,true); break;
        case Page::PEG_RATES: drawEnvelope_(true,false); break;
        case Page::PEG_LEVELS: drawEnvelope_(true,true); break;
        case Page::PEG_ON: drawPitchOn_(); break;
        case Page::MIDI: drawMidi_(); break;
        case Page::HOME:
        default:`);
s=s.replace('ui_display().drawText(2, 0, "Bank  -");','if(ui_preset_index()==0) ui_display().drawText(2, 0, "INIT PATCH");\n      else {\n      ui_display().drawText(2, 0, "Bank  -");')
 .replace('ui_display().drawText(2 + 6 * 7, 0, t);','ui_display().drawText(2 + 6 * 7, 0, t);\n      }');
const updatedLabStart=s.indexOf('    inline void drawPhaserLab_()');
const labEnd=s.indexOf('    RDX_Patch& patch_;',updatedLabStart);
s=s.slice(0,updatedLabStart)+`    inline void drawEnvelope_(bool pitch,bool levels) {
      ui_display().setTextScale(UITextScale::X1);
      const uint8_t* r=pitch?patch_.common.pegRate:patch_.ops[selectedOp_].egRate;
      const uint8_t* l=pitch?patch_.common.pegLevel:patch_.ops[selectedOp_].egLevel;
      for(int i=0;i<4;++i){
        if(pitch&&levels){char value[8];ui_snprintf(value,sizeof value,"%+d",(int)l[i]-64);ui_display().drawText(i*32+5,0,value);}
        else ui_display().drawText(i*32+8,0,toText(levels?l[i]:r[i]));
      }
      drawEG(ui_display(),0,14,128,50,l,r,pitch?UIEnvType::ENV_PITCH:UIEnvType::ENV_AMP,levels?UIEnvMode::ENV_LEVELS:UIEnvMode::ENV_RATES);
    }
    inline void drawPitchOn_(){
      ui_display().setTextScale(UITextScale::X1);ui_display().drawText(10,0,"PITCH EG ON/OFF");
      drawAlgo(ui_display(),14,patch_.common.algorithm,22,false);
      for(int i=0;i<4;++i)ui_display().drawText(i*32+5,46,patch_.ops[i].pegEnable?"ON":"off");
    }
    inline void drawMidi_(){
      ui_display().setTextScale(UITextScale::X1);
      ui_display().drawText(0,2,"RX CHANNEL");
      ui_display().drawText(90,2,ui_midi_channel()?toText(ui_midi_channel()):"ALL");
      ui_display().drawText(0,20,"FM CONTROL CC");ui_display().drawText(100,20,ui_midi_control()?"ON":"off");
      ui_display().drawText(0,44,"PITCH / MOD / VOL");ui_display().drawText(0,55,"ALWAYS RECEIVED");
    }
    inline void drawPhaserLab_() {
      static const char* names[5]={"PH MIX","PH FB","FL MIX","FL DEP","FL OFF"};
      ui_display().setTextScale(UITextScale::X1);
      for(uint8_t i=0;i<5;++i) {
        int value=(int)(phaserTune_->getTuneParam(i)*100.0f+0.5f);
        char line[26];ui_snprintf(line,sizeof line,"%c %s %d.%02d",i==*phaserTuneSel_?'>':' ',names[i],value/100,value%100);
        ui_display().drawText(2,2+i*10,line);
      }
      ui_display().drawText(0,55,"METERS NOT AVAILABLE");
    }
\n`+s.slice(labEnd);
fs.writeFileSync('src/gui/RDX_GUI.h','// Original ESP screen drawings, adapted for a snapshot and FM-1 display backend.\n#pragma once\n#include "ram_display.h"\n#include "bmp_fx.h"\n#include "bmp_waves.h"\n#include "bmp_slider.h"\n#include "UI_Algos.h"\n#include "RDX_Sliders.h"\n#include "RDX_Graph.h"\n'+s);
fs.writeFileSync('gui-lock.json',JSON.stringify({sha256:hashes},null,2)+'\n');
console.log('Imported original GUI screens, widgets, font, diagrams and bitmaps.');
