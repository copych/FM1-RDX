// Local hardware A/B installers, using the same checked updater as the preview.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {productOf,logicalImage,checkUfw} from '../platform/fm1-base/web/fm1pkg.js';
const root='build/site',base=JSON.parse(fs.readFileSync('site/preview.json'));
const results=JSON.parse(fs.readFileSync('build/compiler-experiment/results.json'));
const template=fs.readFileSync(`${root}/install/index.html`,'utf8');
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const passed=results.filter(r=>r.target);
if(!passed.length)throw Error('No target candidates');
fs.mkdirSync(`${root}/compiler`,{recursive:true});
for(const r of passed){
  if(!r.host||!r.audio||!r.audioMatchesO2||!r.equivalence||!r.browser)throw Error(`Incomplete checks: ${r.name}`);
  if(!/^[a-z0-9-]+$/.test(r.name))throw Error('Invalid candidate name');
  const pkg=fs.readFileSync(`build/compiler-experiment/${r.name}/rdx.fwsc`);
  if(hash(pkg)!==r.firmwareSha256||productOf(pkg)!==base.product)throw Error('Wrong candidate package');
  checkUfw(logicalImage(pkg));
  if(!pkg.includes(Buffer.from('FELUCCA-LOADER-1')))throw Error('Missing updater');
  const filename=`compiler-${r.name}.fwsc`;
  fs.writeFileSync(`${root}/firmware/${filename}`,pkg);
  const version=`Compiler test ${r.name.toUpperCase()}`;
  let html=template.replaceAll(base.version,version).replaceAll(base.firmwareSha256,r.firmwareSha256);
  html=html.replaceAll(/\.\.\/firmware\/fm1-rdx-preview-[a-z0-9.-]+\.fwsc/g,`../../firmware/${filename}`);
  html=html.replaceAll('href="../emu/"','href="../../emu/"');
  const meta=JSON.parse(html.match(/const meta = (\{[^\n]+\});/)[1]);
  if(meta.sha256!==r.firmwareSha256||meta.pkg!==`../../firmware/${filename}`||meta.version!==version)throw Error('Installer mismatch');
  const dir=`${root}/compiler/${r.name}`;fs.mkdirSync(dir,{recursive:true});fs.writeFileSync(`${dir}/index.html`,html);
}
const rows=passed.map(r=>`<tr><td><a href="./${r.name}/">Install ${r.name.toUpperCase()}</a></td><td>${r.imageBytes.toLocaleString('en-US')} bytes</td></tr>`).join('');
fs.writeFileSync(`${root}/compiler/index.html`,`<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>FM1-RDX compiler comparison</title><style>body{max-width:760px;margin:40px auto;padding:20px;font:18px/1.6 system-ui;background:#151820;color:#eee}a{color:#8edaff}td{padding:10px 24px 10px 0}h1{line-height:1.2}</style><h1>Compare compiler options</h1><p>These previews keep the same sound equations, sample rate, effects, clock and voice ceiling. No hardware speed winner has been selected.</p><table>${rows}</table><p>Start with O2. After installing, close the installer tab. Use the same patch, held notes, velocity and effects for every candidate. Press SEL + REC once for Audio Timing, then SAVE to reset peaks. Hold the same four notes for the same duration and photograph SYN, APP MAX, XRUN and the build label at the bottom.</p><p>Keep Render Profile off for this comparison. Its kernels have different execution layouts. Note any audible change. Return to O2 after testing.</p><p>U4 requests selective unrolling of four-operator loops. O3 changes the optimization level of both C++ units; the platform stays at its existing level. Host audio comparisons passed, but device sound/timing remain unverified.</p><p><a href="../install/">Baseline preview installer</a> · <a href="../emu/">Browser emulator</a></p></html>`);
fs.writeFileSync(`${root}/compiler/results.json`,JSON.stringify(results,null,2)+'\n');
const files={};
function visit(dir){for(const e of fs.readdirSync(dir,{withFileTypes:true})){
  if(e.name==='.git'||e.name==='manifest.json')continue;
  const p=path.join(dir,e.name);if(e.isDirectory())visit(p);
  else files[path.relative(root,p).replaceAll('\\','/')]={bytes:fs.statSync(p).size,sha256:hash(fs.readFileSync(p))};
}}
visit(root);fs.writeFileSync(`${root}/manifest.json`,JSON.stringify({version:base.version,product:base.product,files},null,2)+'\n');
console.log(`Prepared ${passed.length} checked local compiler-test installers.`);
