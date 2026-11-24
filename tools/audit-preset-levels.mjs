import fs from 'node:fs';
import {startEmu} from '../platform/fm1-base/tests/emu_harness.mjs';
const emu=await startEmu('app/build/emu');emu.run(700);
const midi=(s,a,b=0)=>emu.ex.web_midi_in(0,(s>>4)|(s<<8)|(a<<16)|(b<<24));
const sx=(hi,mid,lo,...data)=>emu.send({type:'app',part:'midi',data:Uint8Array.from([240,67,16,127,28,5,hi,mid,lo,...data,247])});
const set=(lo,...d)=>sx(48,0,lo,...d);
function readCommon(){emu.take('midi');emu.send({type:'app',part:'midi',data:Uint8Array.from([240,67,32,127,28,5,48,0,0,247])});emu.run(40);const frames=emu.take('midi').flatMap(x=>x.msgs);const frame=frames.find(x=>x[8]===48&&x.length===51);if(!frame)throw Error('No common patch readback');return frame.slice(11,49);}
function stats(ms){let peak=0,sum=0,n=0,rails=0;for(let b=0;b<Math.ceil(ms*44.1/128);b++){emu.quantum();for(const ch of emu.out)for(const x of ch){peak=Math.max(peak,Math.abs(x));sum+=x*x;n++;if(Math.abs(x)>.999)rails++;}}emu.take('frame');return {peak,rms:Math.sqrt(sum/n),rails,dbPeak:20*Math.log10(Math.max(peak,1e-12)),dbRms:10*Math.log10(Math.max(sum/n,1e-24))};}
function preset(p){midi(176,120);midi(192,p);emu.run(120);}
function play(notes,velocity=127){for(const note of notes)midi(144,note,velocity);}

const rows=[];for(let p=0;p<33;p++)for(const velocity of [100,127])for(const notes of [[48],[60],[72],[48,55,60,64,67,72,79]])for(const wet of [false,true]){
 preset(p===0?32:p-1);if(!wet)set(29,0,0,0,0,0,0);emu.run(100);play(notes,velocity);const m=stats(1000);if(!Number.isFinite(m.rms)||m.peak>0.89127)throw Error('Unbounded preset '+p);rows.push({p,velocity,notes,wet,...m});
}fs.writeFileSync('build/preset-level-audit.json',JSON.stringify(rows,null,2));console.log('PASS '+rows.length+' factory/Init dry/wet note, velocity and chord cases under -1 dBFS ceiling.');
