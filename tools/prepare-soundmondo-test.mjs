import fs from 'node:fs';
const hex=a=>Array.from(a,v=>v.toString(16).padStart(2,'0')).join(' ');
const wire=fs.readFileSync('RDX_before_UAC/data/patches/12-WobbleBass.syx');
const frames=[];for(let i=0;i<wire.length;){const e=wire.indexOf(0xf7,i);if(e<0)throw Error('fixture');frames.push(wire.subarray(i,e+1));i=e+1;}
const lines=['wait 600','sysex f0 7e 7f 06 01 f7','wait 40','expect_sysex f0 7e 7f 06 02 43 00 41 53 06 03 00 00 7f f7','sysex_out'];
for(const frame of frames)lines.push('sysex '+hex(frame));
lines.push('wait 100','sysex f0 43 20 7f 1c 05 0e 0f 00 f7','wait 100');
for(const frame of frames)lines.push('expect_sysex '+hex(frame));
lines.push('sysex_out','sysex f0 43 10 7f 1c 05 31 00 12 37 f7','sysex f0 43 10 7f 1c 05 31 01 12 38 f7','wait 100','expect op1_level 55','expect op2_level 56','expect page 0',
 'sysex f0 43 30 7f 1c 05 31 00 12 f7','wait 40','expect_sysex f0 43 10 7f 1c 05 31 00 12 37 f7',
 'sysex f0 43 10 7f 1c 05 00 00 01 10 f7','wait 40','expect midi_rx 0','expect store_writes 0');
fs.mkdirSync('build/soundmondo-controls',{recursive:true});fs.writeFileSync('build/soundmondo-controls.txt',lines.join('\n')+'\n');
