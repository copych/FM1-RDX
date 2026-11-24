import fs from 'node:fs';
import {startEmu} from '../platform/fm1-base/tests/emu_harness.mjs';
const emu=await startEmu(process.argv[2]||'app/build/emu');emu.run(700);
const midi=(s,a,b=0)=>emu.ex.web_midi_in(0,(s>>4)|(s<<8)|(a<<16)|(b<<24));
const sx=(hi,mid,lo,...data)=>emu.send({type:'app',part:'midi',data:Uint8Array.from([240,67,16,127,28,5,hi,mid,lo,...data,247])});
const set=(lo,...d)=>sx(48,0,lo,...d);
function readCommon(){emu.take('midi');emu.send({type:'app',part:'midi',data:Uint8Array.from([240,67,32,127,28,5,48,0,0,247])});emu.run(40);const frames=emu.take('midi').flatMap(x=>x.msgs);const frame=frames.find(x=>x[8]===48&&x.length===51);if(!frame)throw Error('No common patch readback');return frame.slice(11,49);}
function stats(ms){let peak=0,sum=0,n=0,rails=0;const mean=[0,0];for(let b=0;b<Math.ceil(ms*44.1/128);b++){emu.quantum();for(let c=0;c<2;c++)for(const raw of emu.out[c]){const x=raw*10;peak=Math.max(peak,Math.abs(x));sum+=x*x;mean[c]+=x;n++;if(Math.abs(raw)>.999)rails++;}}emu.take('frame');const dc=mean.map(v=>v/(n/2)),totalPower=sum/n,acPower=Math.max(0,totalPower-(dc[0]*dc[0]+dc[1]*dc[1])*0.5);return {peak,totalRms:Math.sqrt(totalPower),dc,rms:Math.sqrt(acPower),rails,dbPeak:20*Math.log10(Math.max(peak,1e-12)),dbRms:10*Math.log10(Math.max(acPower,1e-24))};}
function preset(p){midi(176,120);midi(192,p);emu.run(120);}
function play(notes,velocity=127){for(const note of notes)midi(144,note,velocity);}

const files=['build/fx-level-baseline.json','build/fx-level-extra.json'];
for(const file of files){const rows=JSON.parse(fs.readFileSync(file));for(let k=0;k<rows.length;k++){const row=rows[k];if(row.skipped||row.body.peak<0.8905)continue;const {level,id,p1,p2}=row;
preset(32);set(13,0);set(16,11);for(let op=0;op<4;op++){sx(49,op,18,level);sx(49,op,19,0);}set(29,id,p1,p2,0,0,0);emu.run(180);const patch=readCommon();if(patch[29]!==id||patch[30]!==p1||patch[31]!==p2)throw Error('Rejected setting');play([48,55,60,64],127);const onset=stats(1000),body=stats(3000);midi(176,123);const tail=stats(1200);rows[k]={level,id,p1,p2,onset,body,tail,captureHeadroom:10};if(body.rails||body.peak>=8.9)throw Error('Capture overload');console.log('remeasured',level,id,p1,p2,body.rms);}
fs.writeFileSync(file,JSON.stringify(rows,null,2));}
