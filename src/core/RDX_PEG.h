// Adapted from RDX_before_UAC; see docs/PORT.md and source-lock.json.
#pragma once
#include "rdx_platform.h"
#include "RDX_Constants.h"

class  RDX_PEG {
public:
    enum class Stage { ATTACK, DECAY1, DECAY2, RELEASE, SUSTAIN, IDLE };

    inline void initPEG(const uint8_t rates[4], const uint8_t levels[4], bool need_reset = true) {
        for (int i = 0; i < 4; ++i) {
            rateIndices_[i]  = rates[i];   // 0..127
            levelTargets_[i] = levels[i]; // center at 64
        }
        if (need_reset) reset();
    }


    inline  __attribute__((always_inline))   float outVal() const { return current_ - CENTER; }
 
    inline  __attribute__((always_inline))   float processPEG() {
        if (stage_ == Stage::IDLE) return outVal();

        if (stage_ == Stage::SUSTAIN) {
            if (!gate_) enterStage(Stage::RELEASE);
            return outVal();
        }

        if ( rising_ ) { // rising
            // linear progression 
            current_ += stepIncrement_;
            if (current_ >= border_) {
                current_ = border_;
                advanceStage();
            }
        } else {            // falling
            // linear progression 
            current_ += stepIncrement_;
            if (current_ <= border_) {
                current_ = border_;
                advanceStage();
            }
        }

        return outVal();
    }

    inline void gate(bool g) {
        bool was = gate_;
        gate_ = g; 
        if (g && !was) {
            if (stage_ == Stage::IDLE) current_ = CENTER; 
            RDX_FAR(controlStage)(this,Stage::ATTACK);
        } else if (!g && was) {
            if (stage_ != Stage::IDLE && stage_ != Stage::RELEASE) RDX_FAR(controlStage)(this,Stage::RELEASE);
        }
    }
 
    inline __attribute__((always_inline)) void reset() {
        current_ = CENTER;
        border_ = CENTER;
        stage_ = Stage::IDLE;
        gate_ = false;
        rising_ = true;
        stepIncrement_ = 0.0f;
    }

    inline Stage getStage() const { return stage_; }
    inline bool isActive() const { return stage_ != Stage::IDLE; }
    inline bool isGateOn() const { return gate_; }

private:
    static RDX_RAM_CODE void controlStage(RDX_PEG* self,Stage s){self->enterStage(s);}
    RDX_RAM_CODE void enterStage(Stage s) {
        stage_  = s; 
        border_ = stageTarget(s)  ;
        float speed;
        // sustain just holds
        if (s == Stage::SUSTAIN) {
            rising_    = false;
            return;
        }

        if (s == Stage::IDLE) {

            return;
        }

        // rising or falling?
        rising_ = (border_ > current_) ;

        target_ = border_;

        // fetch coefficient from tables
        const int idx = (int)rateIndices_[(int)s];
		
        if (rising_) {
            // Rising : compute linear per-sample increment
            speed = PEG_SPEED[idx];
            stepIncrement_ =   speed * DIV_SAMPLE_RATE ; 
        } else {
            // Falling : compute linear per-sample increment
            speed = PEG_SPEED[idx]; 
            stepIncrement_ = -speed * DIV_SAMPLE_RATE * 1.1f ; 
        }
    }

    RDX_RAM_CODE void advanceStage() {
        switch (stage_) {
            case Stage::ATTACK: 
                enterStage(gate_ ? Stage::DECAY1 : Stage::RELEASE); 
                break;
            case Stage::DECAY1:  
                enterStage(gate_ ? Stage::DECAY2 : Stage::RELEASE); 
                break;
            case Stage::DECAY2:  
                enterStage(gate_ ? Stage::SUSTAIN : Stage::RELEASE); 
                break;
            case Stage::SUSTAIN: 
                if (!gate_) { 
                    enterStage(Stage::RELEASE); 
                } 
                break;
            case Stage::RELEASE: 
                stage_ = Stage::IDLE; 
                enterStage(Stage::IDLE); 
                break;
            default: break;
        } 
    }

    RDX_RAM_CODE float stageTarget(Stage s) const {
        switch (s) {
            case Stage::ATTACK:  return levelTargets_[0];
            case Stage::DECAY1:  return levelTargets_[1];
            case Stage::DECAY2:  return levelTargets_[2];
            case Stage::RELEASE: return levelTargets_[3];
            case Stage::SUSTAIN: return levelTargets_[2];
            default:             return outVal();
        }
    }

	
    const float CENTER = 64.0f;
	volatile float current_ = CENTER;  // output level id
    float border_  = CENTER;  // stage target level
    float target_ = CENTER; // control value
	float stepIncrement_= 0.0f;  // per-sample increment
    bool rising_ = false;  // rising/falling 
    Stage stage_ = Stage::IDLE;
    bool gate_ = false;

    float rateIndices_[4] = {0.f,0.f,0.f,0.f};     // ATT, D1, D2, REL
    float levelTargets_[4] = {CENTER,CENTER,CENTER,CENTER}; // L1..L4
};
