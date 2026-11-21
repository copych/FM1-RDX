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
