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
const rows=[];
for(const fb of [0,127])for(const count of [1,7]){preset(32);set(13,0);set(16,11);set(29,0,0,0,0,0,0);for(let op=0;op<4;op++){sx(49,op,18,127);sx(49,op,19,fb);}emu.run(150);play(Array(count).fill(60));rows.push({kind:'maximum',fb,count,...stats(2400)});}
for(let p=0;p<33;p++)for(const wet of [false,true]){preset(p===0?32:p-1);if(!wet)set(29,0,0,0,0,0,0);emu.run(100);play([48,55,60,64,67,72,79],127);rows.push({kind:'preset',p,wet,...stats(2400)});}
fs.writeFileSync('build/level-baseline.json',JSON.stringify(rows,null,2));console.log(JSON.stringify(rows.filter(r=>r.kind==='maximum')));
// A single reusable sound avoids note-envelope changes in wet/dry comparisons.
const results=[];
for(const level of [85,127])for(let id=0;id<8;id++)for(const p1 of [0,32,64,96,127])for(const p2 of [0,32,64,96,127]){
 if(id===0&&(p1!==64||p2!==64))continue;
 preset(32);set(13,0);set(16,11);for(let op=0;op<4;op++){sx(49,op,18,level);sx(49,op,19,0);}set(29,id,p1,p2,0,0,0);emu.run(180);const patch=readCommon();if(patch[29]!==id||patch[30]!==p1||patch[31]!==p2){results.push({level,id,p1,p2,skipped:true,reason:"FX setting rejected by arena budget"});continue;}play([48,55,60,64],127);const onset=stats(1000),body=stats(3000);midi(176,123);const tail=stats(1200);
 results.push({level,id,p1,p2,onset,body,tail});if(results.length%25===0){fs.writeFileSync('build/fx-level-baseline.json',JSON.stringify(results,null,2));console.log('FX cases '+results.length);}
}
fs.writeFileSync('build/fx-level-baseline.json',JSON.stringify(results,null,2));console.log('DONE '+results.length);
