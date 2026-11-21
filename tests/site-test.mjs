import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import assert from 'node:assert/strict';
import {webcrypto,createHash} from 'node:crypto';
const dir=process.argv[2]||'build/site';
const installer=process.argv[3]||'install/index.html';
const html=fs.readFileSync(`${dir}/${installer}`,'utf8');
const script=html.match(/<script type="module">([\s\S]*?)<\/script>/)[1];
const meta=JSON.parse(script.match(/const meta = (\{[^\n]*\});/)[1]);
const raw=fs.readFileSync(path.resolve(dir,path.dirname(installer),meta.pkg));
assert.equal(createHash('sha256').update(raw).digest('hex'),meta.sha256);
assert.equal(meta.boot,null,'No bootloader package offered');
assert(!html.includes('{{NAME}}'));
assert(html.includes('hardware retesting is pending'));
const manifest=JSON.parse(fs.readFileSync(`${dir}/manifest.json`));
for(const [file,info] of Object.entries(manifest.files))assert.equal(createHash('sha256').update(fs.readFileSync(`${dir}/${file}`)).digest('hex'),info.sha256,file);
for(const file of ['index.html',installer,'emu/index.html']){
 const page=fs.readFileSync(`${dir}/${file}`,'utf8');
 for(const m of page.matchAll(/\b(?:href|src)="([^"#]+)"/g)){
  if(/^(?:https?:|data:|#)/.test(m[1]))continue;
  const p=path.resolve(dir,path.dirname(file),m[1]);
  // Upstream backup links are intentionally hidden when meta.backup is false.
  if(m[1]==='../backup/'&&file===installer)continue;
  assert(fs.existsSync(p),`${file}: missing ${m[1]}`);
 }
}
async function boot(bytes,midiAvailable=true){
 const elements=new Map();let midiRequests=0;
 const el=id=>{if(!elements.has(id))elements.set(id,{textContent:'',hidden:true,disabled:true,value:0,dataset:{},style:{},addEventListener(){},setAttribute(){},append(){}});return elements.get(id);};
 const sandbox={console,crypto:webcrypto,Uint8Array,Uint16Array,Uint32Array,DataView,Map,Set,TextEncoder,TextDecoder,performance,URL,setTimeout,clearTimeout,
  navigator:midiAvailable?{requestMIDIAccess:async()=>{midiRequests++;throw Error('Must not request MIDI at page load');}}:{},
  document:{getElementById:el,querySelectorAll:()=>[],documentElement:{lang:'en'}},window:{addEventListener(){}},localStorage:{getItem:()=>null,setItem(){}},
  fetch:async()=>({ok:true,arrayBuffer:async()=>Uint8Array.from(bytes).buffer})};
 vm.runInNewContext(script,sandbox);
 for(let i=0;i<30;i++){await new Promise(r=>setTimeout(r,10));if(el('go').disabled===false||el('alert').textContent)break;}
 assert.equal(midiRequests,0,'Page load never requests MIDI or sends device commands');
 return {go:el('go'),status:el('status'),alert:el('alert'),boot:el('mbootopt')};
}
const ok=await boot(raw);assert.equal(ok.go.disabled,false,'Valid pinned package enables install');assert.equal(ok.boot.hidden,true);
const bad=Buffer.from(raw);bad[2000]^=1;
const refused=await boot(bad);assert.equal(refused.go.disabled,true);assert(refused.alert.textContent.includes('damaged'),'Changed package refused');
const unsupported=await boot(raw,false);assert.equal(unsupported.go.disabled,true);assert(unsupported.alert.textContent,'Unsupported browser explained');
console.log('PASS: Pages assets/links/hashes, pinned installer boot, corruption rejection, unsupported browser, no automatic MIDI, no bootloader offer.');
