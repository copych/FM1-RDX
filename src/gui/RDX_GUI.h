// Original ESP screen drawings, adapted for a snapshot and FM-1 display backend.
#pragma once
#include "ram_display.h"
#include "bmp_fx.h"
#include "bmp_waves.h"
#include "bmp_slider.h"
#include "UI_Algos.h"
#include "RDX_Sliders.h"
#include "RDX_Graph.h"
class RDX_GUI {
  public:
    enum class Page : uint8_t {
      HOME = 0,
      LEVEL,
      FEEDBACK,
      FREQ,
      ALGO,
      FUNCTION_VOICE,
      EDIT_OP1,
      EDIT_OP2,
      EDIT_OP3,
      EDIT_LFO1,
      EDIT_LFO2,
      EDIT_LFO3,
      PHASER_LAB, FX1, FX2, AEG_RATES, AEG_LEVELS, PEG_RATES, PEG_LEVELS, PAGES_COUNT
    };

    RDX_GUI (RDX_Patch& patch) : patch_(patch) {};
    
    inline void begin() { ui_display().begin(); }

    inline void draw() {
      if (updateCounter_ > 0) {
        updateCounter_--;
        return;
      } 
      if(!needUpdate_) return;

      ui_display().clear();
      ui_display().setColor(UIDisplayColor::WHITE);
      ui_display().setBrush(UIDisplayBrush::SOLID);
      ui_display().setTextScale(UITextScale::X1);

      switch (page_) {
        case Page::LEVEL:
          drawLevel_();
          break;

        case Page::FEEDBACK:
          drawFeedback_();
          break;

        case Page::FREQ:
          drawFreqPage_();
          break;

        case Page::ALGO:
          drawAlgoPage_();
          break;

        case Page::FUNCTION_VOICE:
          drawFunctionVoice_();
          break;

        case Page::EDIT_OP1:
          drawEditOp1_();
          break;
        
        case Page::EDIT_OP2:
          drawEditOp2_();
          break;

        case Page::EDIT_OP3:
          drawEditOp3_();
          break;

        case Page::EDIT_LFO1:
          drawEditLfo1_();
          break;

        case Page::EDIT_LFO2:
          drawEditLfo2_();
          break;

        case Page::EDIT_LFO3:
          drawEditLfo3_();
          break;

        case Page::PHASER_LAB:
          drawPhaserLab_();
          break;

        case Page::FX1: drawEffect_(0); break;
        case Page::FX2: drawEffect_(1); break;
        case Page::AEG_RATES: drawEnvelope_(false,false); break;
        case Page::AEG_LEVELS: drawEnvelope_(false,true); break;
        case Page::PEG_RATES: drawEnvelope_(true,false); break;
        case Page::PEG_LEVELS: drawEnvelope_(true,true); break;
        case Page::HOME:
        default:
          drawHome_();
          break;
      }

      ui_display().update();
      if (page_ == Page::PHASER_LAB) {
        // Keep the peak meters live without refreshing the OLED every GUI-task tick.
        updateCounter_ = 50;
        needUpdate_ = true;
      } else {
        needUpdate_ = false;
      }
    }

    inline void push() { needUpdate_ = true; }

    inline void pause(int32_t n) { updateCounter_ = n; }

    inline void setPage(Page page) {
      if (page_ == page) return;
      page_ = page;
      push();
    }

    inline Page page() const { return page_; }

    inline void setOperator(uint8_t op) {
      if (op > 3) op = 3;
      if (selectedOp_ == op) return;
      selectedOp_ = op;
      push();
    }

    inline uint8_t selectedOperator() const { return selectedOp_; }

    inline void setPhaserLab(UiPhaserView* phaser, uint8_t* selection,
                             const volatile float* hostInputPeak,
                             const volatile float* hostOutputPeak) {
      phaserTune_ = phaser;
      phaserTuneSel_ = selection;
      hostInputPeak_ = hostInputPeak;
      hostOutputPeak_ = hostOutputPeak;
      setPage(Page::PHASER_LAB);
    }

    inline void nextTestPage() {
      switch (page_) {
        case Page::HOME:
          setPage(Page::LEVEL);
          break;
        case Page::LEVEL:
          setPage(Page::FEEDBACK);
          break;
        case Page::FEEDBACK:
          setPage(Page::FREQ);
          break;
        case Page::FREQ:
          setPage(Page::ALGO);
          break;
        case Page::ALGO:
          setPage(Page::FUNCTION_VOICE);
          break;
        case Page::FUNCTION_VOICE:
          setPage(Page::EDIT_OP1);
          break;
        case Page::EDIT_OP1:
          setPage(Page::EDIT_OP2);
          break;
        case Page::EDIT_OP2:
          setPage(Page::EDIT_OP3);
          break;
        case Page::EDIT_OP3:
          setPage(Page::EDIT_LFO1);
          break;
        case Page::EDIT_LFO1:
          setPage(Page::EDIT_LFO2);
          break;
        case Page::EDIT_LFO2:
          setPage(Page::EDIT_LFO3);
          break;
        case Page::EDIT_LFO3:
        default:
          setPage(Page::HOME);
          break;
      }
    }

  private:
    inline void drawHome_() {
      ui_display().setTextScale(UITextScale::X2);
      ui_display().drawTextBytes(4, 10, patch_.common.voiceName, 10);

      ui_display().setTextScale(UITextScale::X1);
      ui_display().drawText(2, 0, "Bank  -");

      uint8_t m, l;
      m = (ui_preset_index()-1) / 8 + 1;
      l = (ui_preset_index()-1) % 8 + 1;
      char t[5];
      ui_snprintf(t, sizeof(t), "%d", m);
      ui_display().drawText(2 + 6 * 5, 0, t);
      ui_snprintf(t, sizeof(t), "%d", l);
      ui_display().drawText(2 + 6 * 7, 0, t);

      ui_display().setBrush(UIDisplayBrush::DOTTED);
      ui_display().drawHLine(0, 28, 128);

      drawAlgo(ui_display(), 29, patch_.common.algorithm, 35, true);
    }

    inline void drawLevel_() {
      // Reface-DX-like LEVEL page:
      // compact algorithm/operator topology at the top,
      // then four live operator output-level gauges.
      ui_display().setTextScale(UITextScale::X1);
      ui_display().setBrush(UIDisplayBrush::SOLID);
      ui_display().setColor(UIDisplayColor::WHITE);

      drawAlgo(ui_display(), 0, patch_.common.algorithm, 18, false);

      constexpr int COL_W = 32;
      constexpr int SLIDER_W = 18;
      constexpr int SLIDER_Y = 22;
      constexpr int SLIDER_H = 43;

      for (uint8_t op = 0; op < 4; ++op) {
        const int x = op * COL_W + (COL_W - SLIDER_W) / 2;
        drawSlider1(ui_display(),
          x, SLIDER_Y,
          SLIDER_W, SLIDER_H,
          patch_.ops[op].outLevel,
          0, 127,
          true, false
        );
      }
      drawDividers(ui_display(), 4, SLIDER_Y + 8,  SLIDER_H);
    }

    inline void drawFeedback_() {
      // Reface-DX-like FB page:
      // same four-operator topology used by LEVEL,
      // but the four live gauges show per-operator feedback.
      ui_display().setTextScale(UITextScale::X1);
      ui_display().setBrush(UIDisplayBrush::SOLID);
      ui_display().setColor(UIDisplayColor::WHITE);

      drawAlgo(ui_display(), 0, patch_.common.algorithm, 18, false);

      constexpr int COL_W = 32;
      constexpr int SLIDER_W = 18;
      constexpr int SLIDER_Y = 22;
      constexpr int SLIDER_H = 43;

      for (uint8_t op = 0; op < 4; ++op) {
        const int x = op * COL_W + (COL_W - SLIDER_W) / 2;
        drawSlider2(ui_display(),
          x, SLIDER_Y,
          SLIDER_W, SLIDER_H,
          (patch_.ops[op].fbType != RDX_FB_SAW ) ? -patch_.ops[op].feedback : patch_.ops[op].feedback ,
          -127, 127,
          true, false
        );
      }
      drawDividers(ui_display(), 4, SLIDER_Y,  SLIDER_H);

    }
  

    inline void drawFreqPage_() {
      ui_display().setTextScale(UITextScale::X1);
      ui_display().setBrush(UIDisplayBrush::SOLID);
      ui_display().setColor(UIDisplayColor::WHITE);

      drawAlgo(ui_display(), 0, patch_.common.algorithm, 18, false);

      constexpr int COL_W = 32;
      constexpr int VALUE_Y = 30;
      constexpr int VALUE_H = 42;

      for (uint8_t op = 0; op < 4; ++op) {
        drawFreq(ui_display(),
          op * COL_W, VALUE_Y,
          COL_W - 3, VALUE_H,
          patch_.ops[op].freqCoarse,
          patch_.ops[op].freqFine
        );
      }

      drawDividers(ui_display(), 4, VALUE_Y, VALUE_H);
    }

    inline void drawAlgoPage_() {
      ui_display().setTextScale(UITextScale::X1);
      ui_display().setBrush(UIDisplayBrush::SOLID);
      ui_display().setColor(UIDisplayColor::WHITE);

      // Full-screen algorithm view. Existing renderer also draws the
      // algorithm number when showId=true.
      drawAlgo(ui_display(), 0, patch_.common.algorithm, 64, true);
    }



    inline void drawFunctionVoice_() {
      ui_display().setTextScale(UITextScale::X1);
      ui_display().setBrush(UIDisplayBrush::SOLID);
      ui_display().setColor(UIDisplayColor::WHITE);

      // FUNCTION page 1/4: VOICE
      const char* title = "VOICE";
      const int titleW = ui_display().getTextWidth(title);
      ui_display().drawText((ui_display().getWidth() - titleW) / 2, 0, title);
      drawPager(ui_display(), 0, 0, 4, 0);

      constexpr int COL_W = 32;
      constexpr int Y = 13;
      constexpr int H = 51;
      constexpr int SW = 18;

      // 1: transpose (-24..+24 in the patch data used by RDX)
      ui_display().drawText(11, Y, "TP");
      drawSlider3(ui_display(),
        7, Y + 10,
        SW, H - 10,
        (int8_t)patch_.common.transpose - 64,
        -24, 24,
        true, true
      );

      // 2: mono/poly mode
      //ui_display().drawText(COL_W + 2, Y, "MODE");
      const char* mode1;
      const char* mode2;
      switch (patch_.common.monoPoly) {
        case RDX_MODE_MONO_FULL:
          mode1 = "MONO-";
          mode2 = "FULL";
          break;
        case RDX_MODE_MONO_LEGATO:
          mode1 = "MONO-";
          mode2 = "LGATO";
          break;
        case RDX_MODE_POLY:
        default:
          mode1 = "POLY";
          mode2 = " ";
          break;
      }
      ui_display().drawText(COL_W + 2, Y + 20, mode1);
      ui_display().drawText(COL_W + 2, Y + 31, mode2);

      // 3: portamento time 0..127
      ui_display().drawText(COL_W * 2 + 2, Y, "PORTA");
      drawSlider4(ui_display(),
        COL_W * 2 + 7, Y + 10,
        SW, H - 10,
        patch_.common.portaTime,
        0, 127,
        true, false
      );

      // 4: pitch-bend range, stored as 64 + semitones
      ui_display().drawText(COL_W * 3 + 11, Y, "PB");
      drawSlider3(ui_display(),
        COL_W * 3 + 7, Y + 10,
        SW, H - 10,
        (int)patch_.common.pbRange - 64,
        -24, 24,
        true, true
      );

      drawDividers(ui_display(), 4, Y + 9, H - 9);
    }



    inline const char* fxParam1Name_(uint8_t type) const {
      switch (type) {
        case 1: return "DRIVE"; // Distortion
        case 2: return "DEPTH"; // Touch Wah
        case 3: return "DEPTH"; // Chorus
        case 4: return "DEPTH"; // Flanger
        case 5: return "DEPTH"; // Phaser
        case 6: return "DEPTH"; // Delay
        case 7: return "DEPTH"; // Reverb
        default: return "---";
      }
    }

    inline const char* fxParam2Name_(uint8_t type) const {
      switch (type) {
        case 1: return "TONE"; // Distortion
        case 2: return "RATE"; // Touch Wah
        case 3: return "RATE"; // Chorus
        case 4: return "RATE"; // Flanger
        case 5: return "RATE"; // Phaser
        case 6: return "TIME"; // Delay
        case 7: return "TIME"; // Reverb
        default: return "---";
      }
    }

    inline void drawEffect_(uint8_t slot) {
      ui_display().setTextScale(UITextScale::X1);
      ui_display().setBrush(UIDisplayBrush::SOLID);
      ui_display().setColor(UIDisplayColor::WHITE);

      const uint8_t type = patch_.common.effects[slot][0];
      const uint8_t p1   = patch_.common.effects[slot][1];
      const uint8_t p2   = patch_.common.effects[slot][2];

      // Yamaha-style header.
      char title[9];
      ui_snprintf(title, sizeof(title), "EFFECT %d", slot + 1);
      const int titleW = ui_display().getTextWidth(title);
      ui_display().drawText((ui_display().getWidth() - titleW) / 2, 0, title);
      drawPager(ui_display(), 0, 0, 2, slot);

      // TYPE occupies the left half of the screen. The existing bmp_fx
      // sheet contains eight 32x32 tiles, one per FX type.
      ui_display().drawText(3, 13, "TYPE");

      if (type < 8) {
        drawFX(ui_display(), 0, 24, type);
      }

      // Parameter 1 / 2 occupy the two right-hand logical controls.
      constexpr int Y = 13;
      constexpr int H = 51;
      constexpr int SW = 18;

      const char* p1Name = fxParam1Name_(type);
      const char* p2Name = fxParam2Name_(type);

      int tw = ui_display().getTextWidth(p1Name);
      ui_display().drawText(80 - tw / 2, Y, p1Name);
      drawSlider4(ui_display(),
        71, Y + 10,
        SW, H - 10,
        p1,
        0, 127,
        true, false
      );

      tw = ui_display().getTextWidth(p2Name);
      ui_display().drawText(112 - tw / 2, Y, p2Name);
      drawSlider4(ui_display(),
        103, Y + 10,
        SW, H - 10,
        p2,
        0, 127,
        true, false
      );

      drawDividers(ui_display(), 4, Y + 9, H - 9);
    }


    inline void drawEditOp1_() {
      ui_display().setTextScale(UITextScale::X1);
      ui_display().setBrush(UIDisplayBrush::SOLID);
      ui_display().setColor(UIDisplayColor::WHITE);

      const RDX_OpParams& op = patch_.ops[selectedOp_];

      // Operator number: square = carrier, circle = modulator.
      char opNum[2] = { char('1' + selectedOp_), 0 };

      if (carriers[patch_.common.algorithm][selectedOp_]) {
        ui_display().drawRect(58, 0, 11, 11);
      } else {
        ui_display().drawCircle(63, 6, 11);
      }

      ui_display().drawText(61, 2, opNum);

      // Four operator selectors at the top-right.
      drawPager(ui_display(), 0, 0, 3, 0);

      constexpr int COL_W = 32;
      constexpr int HEAD_Y = 14;
      constexpr int BODY_Y = 26;
      constexpr int BODY_H = 38;

      ui_display().drawText(10, HEAD_Y, "OP");
      ui_display().drawText(37, HEAD_Y, "MODE");
      ui_display().drawText(66, HEAD_Y, "RATIO");
      ui_display().drawText(99, HEAD_Y, "DTUNE");

      // OP on/off: large text as in the reference.
      ui_display().setTextScale(UITextScale::X2);
      ui_display().drawText(3, BODY_Y + 5, op.enable ? "ON" : "OFF");
      ui_display().setTextScale(UITextScale::X1);

      // Frequency mode.
      const char* mode = (op.freqMode == RDX_FREQ_FIXED) ? "Fixed" : "Ratio";
      const int modeW = ui_display().getTextWidth(mode);
      ui_display().drawHLine(COL_W + 2, BODY_Y + 8, COL_W - 3);
      ui_display().drawHLine(COL_W + 2, BODY_Y + 8 + 18, COL_W - 3);
      ui_display().drawText(COL_W + (COL_W - modeW) / 2 + 1, BODY_Y + 13, mode);

      // Existing coarse/fine frequency renderer.
      drawFreq(ui_display(),
        COL_W * 2, BODY_Y + 1,
        COL_W - 3, BODY_H,
        op.freqCoarse,
        op.freqFine
      );

      // Existing centered signed slider; detune is stored 64-centered.
      drawSlider3(ui_display(),
        COL_W * 3 + 7, BODY_Y,
        18, BODY_H,
        (int)op.freqDetune - 64,
        -64, 63,
        true, true
      );

      drawDividers(ui_display(), 4, BODY_Y, BODY_H);
    }

    inline void drawEditOp2_() {
      ui_display().setTextScale(UITextScale::X1);
      ui_display().setBrush(UIDisplayBrush::SOLID);
      ui_display().setColor(UIDisplayColor::WHITE);

      const RDX_OpParams& op = patch_.ops[selectedOp_];

      // Operator number: square = carrier, circle = modulator.
      char opNum[2] = { char('1' + selectedOp_), 0 };

      if (carriers[patch_.common.algorithm][selectedOp_]) {
        ui_display().drawRect(58, 0, 11, 11);
      } else {
        ui_display().drawCircle(63, 6, 11);
      }

      ui_display().drawText(61, 2, opNum);

      // Four operator selectors at the top-right.
      drawPager(ui_display(), 0, 0, 3, 1);

      constexpr int COL_W  = 32;
      constexpr int HEAD_Y = 14;
      constexpr int BODY_Y = 26;
      constexpr int BODY_H = 38;
      constexpr int SL_W   = 18;

      ui_display().drawText(4,  HEAD_Y, "LEVEL");
      ui_display().drawText(35, HEAD_Y, "VEL.S");
      ui_display().drawText(73, HEAD_Y, "FB");
      ui_display().drawText(99, HEAD_Y, "KSC-R");

      // Operator output level.
      drawSlider1(
        ui_display(),
        7,
        BODY_Y,
        SL_W,
        BODY_H,
        op.outLevel,
        0,
        127,
        true,
        false
      );

      // Velocity sensitivity.
      drawSlider4(
        ui_display(),
        COL_W + 7,
        BODY_Y,
        SL_W,
        BODY_H,
        op.velSens,
        0,
        127,
        true,
        false
      );

      // Feedback: sign represents feedback type.
      const int fb =
        (op.fbType != RDX_FB_SAW)
          ? -(int)op.feedback
          :  (int)op.feedback;

      drawSlider2(ui_display(),
        COL_W * 2 + 7, BODY_Y,
        SL_W, BODY_H,
        fb,
        -127, 127,
        true, true
      );

      // Keyboard rate scaling.
      drawSlider4(ui_display(),
        COL_W * 3 + 7, BODY_Y,
        SL_W, BODY_H,
        op.rateScaling,
        0, 127,
        true, false
      );

      drawDividers(ui_display(), 4, BODY_Y, BODY_H);
    }


    inline void drawEditOp3_() {
      ui_display().setTextScale(UITextScale::X1);
      ui_display().setBrush(UIDisplayBrush::SOLID);
      ui_display().setColor(UIDisplayColor::WHITE);

      const RDX_OpParams& op = patch_.ops[selectedOp_];

      constexpr uint8_t Y = 13;
      const uint8_t H = ui_display().getHeight();

      // Selected operator.
      char opNum[2] = { char('1' + selectedOp_), 0 };

      if (carriers[patch_.common.algorithm][selectedOp_]) {
        ui_display().drawRect(58, 0, 11, 11);
      } else {
        ui_display().drawCircle(63, 6, 11);
      }

      ui_display().drawText(61, 2, opNum);

      drawPager(ui_display(), 0, 0, 3, 2);

      // Depths are stored as 0..127.
      // Their signs are encoded by the selected curve types.
      const int lVal =
        (op.scaleLC == RDX_SCALE_POS_LIN ||
        op.scaleLC == RDX_SCALE_POS_EXP)
          ? (int)op.scaleLD
          : -(int)op.scaleLD;

      const int rVal =
        (op.scaleRC == RDX_SCALE_POS_LIN ||
        op.scaleRC == RDX_SCALE_POS_EXP)
          ? (int)op.scaleRD
          : -(int)op.scaleRD;

      drawSlider5(ui_display(),
        8, Y,
        17, H - Y,
        lVal,
        -127, 127,
        true, true
      );

      drawSlider5(ui_display(),
        32 * 3 + 8, Y,
        17, H - Y,
        rVal,
        -127, 127,
        true, true
      );

      drawDividers(ui_display(),
        4, Y,
        H - Y
      );

      drawKeyScaling(ui_display(),
        32, Y,
        61, H - Y,
        op.scaleLC, op.scaleRC,
        op.scaleLD, op.scaleRD
      );
    }


    inline void drawEditLfo1_() {
      ui_display().setTextScale(UITextScale::X1);
      ui_display().setBrush(UIDisplayBrush::SOLID);
      ui_display().setColor(UIDisplayColor::WHITE);

      constexpr int COL_W  = 32;
      constexpr int HEAD_Y = 14;
      constexpr int BODY_Y = 26;
      constexpr int BODY_H = 38;
      constexpr int SL_W   = 18;

      // Header.
      ui_display().drawText(54, 0, "LFO");

      // Page selector.
      drawPager(ui_display(), 0, 0, 3, 0);

      ui_display().drawText(7,   HEAD_Y, "WAVE");
      ui_display().drawText(36,  HEAD_Y, "SPEED");
      ui_display().drawText(69,  HEAD_Y, "DELAY");
      ui_display().drawText(101, HEAD_Y, "PMD");

      // Waveform bitmap.
      drawWave(ui_display(),
        0, BODY_Y,
        patch_.common.lfoWave
      );

      // LFO speed.
      drawSlider4(ui_display(),
        COL_W + 7, BODY_Y,
        SL_W, BODY_H,
        patch_.common.lfoSpeed,
        0, 127,
        true, false
      );

      // LFO delay.
      drawSlider4(ui_display(),
        COL_W * 2 + 7, BODY_Y,
        SL_W, BODY_H,
        patch_.common.lfoDelay,
        0, 127,
        true, false
      );

      // Pitch modulation depth.
      drawSlider4(ui_display(),
        COL_W * 3 + 7, BODY_Y,
        SL_W, BODY_H,
        patch_.common.lfoPMD,
        0, 127,
        true, false
      );

      drawDividers(ui_display(), 4, BODY_Y, BODY_H);
    }



    inline void drawEditLfo2_() {
      ui_display().setTextScale(UITextScale::X1);
      ui_display().setBrush(UIDisplayBrush::SOLID);
      ui_display().setColor(UIDisplayColor::WHITE);

      constexpr int COL_W = 32;
      constexpr int ALGO_Y = 11;
      constexpr int ALGO_H = 19;
      constexpr int BODY_Y = 31;
      constexpr int BODY_H = 33;

      // Yamaha LFO page 2.
      const char* title = "LFO PMD On/Off";
      const int titleW = ui_display().getTextWidth(title);
      ui_display().drawText((ui_display().getWidth() - titleW) / 2, 0, title);
      drawPager(ui_display(), 0, 0, 3, 1);

      // Compact current-algorithm topology, as on the reference screen.
      drawAlgo(ui_display(),
        ALGO_Y, patch_.common.algorithm,
        ALGO_H, false
      );

      // Yamaha renders enabled as "ON" and disabled as "off".
      for (uint8_t op = 0; op < 4; ++op) {
        const int x0 = op * COL_W;

        if (patch_.ops[op].lfoPMDEnable) {
          ui_display().setTextScale(UITextScale::X2);
          const char* state = "ON";
          const int stateW = ui_display().getTextWidth(state);
          ui_display().drawText(x0 + (COL_W - stateW) / 2, BODY_Y + 7, state);
        } else {
          ui_display().setTextScale(UITextScale::X1);
          const char* state = "off";
          const int stateW = ui_display().getTextWidth(state);
          ui_display().drawText(x0 + (COL_W - stateW) / 2, BODY_Y + 12, state);
        }
      }

      ui_display().setTextScale(UITextScale::X1);
      drawDividers(ui_display(), 4, BODY_Y, BODY_H);
    }


    inline void drawEditLfo3_() {
      ui_display().setTextScale(UITextScale::X1);
      ui_display().setBrush(UIDisplayBrush::SOLID);
      ui_display().setColor(UIDisplayColor::WHITE);

      constexpr int COL_W = 32;
      constexpr int ALGO_Y = 11;
      constexpr int ALGO_H = 19;
      constexpr int BODY_Y = 33;
      constexpr int BODY_H = 30;
      constexpr int SL_W = 18;

      // Yamaha LFO page 3: per-operator amplitude modulation depth.
      const char* title = "LFO AMD";
      const int titleW = ui_display().getTextWidth(title);
      ui_display().drawText((ui_display().getWidth() - titleW) / 2, 0, title);
      drawPager(ui_display(), 0, 0, 3, 2);

      // Compact current-algorithm topology, matching LFO page 2.
      drawAlgo(ui_display(),
        ALGO_Y, patch_.common.algorithm,
        ALGO_H, false
      );

      // One AMD value (0..127) per operator.
      for (uint8_t op = 0; op < 4; ++op) {
        drawSlider2(ui_display(),
          op * COL_W + 7, BODY_Y,
          SL_W, BODY_H,
          patch_.ops[op].lfoAMD,
          0, 127,
          true, false, 
          false
        );
      }

      drawDividers(ui_display(), 4, BODY_Y, BODY_H);
    }


    inline void drawEnvelope_(bool pitch,bool levels) {
      ui_display().setTextScale(UITextScale::X1);
      const uint8_t* r=pitch?patch_.common.pegRate:patch_.ops[selectedOp_].egRate;
      const uint8_t* l=pitch?patch_.common.pegLevel:patch_.ops[selectedOp_].egLevel;
      for(int i=0;i<4;++i)ui_display().drawText(i*32+8,0,toText(levels?l[i]:r[i]));
      drawEG(ui_display(),0,14,128,50,l,r,pitch?UIEnvType::ENV_PITCH:UIEnvType::ENV_AMP,levels?UIEnvMode::ENV_LEVELS:UIEnvMode::ENV_RATES);
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

    RDX_Patch& patch_;
    Page page_ = Page::HOME;
    uint8_t selectedOp_ = 0;
    UiPhaserView* phaserTune_ = nullptr;
    uint8_t* phaserTuneSel_ = nullptr;
    const volatile float* hostInputPeak_ = nullptr;
    const volatile float* hostOutputPeak_ = nullptr;
    bool needUpdate_ = false;
    int32_t updateCounter_ = 0; 

};

