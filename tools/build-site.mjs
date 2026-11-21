// Assemble an explicitly experimental, pinned preview, not an upstream release.
// Installer adaptations retain the upstream MPL notice and update/package checks.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {productOf, logicalImage, checkUfw} from '../platform/fm1-base/web/fm1pkg.js';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
process.chdir(root);
const config=JSON.parse(fs.readFileSync('site/preview.json'));
const hash=data=>crypto.createHash('sha256').update(data).digest('hex');
const firmware=fs.readFileSync('app/build/rdx.fwsc');
const wasm=fs.readFileSync('app/build/emu/app.wasm');
if(hash(firmware)!==config.firmwareSha256||hash(wasm)!==config.wasmSha256)throw Error('Preview pin mismatch: test the new build before updating site/preview.json');
if(productOf(firmware)!==config.product)throw Error('Wrong firmware product');
checkUfw(logicalImage(firmware));
if(!firmware.includes(Buffer.from('FELUCCA-LOADER-1')))throw Error('Missing update loader');
// Refuse stale binaries when app or DSP source differs from recorded build inputs.
const evidence=JSON.parse(fs.readFileSync('build/evidence.json'));
for(const [file,digest] of Object.entries(evidence.source))if(file.startsWith('src/')||file.startsWith('app/')){
  if(hash(fs.readFileSync(file))!==digest)throw Error(`Rebuild evidence required: ${file}`);
}
const out=path.resolve('build/site');
fs.mkdirSync(path.join(out,'install'),{recursive:true});
fs.mkdirSync(path.join(out,'firmware'),{recursive:true});
fs.copyFileSync('site/index.html',path.join(out,'index.html'));
fs.writeFileSync(path.join(out,'.nojekyll'),'');
fs.writeFileSync(path.join(out,'.gitattributes'),'* -text\n');
const slug=config.version.toLowerCase().replaceAll(' ','-');
if(!/^[a-z0-9.-]+$/.test(slug))throw Error('Invalid preview version');
const packageName=`fm1-rdx-${slug}.fwsc`;
fs.writeFileSync(path.join(out,'firmware',packageName),firmware);
function copyRuntime(from,to){
  fs.mkdirSync(to,{recursive:true});
  for(const e of fs.readdirSync(from,{withFileTypes:true})){
    if(['o2','gen'].includes(e.name))continue;
    const src=path.join(from,e.name),dst=path.join(to,e.name);
    if(e.isDirectory())copyRuntime(src,dst);
    else if(/\.(js|wasm|html|json|css|png|svg|woff2?)$/.test(e.name))fs.copyFileSync(src,dst);
  }
}
copyRuntime('app/build/emu',path.join(out,'emu'));
if(fs.existsSync('app/web/words'))copyRuntime('app/web/words',path.join(out,'words'));
const emuPath=path.join(out,'emu/index.html');
let emu=fs.readFileSync(emuPath,'utf8');
emu=emu.replace('<body>','<body><nav aria-label="Project navigation" style="padding:16px;font:16px system-ui;display:flex;gap:20px;flex-wrap:wrap"><a href="../">FM1-RDX home</a><a href="../install/">Install on FM-1</a><a href="https://github.com/copych/FM1-RDX/issues">Report a problem</a></nav>');
emu=emu.replace(/<p>This tests the current prototype[\s\S]*?Browser performance does not measure FM-1 timing\.<\/p>/,'<p>Explore the sounds and editor without connecting an instrument. Edits are temporary and are lost when you load another preset or restart.</p>');
fs.writeFileSync(emuPath,emu);
let html=fs.readFileSync('platform/fm1-base/web/installer.html','utf8');
const lib=['fm1ports.js','fm1pkg.js','fm1ota.js'].map(name=>fs.readFileSync(`platform/fm1-base/web/${name}`,'utf8').replace(/^export\s+/gm,'').replace(/^import .*?;\r?\n/gm,'')).join('\n');
const meta={version:config.version,product:config.product,pkg:`../firmware/${packageName}`,sha256:config.firmwareSha256,backup:false,res:null,boot:null};
const replacements={NAME:'FM1-RDX',REPO:'https://github.com/copych/FM1-RDX',REPO_SHORT:'github.com/copych/FM1-RDX',LICENSE:'Preview; component licences apply',CREDITS:' RDX synth and ESP GUI by copych. Original RDX and voice licensing is not yet specified; no blanket licence is asserted.',RES:'additional data',RES_JA:'追加データ'};
for(const [key,value] of Object.entries(replacements))html=html.replaceAll(`{{${key}}}`,value);
html=html.replace('/*LIB*/',()=>lib).replace('/*META*/',()=>JSON.stringify(meta)).replace('/*APP*/','null');
const anchor='    const pkg = new Uint8Array(await r.arrayBuffer());';
if(!html.includes(anchor))throw Error('Installer template changed: review checksum guard');
html=html.replace(anchor,`${anchor}\n    if (await sha256hex(pkg) !== meta.sha256) { sayK("badpkg"); return; }`);
html=html.replace('Installing can lose what FM1-RDX has saved.','Installing replaces your current firmware and can erase saved data. Back up using your current firmware’s tools.');
html=html.replace('Do not unplug the USB cable while writing.','Do not unplug the USB cable while writing. After installation, close this tab before opening a DAW or MIDI app so it can access the MIDI port.');
html=html.replace('<h1>Install FM1-RDX</h1>','<nav aria-label="Project navigation" style="display:flex;gap:18px;margin-bottom:24px"><a href="../">Home</a><a href="../emu/">Play in browser</a></nav><h1>Install FM1-RDX</h1><p class="backup"><strong>Experimental preview · 2026.11.21</strong><br>Ready for hardware testing. Software checks pass; physical FM-1 testing is pending. Patch edits are not saved permanently yet. This installer does not replace the bootloader.</p><p><a href="../firmware/'+packageName+'" download>Download this preview firmware</a></p>');
html=html.replace('Experimental preview · 2026.11.21',`Experimental preview · ${config.version.replace('Preview ','')}`).replace('Ready for hardware testing. Software checks pass; physical FM-1 testing is pending.','The first preview boots and plays on an FM-1. This update fixes display redraws, bank numbering and MoDemLead volume; hardware retesting is pending.');
if(/\{\{[A-Z_]+\}\}|\/\*(?:LIB|META|APP)\*\//.test(html))throw Error('Unfilled installer placeholder');
fs.writeFileSync(path.join(out,'install/index.html'),html);
for(const name of ['Apache-2.0.txt','BSD-2-Clause-micro-ecc.txt'])fs.copyFileSync(`platform/fm1-base/LICENSES/${name}`,path.join(out,'firmware',name));
if(fs.existsSync('platform/fm1-base/LICENSE'))fs.copyFileSync('platform/fm1-base/LICENSE',path.join(out,'firmware/MPL-2.0.txt'));
fs.copyFileSync('platform/fm1-base/assets/fonts/Barlow-OFL.txt',path.join(out,'firmware/Barlow-OFL.txt'));
const files={};
function manifest(dir){for(const e of fs.readdirSync(dir,{withFileTypes:true})){if(e.name==='.git'||e.name==='manifest.json')continue;const p=path.join(dir,e.name);if(e.isDirectory())manifest(p);else files[path.relative(out,p).replaceAll('\\','/')]={bytes:fs.statSync(p).size,sha256:hash(fs.readFileSync(p))};}}
manifest(out);
fs.writeFileSync(path.join(out,'manifest.json'),JSON.stringify({version:config.version,product:config.product,files},null,2)+'\n');
console.log(`Prepared ${out}: home, emu, install, pinned firmware. No device commands sent.`);
