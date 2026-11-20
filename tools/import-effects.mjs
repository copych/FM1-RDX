import fs from 'node:fs';
import crypto from 'node:crypto';
const files=['fx_base.h','fx_distortion.h','fx_touch_wah.h','fx_chorus.h','fx_flanger.h','fx_phaser.h','fx_delay.h','fx_reverb.h'];
const hashes={};
fs.mkdirSync('src/fx',{recursive:true});
const lit=x=>`${Math.fround(x).toExponential(9)}f`;
const array=a=>a.map(lit).join(',');
const f=Math.fround;
const comb=[47.3,53.7,71.1,89.5].map(ms=>Math.trunc(f(f(f(ms)/f(1000))*44100)));
const gains=Array.from({length:128},(_,p)=>comb.map(n=>Math.min(.95,10**(-3*(n/44100)/(.25*24**(p/127))))));
fs.writeFileSync('src/fx/coefficients.h',`#pragma once
// Offline coefficients: no runtime libm calls in the effects path.
constexpr float distortionTone[128]={${array(Array.from({length:128},(_,p)=>1-Math.exp(-2*Math.PI*(300+(p/127)*(8000-300))/44100)))}};
constexpr float reverbFeedback[128][4]={${gains.map(a=>`{${array(a)}}`).join(',')}};
`);
for(const file of files) {
  const raw=fs.readFileSync(`RDX_before_UAC/${file}`);hashes[file]=crypto.createHash('sha256').update(raw).digest('hex');
  let s=raw.toString().replace(/\r\n/g,'\n');
  s=s.replace(/#include [<"](?:config.h|esp32-hal.h|cmath|cstring)[>"]\n/g,'')
    .replace(/#include "(RDX_Constants.h|misc.h)"/g,'#include "../core/$1"')
    .replace(/\s*ESP_LOG[DIEW]\([\s\S]*?\);/g,'')
    .replace(/\bIRAM_ATTR\b|\bvirtual\s+|\s+override\b/g,'')
    .replace(/__attribute__\(\(always_inline\)\)/g,'')
    .replace(/std::memset/g,'memset').replace(/std::min/g,'rdxMin')
    .replace(/\bunlikely\(([^)]*)\)/g,'($1)')
    .replace(/\bTWO_PI\b/g,'RDX_TWO_PI').replace(/\bM_PI\b/g,'RDX_PI');
  // The owning arena is cleared in bounded chunks before preparing a patch.
  s=s.replace(/\s*memset\((?:delayLine_[lr]_|buffer[LR]_|delayL_|combBuf_\[ch\]\[i\]|allBuf_\[ch\]\[i\])[^;]*;/g,'');
  if(file==='fx_base.h') {
    s=s.replace('#pragma once','#pragma once\n#include "support.h"')
      .replace(' = 0;', ' {}'); // concrete base: no vtables or delete runtime
  }
  if(file==='fx_distortion.h') {
    s=s.replace('float fc = 300.f + tone_ * (8000.f - 300.f);','');
    s=s.replace('float x = expf(-2.f * RDX_PI * fc * DIV_SAMPLE_RATE);\n        toneCoef_ = 1.f - x;',
      'const int param=(int)(tone_*127.0f+0.5f);\n        toneCoef_ = distortionTone[param < 0 ? 0 : param > 127 ? 127 : param];');
  }
  if(file==='fx_reverb.h') {
    s=s.replace(/float rt60 =[\s\S]*?(?=        lastTime_ = t;)/,
      'const unsigned param=(unsigned)(t*127.0f+0.5f);\n        for(int i=0;i<NUM_COMBS;++i) combGain_[i]=reverbFeedback[param>127 ? 127 : param][i];\n');
  }
  if(file==='fx_chorus.h') {
    s=s.replace('while (index < 0) index += MAX_DELAY;', 'index = wrap01(index * (1.0f / MAX_DELAY)) * MAX_DELAY;');
  }
  if(file==='fx_phaser.h') {
    s=s.replace('for (uint32_t i = 0; i < FLANGER_BUF_SIZE; ++i) delayL_[i] = delayR_[i] = 0.f;', '// Arena is already zeroed before reset.');
  }
  if(file==='fx_delay.h') {
    s=s.replace(/#ifdef BOARD_HAS_PSRAM[\s\S]*?#endif/, 'uint32_t MAX_DELAY = 2; // ring storage only; tap timing retains the source limit');
    s=s.replace('FxDelay() = default;', 'FxDelay() = default;\n    void setCapacity(uint32_t n) {MAX_DELAY=n;}');
    s=s.replace('for (int i = 0; i < frames; ++i)', 'for (uint32_t i = 0; i < frames; ++i)');
    s=s.replace(/delayLen_ = MAX_DELAY \/ 4/g, 'delayLen_ = SAMPLE_RATE / 4');
    s=s.replace('1, MAX_DELAY - 1','1, SAMPLE_RATE - 1');
  }
  // Keep float arithmetic throughout, including inherited unsuffixed constants.
  s=s.replace(/(?<![\w.])((?:\d+\.\d*|\.\d+)(?:[eE][+-]?\d+)?|\d+[eE][+-]?\d+)(?![\w.])/g,'$1f');
  fs.writeFileSync(`src/fx/${file}`,'// Adapted from the user snapshot; original notices below are retained.\n'+s);
}
fs.writeFileSync('effects-lock.json',JSON.stringify({source:'RDX_before_UAC',sha256:hashes},null,2)+'\n');
console.log('Imported seven source effects and base; generated float coefficient tables.');
