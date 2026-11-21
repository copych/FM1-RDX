// Extract the user's RDX snapshot reproducibly; never modify that snapshot.
import fs from 'node:fs';
import crypto from 'node:crypto';
const names = ['RDX_Types.h', 'RDX_State.h', 'RDX_Constants.h', 'RDX_Envelope.h',
  'RDX_PEG.h', 'RDX_LFO.h', 'RDX_Operator.h', 'RDX_Voice.h'];
fs.mkdirSync('src/core', {recursive:true});
const hashes = {};
const literal = x => `${Math.fround(x).toExponential(9)}f`;
const array = a => a.map(literal).join(',\n');
// Preserve the source's float operation boundaries, including powf arguments.
const f=Math.fround;
const sourceGain=x=>f(f(10 ** f(-f(f(127-f(x))*48)/f(20*127)))-f(0.003981071705533));
const forward = Array.from({length:192}, (_,i) => sourceGain(i));
forward[0] = 0;
const max = forward[191];
const inverse = Array.from({length:192}, (_,i) => {
  const target = f(f(max*i)/191);
  let lo=0, hi=191;
  while(hi-lo>1) {const mid=(lo+hi)>>1; if(forward[mid]<target)lo=mid;else hi=mid;}
  const fraction=f(f(target-forward[lo])/f(f(forward[hi]-forward[lo])+f(1e-12)));
  return f(lo+f(fraction*(hi-lo)));
});
for(const name of names) {
  const original = fs.readFileSync(`RDX_before_UAC/${name}`);
  hashes[name] = crypto.createHash('sha256').update(original).digest('hex');
  let s = original.toString().replace(/\r\n/g,'\n');
  s = s.replace(/#include <Arduino.h>/g, '#include "rdx_platform.h"')
    .replace(/#include "esp_log.h"\n/g,'')
    .replace(/\bIRAM_ATTR\b|\bDRAM_ATTR\b/g,'')
    .replace(/class\s+__attribute__\(\(always_inline\)\)/g,'class');
  // Remove logging calls, including calls reachable from envelope transitions.
  s = s.replace(/\s*ESP_LOG[DIEW]\([\s\S]*?\);/g,'');
  if(name==='RDX_Types.h') {
    s = s.slice(0,s.indexOf('static inline bool patchToSyx'));
    s = s.replace('#include <cstdint>', '#include "rdx_platform.h"');
    s = s.replace(/\/\/ Convenience overload[\s\S]*?(?=struct RDX_Controls)/,'');
    s += '\nstatic_assert(sizeof(RDX_Common)==38, "common wire layout");\nstatic_assert(sizeof(RDX_OpParams)==28, "operator wire layout");\nstatic_assert(sizeof(RDX_Patch)==150, "patch wire layout");\n';
  }
  if(name==='RDX_Constants.h') {
    const start=s.indexOf('constexpr inline float mapLevel');
    const end=s.indexOf('//----------------------- LFO SPEED');
    s=s.slice(0,start)+`// Generated offline: no constexpr libm extensions or startup constructors.
constexpr int LEVEL_LUT_SIZE = 192;
struct LevelLUT { std::array<float,192> forward; std::array<float,192> inverse; float maxValue; };
constexpr LevelLUT levelLUT = {{{${array(forward)}}}, {{${array(inverse)}}}, ${literal(max)}};
constexpr float LEVEL_LUT_MAX = levelLUT.maxValue;
constexpr float envelopeGain[3057]={${array(Array.from({length:3057},(_,i)=>sourceGain(i/16)))}};
inline float rdxEnvelopeGain(float level) {
    const float x=fclamp(level,0.0f,191.0f)*16.0f;
    const int i=(int)x;
    if(i==3056)return envelopeGain[3056];
    return envelopeGain[i]+(x-i)*(envelopeGain[i+1]-envelopeGain[i]);
}
inline float rdxGain(float level) {
    level = fclamp(level, 0.0f, 191.0f);
    int i = (int)level;
    if (i == 191) return levelLUT.forward[191];
    return levelLUT.forward[i] + (level-i)*(levelLUT.forward[i+1]-levelLUT.forward[i]);
}
inline float rdxGainInv(float value) {
    float scaled = fclamp(value / LEVEL_LUT_MAX * 191.0f, 0.0f, 191.0f);
    int i = (int)scaled;
    if (i == 191) return levelLUT.inverse[191];
    return levelLUT.inverse[i] + (scaled-i)*(levelLUT.inverse[i+1]-levelLUT.inverse[i]);
}
\n`+s.slice(end);
    s=s.replace('float idxf = offset + (float)LFO_SEMITONE_LUT_CENTER;',
      'float idxf = fclamp(offset + (float)LFO_SEMITONE_LUT_CENTER, 0.0f, 254.0f);');
    const a=s.indexOf('constexpr float harmonics');
    const b=s.indexOf('// phase in [0..1)',a);
    s=s.slice(0,a)+`constexpr std::array<float,1025> sinTable = {{${array(Array.from({length:1025},(_,i)=>Math.sin(f(i*2*Math.PI/1024))))}}};\n`+s.slice(b);
    s=s.replace('    float s0   = sinTable[idx];',
      '    // Float wrapping can round a tiny negative phase up to exactly 1.\n    // Keep the interpolation pair in the periodic table, including that seam.\n    idx &= SINLUT_SIZE - 1;\n    float s0   = sinTable[idx];');
    // All inherited table values are single precision constants.
    s=s.replace(/(?<![\w.])((?:\d+\.\d*|\.\d+)(?:[eE][+-]?\d+)?|\d+[eE][+-]?\d+)(?![\w.])/g,'$1f');
    s=s.replace('#include <array>','').replace(/std::array<float,192>/g,'RdxArray<float,192>')
      .replace(/std::array<float,1025>/g,'RdxArray<float,1025>');
  }
  if(name==='RDX_Envelope.h') {
    s=s.replace(/mapLevel\(/g,'rdxEnvelopeGain(')
      .replace('k_ = V0 / currentL_;','k_ = 0.0f;')
      .replace('c_ = 0.0f;\n            }','c_ = V0;\n            }');
    // Reface reference pp.5-6: key-off starts Rate 4, including mono/glide.
    const a=s.indexOf('    inline void gate(bool g)');
    const b=s.indexOf('    inline Stage getStage()',a);
    s=s.slice(0,a)+`    inline void gate(bool g) {
        const bool was=gate_;gate_=g;
        if(g&&!was){
            if(stage_==Stage::IDLE||stage_==Stage::RELEASE)currentL_=0.0f;
            enterStage(Stage::ATTACK);
        } else if(!g&&was&&stage_!=Stage::IDLE&&stage_!=Stage::RELEASE)enterStage(Stage::RELEASE);
    }
\n`+s.slice(b);
  }
  if(name==='RDX_LFO.h') {
    s=s.replace('delay_ = DELAY_TIME_MS[delayParam] * 0.001f;', 'delay_ = delayParam ? DELAY_TIME_MS[delayParam] * 0.001f : 0.0f;');
    const a=s.indexOf('        // --- delay / fade-in ---');
    const b=s.indexOf('        float startPhase',a);
    s=s.slice(0,a)+`        // Reference p.7: delay is a wait before modulation begins.
        if(delaySamples_>0){delaySamples_-=DMA_BUFFER_LEN;value_=increment_=0.0f;return;}
        fadeInEnv_+=fadeInIncrement_;if(fadeInEnv_>1.0f)fadeInEnv_=1.0f;
\n`+s.slice(b);
  }
  if(name==='RDX_Voice.h') {
    s=s.replace('    inline float calcScore() {',`    inline bool isAttacking() const {
        unsigned mask=1u;
        if(algorithm_==5||algorithm_==6)mask=3u;
        else if(algorithm_==7)mask=5u;
        else if(algorithm_>=8&&algorithm_<=10)mask=7u;
        else if(algorithm_==11)mask=15u;
        for(int i=0;i<4;++i)if((mask&(1u<<i))&&ops_[i].isAttacking())return true;
        return false;
    }
    inline float calcScore() {`);
    s=s.replace('        return score_;\n    }','        if(isAttacking())score_+=1000000.0f; // protect developing attacks\n        return score_;\n    }');
    s=s.replace('portaOffsetSemitones = currentNote - noteOnBaseNote_;','currentNoteSemitone_ = currentNote;\n        portaOffsetSemitones = currentNote - noteOnBaseNote_;');
    s=s.replace('vTaskDelay(1); // invalid RDX_State ?', '// Invalid routing fails silent, with bounded work.');
    s=s.replace('uint8_t             note_;','uint8_t             note_ = 0;');
    s=s.replace('    float               portaSemitoneOffset_ = 0.0f;','');
    s=s.replace('currentNoteSemitone_  = 0.f;','currentNoteSemitone_  = 0.f;\n    lfoValue_ = 0.0f;\n    lfoIncrement_ = 0.0f;');
    s=s.replace('portamentoInc_        = 1.f / (ctl_.portaTimeS * (float)sampleRate_);',
      'portamentoInc_        = ctl_.portaTimeS > 0.0f ? 1.0f / (ctl_.portaTimeS * sampleRate_) : 1.0f;');
  }
  if(name==='RDX_Operator.h') {
    s=s.replace('    inline bool isActive() const', '    inline bool isAttacking() const {return params_.enable&&params_.outLevel&&env_.getStage()==RDX_Envelope::Stage::ATTACK;}\n    inline bool isActive() const');
    // Mute output without freezing the operator's envelope/phase state.
    s=s.replace('if (!params_.enable) return 0.f;','');
    s=s.replace('return fbAcc_ * outGain_ * env_.processAEG();','const float sample=fbAcc_ * outGain_ * env_.processAEG();\n        return params_.enable ? sample : 0.0f;');
    s=s.replace('if(phase_>1.0f) phase_ -= 1.0f;', 'phase_ = wrap01(phase_);');
    s=s.replace(': idx_(idx),\n          params_(RDX_State::getState().workingPatch.ops[idx])',
      ': params_(RDX_State::getState().workingPatch.ops[idx]), idx_(idx)');
  }
  s=s.replace(/#include <cmath>\n/g,'');
  fs.writeFileSync(`src/core/${name}`, '// Adapted from RDX_before_UAC; see docs/PORT.md and source-lock.json.\n'+s);
}
fs.writeFileSync('source-lock.json',JSON.stringify({source:'RDX_before_UAC',sha256:hashes,
  fm1_base:'7932dff6f1474cc8c3420e0df75eb75879e47319',
  felucca_bsp:'f869dfa8dc3c081f040407e370d303349e4444d5'},null,2)+'\n');
fs.writeFileSync('src/core/control_tables.h', `#pragma once
// Generated offline from the source formulas; note range -24..152.
constexpr float noteHz[177] = {${array(Array.from({length:177},(_,i)=>f(440*f(2**f((i-24-69)/12)))))}};
constexpr float detuneRatio[128] = {${array(Array.from({length:128},(_,i)=>f(1.00033913)**(i-64)))}};
constexpr float fixedFine[100] = {${array(Array.from({length:100},(_,i)=>f(9.772)**f(i*f(0.01010101))))}};
`);
console.log('Imported 8 core headers; original files unchanged.');
