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

const results=[];const limit=0.89127;
function verify(m,label){if(!Number.isFinite(m.rms)||m.peak>limit)throw Error(label+' invalid/over ceiling: '+JSON.stringify(m));}
for(const fbType of [0,1])for(const fb of [0,127])for(const note of [36,60,84])for(const count of [1,7]){
 preset(32);set(13,0);set(16,11);set(29,0,0,0,0,0,0);for(let op=0;op<4;op++){sx(49,op,0,1);sx(49,op,18,127);sx(49,op,19,fb,fbType);}emu.run(180);play(Array(count).fill(note));const m=stats(2000);verify(m,'raw maximum');results.push({kind:'maximum',fbType,fb,note,count,...m});
}
const refs={};for(const level of [74,95,116]){preset(32);set(13,0);set(16,11);set(29,0,0,0,0,0,0);for(let op=0;op<4;op++){sx(49,op,18,level);sx(49,op,19,0);}emu.run(180);play([48,55,60,64]);stats(1000);refs[level]=stats(2000);}
for(const level of [74,95,116])for(let id=1;id<8;id++)for(const p1 of (id<=2?[16,48,80,112]:[16,80]))for(const p2 of (id<=2?[16,48,80,112]:[16,80])){
 preset(32);set(13,0);set(16,11);for(let op=0;op<4;op++){sx(49,op,18,level);sx(49,op,19,0);}set(29,id,p1,p2,0,0,0);emu.run(180);const p=readCommon();if(p[29]!==id||p[30]!==p1||p[31]!==p2)throw Error('Unexpected rejection');play([48,55,60,64]);const onset=stats(1000),m=stats(2000);midi(176,123);const tail=stats(1000);for(const v of [onset,m,tail])verify(v,'effect '+id);const delta=m.dbRms-refs[level].dbRms;results.push({kind:'interpolated',level,id,p1,p2,delta,onset,body:m,tail});if(results.length%25===0)console.log('validated '+results.length);
}
for(let a=0;a<8;a++)for(let b=0;b<8;b++){
 preset(32);set(13,0);set(16,11);for(let op=0;op<4;op++){sx(49,op,18,127);sx(49,op,19,127,1);}set(29,a,64,64,b,64,64);emu.run(200);const p=readCommon();if(p[29]!==a||p[32]!==b){results.push({kind:'chain',a,b,skipped:true});continue;}play(Array(7).fill(60));const onset=stats(600),body=stats(1800);midi(176,123);const tail=stats(600);for(const v of [onset,body,tail])verify(v,'chain '+a+'/'+b);if(body.rms<1e-7)throw Error('Silent excited chain '+a+'/'+b);results.push({kind:'chain',a,b,onset,body,tail});
}
fs.writeFileSync('build/levels-validation.json',JSON.stringify(results,null,2));const e=results.filter(r=>r.kind==='interpolated').map(r=>r.delta);console.log('PASS sample-peak ceiling across maximum feedback/types, independent parameter/input levels and two-slot chains. Independent RMS deltas '+Math.min(...e).toFixed(2)+'..'+Math.max(...e).toFixed(2)+' dB.');
