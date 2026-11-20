import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../app/build/emu');
const port=Number(process.argv[2]||8788);
if(!Number.isInteger(port)||port<1024||port>65535)throw Error('Port must be 1024..65535');
if(!fs.existsSync(path.join(root,'app.wasm')))throw Error('Build the emulator first: wsl -d Ubuntu -- bash tools/build-emu.sh');
const types={'.html':'text/html; charset=utf-8','.js':'text/javascript; charset=utf-8','.wasm':'application/wasm','.json':'application/json','.css':'text/css','.png':'image/png','.txt':'text/plain; charset=utf-8'};
const server=http.createServer((req,res)=>{
  if(req.method!=='GET'&&req.method!=='HEAD'){res.writeHead(405);res.end();return;}
  let name;
  try{name=decodeURIComponent(new URL(req.url,'http://localhost').pathname);}catch{res.writeHead(400);res.end();return;}
  if(name.includes('\0')||name.includes('\\')){res.writeHead(400);res.end();return;}
  if(name==='/__rdx_emu_health'){
    res.writeHead(200,{'Content-Type':'application/json'});res.end(JSON.stringify({app:'FM1-RDX emulator',root}));return;
  }
  const file=path.resolve(root,'.'+(name==='/'?'/index.html':name));
  if(file!==root&&!file.startsWith(root+path.sep)){res.writeHead(403);res.end();return;}
  fs.stat(file,(error,stat)=>{
    if(error||!stat.isFile()){res.writeHead(404);res.end('Not found');return;}
    res.writeHead(200,{'Content-Type':types[path.extname(file)]||'application/octet-stream','Content-Length':stat.size,'Cache-Control':'no-store','X-Content-Type-Options':'nosniff'});
    if(req.method==='HEAD'){res.end();return;}
    const stream=fs.createReadStream(file);stream.on('error',()=>res.destroy());stream.pipe(res);
  });
});
server.listen(port,'127.0.0.1',()=>console.log(`FM1-RDX emulator: http://127.0.0.1:${port}/`));
server.on('error',e=>{console.error(e.message);process.exit(1);});
