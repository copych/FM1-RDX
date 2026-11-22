import fs from 'node:fs';
import crypto from 'node:crypto';
const hash=path=>crypto.createHash('sha256').update(fs.readFileSync(path)).digest('hex');
const source={};
function visit(root) {
  for(const e of fs.readdirSync(root,{withFileTypes:true})) {
    if(['build','web'].includes(e.name))continue;
    const p=`${root}/${e.name}`;
    if(e.isDirectory())visit(p);else source[p]=hash(p);
  }
}
for(const d of ['src','app','tools','tests'])visit(d);
for(const p of ['source-lock.json','preset-lock.json','effects-lock.json','gui-lock.json'])source[p]=hash(p);
for(const p of ['platform/fm1-base/device/lcd.c','platform/fm1-base/bsp/boards/fm1/fm1_lcd_hw.h','platform/fm1-base/bsp/tests/lcd_hw_test.c'])source[p]=hash(p);
const artifacts={};
for(const p of ['app/build/rdx.fwsc','app/build/rdx.bin','app/build/rdx.elf','app/build/loader/ota.bin','app/build/emu/app.wasm'])
  artifacts[p]={bytes:fs.statSync(p).size,sha256:hash(p)};
fs.writeFileSync('build/evidence.json',JSON.stringify({source,artifacts},null,2)+'\n');
console.log(JSON.stringify(artifacts,null,2));
