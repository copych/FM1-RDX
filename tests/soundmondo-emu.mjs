import assert from 'node:assert/strict';
import fs from 'node:fs';
import {startEmu} from '../platform/fm1-base/tests/emu_harness.mjs';
const emu=await startEmu(process.argv[2]||'app/build/emu');emu.run(700);const ex=emu.ex;
assert.equal(typeof ex.web_sx_in,'function');
function send(frame){emu.send({type:'app',part:'midi',data:Uint8Array.from(frame)});}
function out(){return Uint8Array.from(emu.take('midi').flatMap(event=>event.msgs.flatMap(frame=>Array.from(frame))));}
send([0xf0,0x7e,0x7f,6,1,0xf7]);emu.run(40);
assert.deepEqual(Array.from(out()),[0xf0,0x7e,0x7f,6,2,0x43,0,0x41,0x53,6,3,0,0,0x7f,0xf7]);
for(const name of fs.readdirSync('RDX_before_UAC/data/patches').filter(n=>n.endsWith('.syx')).sort()){
    const wire=fs.readFileSync('RDX_before_UAC/data/patches/'+name);
    for(let i=0;i<wire.length;){const end=wire.indexOf(0xf7,i);assert(end>=0);send(wire.subarray(i,end+1));i=end+1;}
    emu.run(100);send([0xf0,0x43,0x20,0x7f,0x1c,5,0x0e,0x0f,0,0xf7]);emu.run(80);
    assert.deepEqual(Buffer.from(out()),wire,name+' capture matches load');
}
console.log('PASS: actual WebAssembly Reface identity and 33 byte-exact SysEx load/capture round trips.');
