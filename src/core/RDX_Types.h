// Adapted from RDX_before_UAC; see docs/PORT.md and source-lock.json.
// RDX_Types.h
#pragma once
#include "rdx_platform.h"

// ===============================
// Reface DX data types & structs
// ===============================

// ---------------------------------
// Enums
// ---------------------------------

// live update dirty flags
enum GlobalDirtyFlags : uint8_t {
    GDIRTY_NONE  = 0,
    GDIRTY_ALGO  = 1 << 0,
    GDIRTY_LFO   = 1 << 1,
    GDIRTY_MOD   = 1 << 2   // peg + pmDepth + enables
};

enum DirtyFlags : uint8_t {
    DIRTY_NONE      = 0,
    DIRTY_FREQ      = 1 << 0,
    DIRTY_GAIN      = 1 << 1,
    DIRTY_FEEDBACK  = 1 << 2,
    DIRTY_ENV       = 1 << 3,
    DIRTY_SCALING   = 1 << 4,
    DIRTY_ALL       = 0xFF
};


// Frequency mode
enum RDX_FreqMode : uint8_t {
    RDX_FREQ_RATIO = 0,
    RDX_FREQ_FIXED = 1
};

// Feedback type
enum RDX_FeedbackType : uint8_t {
    RDX_FB_SAW    = 0,
    RDX_FB_SQUARE = 1
};

// Scaling curve types
enum RDX_ScaleCurve : uint8_t {
    RDX_SCALE_NEG_LIN = 0,
    RDX_SCALE_NEG_EXP = 1,
    RDX_SCALE_POS_EXP = 2,
    RDX_SCALE_POS_LIN = 3
};

// poly / mono mode
enum RDX_Mode : uint8_t {
    RDX_MODE_POLY = 0,
    RDX_MODE_MONO_FULL = 1,
    RDX_MODE_MONO_LEGATO = 2
};


// ---------------------------------------------------------
// Yamaha Reface DX checksum helper
// ---------------------------------------------------------
inline uint8_t rdxSyxChecksum(const uint8_t* data, uint32_t len) {
    // Sum all bytes, take lower 7 bits, subtract from 128, mask 7 bits
    uint32_t sum = 0;
    for (uint32_t i = 0; i < len; ++i) sum += data[i];
    return (128 - (sum & 0x7F)) & 0x7F;
}

struct RDX_Controls {
    int pitchbend = 0 ; // midi library gives it 0-centered
    float pitchbendSemitones = 0.0f;
    
    int mainVolume = 100; // cc#7
    float mainVolumeFactor = 1.0f; // 0.0 .. 1.0

    // Tuning
    float tuningSemitones = 0.0f;

    int modWheel = 0;
    float modWheelFactor = 0.0f;
    
    bool sustain = false; // sustain pedal CC#64
    
    bool portamento = false; // portamento CC#65
    
    float portaTimeS = 0.06f; // 60ms

    float freqCoef[4]       = {1.0f}; // for ratio/fixed freq modes, for each of 4 ops
    float detuneCoef[4]     = {1.0f};

	// bank / program
    uint32_t  bankMSB = 0;     // CC#0
    uint32_t  bankLSB = 0;     // CC#32
    uint32_t  program = 0;     // Program Change

    uint32_t  wantBankMSB = 0;     // CC#0
    uint32_t  wantBankLSB = 0;     // CC#32
    uint32_t  wantProgram = 0;     // Program Change desired
    
	// NRPN
    struct ParamPair { uint8_t msb = 0x7F, lsb = 0x7F; };
    ParamPair rpn;
    ParamPair nrpn;

    // Utility
    inline uint16_t getBank() const {
        return (bankMSB << 7) | bankLSB;
    }
    inline uint16_t getWantBank() const {
        return (wantBankMSB << 7) | wantBankLSB;
    }

    inline void setBank(uint16_t bank) {
        bankMSB = 0b01111111 & (bank >> 7);
        bankLSB = 0b01111111 & bank;
    }


};

// ---------------------------------------------------------
// System parameters (32 bytes)
// ---------------------------------------------------------
struct __attribute__((packed)) RDX_System {
    uint8_t txMidiChan = 0;     // 00-0F, 7F : 1-15, off
    uint8_t rxMidiChan = 0;  // 00-0F, 0x10 : 1-15, all
    uint8_t masterTune[4] = {0};// 4 lower bits of each element MSB to LSB: 15-12, 11-8, 7-4, 3-0 : -102.4 .. 102.3
    uint8_t localControl = 1;   // 00-01 : off/ON 
    uint8_t masterTranspose = 0x40;    // 34-4C : -12..+12 semitones 
    uint8_t tempo[2] = {0, 0x78};   // 7 bits of each MSB to LSB: 13-7, 6-0: tempo 30..300  
    uint8_t lcdContrast = 20;   // 00-3F
    uint8_t pedalModel = 1;     // 00-01 : FC3 or FC4/FC5 
    uint8_t autoPowerOff = 1;   // 00-01 : off/ON
    uint8_t speakerOn = 1;      // 00-01 : off/ON 
    uint8_t midiControl = 1;    // 00-01 : off/ON
    uint8_t reserved[17];
};

// ---------------------------------------------------------
// Common patch parameters (38 bytes)
// ---------------------------------------------------------
struct __attribute__((packed)) RDX_Common {
    uint8_t voiceName[10];   // 0..9  : VOICENAME (10 chars)
    uint8_t reserved1[2];    // 10..11: reserved
    uint8_t transpose ;  // 12    : TRANSPOSE
    uint8_t monoPoly;        // 13    : MONO/POLY
    uint8_t portaTime;       // 14    : PORTATIME
    uint8_t pbRange ;     // 15    : PB RANGE
    uint8_t algorithm;       // 16    : ALGORITHM
    uint8_t lfoWave;         // 17    : LFO WAVE
    uint8_t lfoSpeed;        // 18    : LFO SPEED
    uint8_t lfoDelay;        // 19    : LFO DELAY
    uint8_t lfoPMD;          // 20    : LFO PMD
    uint8_t pegRate[4];      // 21..24: PEG RATE1..4
    uint8_t pegLevel[4];     // 25..28: PEG LEVEL1..4
    uint8_t effects[2][3];  // 29    0: FX1 TYPE
                            // 30    1: FX1 PARAM1
                            // 31    2: FX1 PARAM2
                            // 32    0: FX2 TYPE
                            // 33    1: FX2 PARAM1
                            // 34    2: FX2 PARAM2
    uint8_t reserved2[3];    // 35..37: reserved
};

// ---------------------------------------------------------
// Operator parameters (28 bytes each, x4)
// ---------------------------------------------------------
struct __attribute__((packed)) RDX_OpParams {
    uint8_t enable;        // 0   : OP On/Off
    uint8_t egRate[4];     // 1-4 : EG RATE1..4
    uint8_t egLevel[4];    // 5-8 : EG LEVEL1..4
    uint8_t rateScaling;   // 9   : RATE SCALING
    uint8_t scaleLD;       // 10  : SCALING LD
    uint8_t scaleRD;       // 11  : SCALING RD
    uint8_t scaleLC;       // 12  : SCALING LC
    uint8_t scaleRC;       // 13  : SCALING RC
    uint8_t lfoAMD;        // 14  : LFO AMD
    uint8_t lfoPMDEnable;  // 15  : LFO PMD On/Off
    uint8_t pegEnable;     // 16  : PEG On/Off
    uint8_t velSens;       // 17  : VELO SENS
    uint8_t outLevel;      // 18  : OUT LEVEL
    uint8_t feedback;      // 19  : FEEDBACK
    uint8_t fbType;        // 20  : FB TYPE
    uint8_t freqMode;      // 21  : FREQ MODE
    uint8_t freqCoarse;    // 22  : FREQ COARSE
    uint8_t freqFine;      // 23  : FREQ FINE
    uint8_t freqDetune;    // 24  : FREQ DETUNE
    uint8_t reserved[3];   // 25-27: reserved
};

// ---------------------------------------------------------
// Full patch (38 + 4*28 = 150 bytes)
// ---------------------------------------------------------
struct __attribute__((packed)) RDX_Patch {
    RDX_Common common;       // 38 bytes
    RDX_OpParams ops[4];     // 112 bytes
};

// ---------------------------------
// Bank Parameters (future)
// ---------------------------------
struct RDX_Bank {
    RDX_Patch patches[32];              // 32 patch slots
};


// The main state structure.
struct SynthState {
    RDX_System      system;
    RDX_Patch       storedPatch;
    RDX_Patch       workingPatch;
    RDX_Controls    controls;
};


static_assert(sizeof(RDX_Common)==38, "common wire layout");
static_assert(sizeof(RDX_OpParams)==28, "operator wire layout");
static_assert(sizeof(RDX_Patch)==150, "patch wire layout");
