import assert from 'node:assert/strict';
import {startEmu} from '../platform/fm1-base/tests/emu_harness.mjs';

const emu=await startEmu(process.argv[2]||'app/build/emu');
assert.equal(emu.take('ready').length,1);
emu.run(700);
assert(emu.take('frame').some(f=>f.fb?.some(v=>v!==0)),'LCD draws');
assert.equal(emu.run(100),0,'boot is silent');
emu.send({type:'keys',mask:1<<7});
assert(emu.run(250)>0.005,'panel key sounds');
emu.send({type:'keys',mask:0});
const midi=(status,d1,d2=0)=>emu.ex.web_midi_in(0,(status>>4)|(status<<8)|(d1<<16)|(d2<<24));
midi(0xb0,120);emu.run(20);assert.equal(emu.run(50),0,'panic silences');
for(let p=0;p<33;p++) {
  midi(0xc0,p);midi(0x90,60,110);
  const samples=emu.record(150);
  assert(samples.some(v=>Math.abs(v)>0.00001),`preset ${p} sounds after program change`);
  assert(samples.every(v=>Number.isFinite(v)&&Math.abs(v)<=1),`preset ${p} is bounded`);
  midi(0x80,60);midi(0xb0,120);emu.run(20);
  assert.equal(emu.run(10),0,`preset ${p} panic`);
}
emu.send({type:'enc',role:2,n:1});emu.run(20);
emu.send({type:'keys',mask:1<<7});assert(emu.run(200)>0.00001,'PRESET encoder leaves a playable patch');
console.log('PASS: WebAssembly boots, LCD, panel key, preset encoder, all 33 MIDI programs, finite audio, and panic.');
// Audible comparison of the same WobbleBass patch, note and velocity.
midi(0xb0,120);emu.send({type:'keys',mask:0});midi(0xc0,1);emu.run(100);
midi(0x90,60,100);const wet=emu.record(1000);
midi(0xb0,120);emu.run(100);
emu.send({type:'buttons',mask:3});emu.run(30);emu.send({type:'buttons',mask:0});
midi(0xc0,1);emu.run(100);midi(0x90,60,100);const dry=emu.record(1000);
assert(wet.some((v,i)=>Math.abs(v-dry[i])>0.0001),'FX button changes WobbleBass audio');
const {writeFileSync}=await import('node:fs');
function wav(samples,file) {
  const data=Buffer.alloc(44+samples.length*2);
  data.write('RIFF');data.writeUInt32LE(data.length-8,4);data.write('WAVEfmt ',8);
  data.writeUInt32LE(16,16);data.writeUInt16LE(1,20);data.writeUInt16LE(1,22);
  data.writeUInt32LE(44100,24);data.writeUInt32LE(88200,28);data.writeUInt16LE(2,32);data.writeUInt16LE(16,34);
  data.write('data',36);data.writeUInt32LE(samples.length*2,40);
  samples.forEach((v,i)=>data.writeInt16LE(Math.trunc(Math.max(-1,Math.min(1,v))*32767),44+i*2));
  writeFileSync(file,data);
}
wav(wet,'build/wobble-wet.wav');wav(dry,'build/wobble-dry.wav');
console.log('PASS: FX wet/dry toggle; WobbleBass comparison WAV files saved.');
// Exercise documented controllers through real USB-MIDI packet parsing in wasm.
midi(0xb0,120);midi(0xc0,32);emu.run(100);midi(0x90,69,100);emu.run(200);
const frequency=s=>{let crossings=0;for(let i=1;i<s.length;++i)if(s[i-1]<=0&&s[i]>0)++crossings;return crossings*44100/s.length;};
assert(Math.abs(frequency(emu.record(500))-440)<4,'Init at A4');
midi(0xe0,127,127);emu.run(20);assert(Math.abs(frequency(emu.record(500))-493.883)<4,'14-bit bend +2 semitones');
midi(0xe0,0,0);emu.run(20);assert(Math.abs(frequency(emu.record(500))-391.995)<4,'14-bit bend -2 semitones');
midi(0xb0,7,0);emu.run(200);assert(emu.record(100).every(v=>Math.abs(v)<1e-6),'CC7 mutes the output');
midi(0xb0,121,0);emu.run(50);assert(emu.record(100).every(v=>Math.abs(v)<1e-6),'Reset Controllers preserves volume');
midi(0xb0,7,127);emu.run(200);assert(Math.abs(frequency(emu.record(500))-440)<4,'Reset Controllers centers bend');
const button=mask=>{emu.send({type:'buttons',mask});emu.run(30);emu.send({type:'buttons',mask:0});emu.run(30);};
button(1<<5);button(1<<5); // GLO twice -> MIDI settings
emu.send({type:'enc',role:3,n:-127});emu.run(30); // RX = ALL, retires old notes
midi(0x9f,69,100);assert(emu.run(250)>0.001,'Omni receives notes on channel 16');
midi(0xbf,120);emu.run(50);assert.equal(emu.run(20),0,'All Sound Off on selected/omni channel');
console.log('PASS: WebAssembly audible pitch bend, volume/reset controllers and omni reception.');
// Compare rendered level values with exact-step references, through the worklet.
button(1<<6);emu.send({type:'enc',role:0,n:1});emu.run(50); // HOME -> LEVEL overview
const turn=(n,ms)=>{emu.send({type:'enc',role:3,n});emu.run(ms);};
const screen=()=>{const frames=emu.take('frame').filter(f=>f.fb);assert(frames.length);return frames.at(-1).fb.slice();};
turn(-127,200);emu.take('frame');for(let i=0;i<3;i++)turn(1,20);emu.run(40);const fastScreen=screen();
turn(-127,200);emu.take('frame');emu.send({type:'buttons',mask:2});turn(10,40);emu.send({type:'buttons',mask:0});emu.run(40);
assert.deepEqual(screen(),fastScreen,'Three fast turns equal ten precise level units');
turn(-127,200);emu.take('frame');for(let i=0;i<3;i++)turn(1,200);const slowScreen=screen();
turn(-127,200);emu.take('frame');emu.send({type:'buttons',mask:2});turn(3,40);emu.send({type:'buttons',mask:0});emu.run(40);
assert.deepEqual(screen(),slowScreen,'Three slow turns equal three precise level units');
assert.notDeepEqual(fastScreen,slowScreen,'Fast and slow rotations produce different displayed values');
console.log('PASS: WebAssembly fast/slow knob acceleration and SEL precision override.');
button(1<<6); // HOME; diagnostic entry starts from normal rendering.
for(let i=0;i<3;i++){
  emu.send({type:'buttons',mask:2});emu.run(20);
  emu.send({type:'buttons',mask:2|(1<<11)});emu.run(20);
  emu.send({type:'buttons',mask:0});emu.run(20);
}
button(1<<7); // SAVE enables split diagnostic rendering.
midi(0xc0,32);emu.run(150); // Program selection returns home and disables profiling.
for(let i=0;i<3;i++){
  emu.send({type:'buttons',mask:2});emu.run(20);
  emu.send({type:'buttons',mask:2|(1<<11)});emu.run(20);
  emu.send({type:'buttons',mask:0});emu.run(20);
}
button(1<<7);midi(0x90,69,110);
const profiled=emu.record(700);assert(profiled.every(Number.isFinite)&&profiled.some(v=>Math.abs(v)>0.001),'Profile renderer plays finite audio');
emu.send({type:'enc',role:0,n:1});emu.run(50);
const fineProfiled=emu.record(200);assert(fineProfiled.every(Number.isFinite)&&fineProfiled.some(v=>Math.abs(v)>0.001),'Fine profile renderer plays finite audio');
button(1<<6);const normalAgain=emu.record(100);assert(normalAgain.every(Number.isFinite)&&normalAgain.some(v=>Math.abs(v)>0.001),'HOME restores normal renderer on held note');
midi(0xb0,120);emu.run(50);assert.equal(emu.run(20),0,'panic after profiling');
console.log('PASS: WebAssembly profile entry, audio, preset-home exit, live return to normal and panic.');
