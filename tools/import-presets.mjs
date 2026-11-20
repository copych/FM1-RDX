import fs from 'node:fs';
import crypto from 'node:crypto';
const root='RDX_before_UAC/data/patches';
const names=fs.readdirSync(root).filter(n=>n.endsWith('.syx')).sort();
const patches=[],hashes={};
for(const name of names) {
  const bytes=fs.readFileSync(`${root}/${name}`);
  hashes[name]=crypto.createHash('sha256').update(bytes).digest('hex');
  let common=null;const ops=Array(4).fill(null);
  for(let i=0;i<bytes.length;) {
    const end=bytes.indexOf(0xf7,i);
    if(end<0 || end-i<9)throw Error(`${name}: truncated frame`);
    const f=bytes.subarray(i,end+1);i=end+1;
    if(f[0]!==0xf0 || f[1]!==0x43 || f[3]!==0x7f || f[4]!==0x1c || f[5]!==0)throw Error(`${name}: identity`);
    if(f.subarray(1,-1).some(v=>v>127))throw Error(`${name}: non-7-bit data`);
    const data=f.subarray(7,-2);
    if(((data.reduce((a,b)=>a+b,0)+f[f.length-2])&127)!==0)throw Error(`${name}: checksum`);
    if(f[6]===42) {
      if(data.length!==42 || data[0]!==5 || data[1]!==0x30 || data[2]!==0 || data[3]!==0 || common)throw Error(`${name}: common block`);
      common=data.subarray(4);
    } else if(f[6]===32) {
      if(data.length!==32 || data[0]!==5 || data[1]!==0x31 || data[2]>3 || data[3]!==0 || ops[data[2]])throw Error(`${name}: operator block`);
      ops[data[2]]=data.subarray(4);
    } else if(f[6]!==4)throw Error(`${name}: unsupported frame`);
  }
  if(!common || ops.some(o=>!o))throw Error(`${name}: incomplete voice`);
  patches.push(`// ${name}\n{${[...Buffer.concat([common,...ops])].join(',')}}`);
}
fs.writeFileSync('src/preset_bank.h',`#pragma once\n// Generated from the user's local RDX snapshot. Asset licensing is not established.\nconstexpr uint8_t presetData[][150] = {\n${patches.join(',\n')}\n};\n`);
fs.writeFileSync('preset-lock.json',JSON.stringify({source:root,sha256:hashes},null,2)+'\n');
console.log(`Imported ${patches.length} complete voices; checksums verified.`);
