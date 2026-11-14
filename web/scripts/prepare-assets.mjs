import { readFile, writeFile, mkdir, readdir, copyFile } from 'node:fs/promises';
import { resolve, dirname, relative } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
const root=resolve(dirname(fileURLToPath(import.meta.url)),'../..');
const output=resolve(root,'web/public');
await mkdir(resolve(output,'wasm'),{recursive:true});
await mkdir(resolve(output,'licenses'),{recursive:true});
for(const name of await readdir(resolve(root,'web/licenses')))if(name.endsWith('.txt'))await copyFile(resolve(root,'web/licenses',name),resolve(output,'licenses',name));
await copyFile(resolve(root,'LICENSE'),resolve(output,'licenses/pbrt-v4-parser-Apache-2.0.txt'));
for(const name of ['pbrt4_wasm.mjs','pbrt4_wasm.wasm']) await copyFile(resolve(root,'build-web-wasm/wasm',name),resolve(output,'wasm',name));
const selected=process.argv.slice(2); const names=selected.length?selected:['cornell-box'];
for(const name of names) {
  if(!['cornell-box','bathroom','spaceship'].includes(name)) throw Error('Unknown scene '+name);
  const source=resolve(root,'scenes',name), entries=[];
  async function walk(path) {
    for(const e of (await readdir(path,{withFileTypes:true})).sort((a,b)=>a.name.localeCompare(b.name,'en'))) {
      const p=resolve(path,e.name);
      if(e.isDirectory() && !e.name.startsWith('.')) await walk(p);
      else if(e.isFile() && (/^license(\.txt)?$/i.test(e.name) || /\.(pbrt|ply|tga|png|jpg|jpeg|exr|hdr|pfm|tif|tiff|spd)$/i.test(e.name))) entries.push(p);
    }
  }
  await walk(source); const chunks=[],files=[];let offset=0;
  for(const path of entries) {const b=await readFile(path);chunks.push(b);files.push({path:relative(source,path).replaceAll('\\','/'),offset,size:b.length,sha256:createHash('sha256').update(b).digest('hex')});offset+=b.length;}
  const dest=resolve(output,'scenes',name);await mkdir(dest,{recursive:true});
  await writeFile(resolve(dest,'resources.bin'),Buffer.concat(chunks));
  await writeFile(resolve(dest,'manifest.json'),JSON.stringify({version:1,root:'scene-v4.pbrt',bytes:offset,files},null,2));
  console.log(name,entries.length,'files',offset,'bytes');
}
