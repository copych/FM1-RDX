// Explicit release helper; credentials stay in memory and are never logged.
import fs from 'node:fs';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
const repo='copych/FM1-RDX',version=JSON.parse(fs.readFileSync('site/preview.json')).version,tag='v'+version;
if(!/^\d+\.\d+\.\d+$/.test(version))throw Error('Expected a stable release version');
const git=(...args)=>execFileSync('git',['-c',`safe.directory=${process.cwd().replaceAll('\\','/')}`,...args],{encoding:'utf8'}).trim();
const raw=execFileSync('git',['-c',`safe.directory=${process.cwd().replaceAll('\\','/')}`,'credential','fill'],{
 input:'protocol=https\nhost=github.com\n\n',encoding:'utf8',env:{...process.env,GIT_TERMINAL_PROMPT:'0'}});
const credential=Object.fromEntries(raw.trim().split(/\r?\n/).map(line=>{const at=line.indexOf('=');return [line.slice(0,at),line.slice(at+1)];}));
if(!credential.password)throw Error('GitHub credential unavailable');
async function api(path,method='GET',body,type='application/json'){
 const url=path.startsWith('https:')?new URL(path):new URL(`https://api.github.com${path}`);
 if(!['api.github.com','uploads.github.com'].includes(url.hostname))throw Error('Unexpected GitHub endpoint');
 const response=await fetch(url,{method,headers:{Authorization:`Bearer ${credential.password}`,Accept:'application/vnd.github+json',
  'X-GitHub-Api-Version':'2022-11-28','User-Agent':'FM1-RDX-release',...(body?{'Content-Type':type}:{})},
  ...(body?{body:Buffer.isBuffer(body)?body:type==='application/json'?JSON.stringify(body):body}:{})});
 if(response.status===404)return null;
 if(!response.ok)throw Error(`GitHub ${method} ${url.pathname}: HTTP ${response.status}: ${(await response.text()).slice(0,500)}`);
 return response.status===204?{}:response.json();
}
const mode=process.argv[2]||'inspect';
if(mode==='inspect'||mode==='status'){
 const user=await api('/user');const pages=await api(`/repos/${repo}/pages`);
 const build=await api(`/repos/${repo}/pages/builds/latest`);
 const release=await api(`/repos/${repo}/releases/tags/${tag}`);
 console.log(JSON.stringify({login:user.login,pages:pages&&{url:pages.html_url,status:pages.status,source:pages.source},
  build:build&&{status:build.status,error:build.error,commit:build.commit},release:release&&{url:release.html_url,id:release.id,tag:release.tag_name,assets:release.assets.map(a=>a.name)}},null,2));
}else if(mode==='release'){
 const head=git('rev-parse','HEAD');
 let release=await api(`/repos/${repo}/releases/tags/${tag}`);
 if(!release)release=await api(`/repos/${repo}/releases`,'POST',{tag_name:tag,target_commitish:head,name:'FM1-RDX '+version,
  body:fs.readFileSync('build/release-notes.md','utf8'),draft:true,prerelease:false});
 const assets=[['app/build/rdx.fwsc',`FM1-RDX-v${version}.fwsc`,'application/octet-stream'],
  [`build/FM1-RDX-v${version}-web.zip`,`FM1-RDX-v${version}-web.zip`,'application/zip'],
  ['build/SHA256SUMS.txt','SHA256SUMS.txt','text/plain'],['build/evidence.json','build-evidence.json','application/json']];
 for(const [path,name,type] of assets){
  const bytes=fs.readFileSync(path),existing=release.assets.find(a=>a.name===name);
  if(existing){
   const downloaded=Buffer.from(await (await fetch(existing.browser_download_url)).arrayBuffer());
   if(createHash('sha256').update(downloaded).digest('hex')!==createHash('sha256').update(bytes).digest('hex'))throw Error(`Existing asset differs: ${name}`);
  }else{
   const url=release.upload_url.replace(/\{.*$/,'')+`?name=${encodeURIComponent(name)}`;
   const asset=await api(url,'POST',bytes,type);
   const expected='sha256:'+createHash('sha256').update(bytes).digest('hex');
   if(asset.digest&&asset.digest!==expected)throw Error(`Uploaded asset digest differs: ${name}`);
  }
  console.log(`Asset verified/uploaded: ${name}`);
 }
 if(release.draft)release=await api(`/repos/${repo}/releases/${release.id}`,'PATCH',{draft:false,make_latest:'true'});
 console.log(`Published ${release.html_url}`);
}else if(mode==='build-pages'){
 const build=await api(`/repos/${repo}/pages/builds`,'POST');console.log(JSON.stringify(build));
}else throw Error('Expected inspect, status, release or build-pages');
