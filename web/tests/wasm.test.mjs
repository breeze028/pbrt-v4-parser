import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, mkdir } from 'node:fs/promises';
import { resolve, dirname } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
const root=resolve(dirname(fileURLToPath(import.meta.url)),'../..');
const create=(await import(pathToFileURL(resolve(root,'build-web-wasm/wasm/pbrt4_wasm.mjs')))).default;
const wasm=await create();
const native=process.env.PBRT4_WEB_PACK ?? resolve(root,process.platform==='win32'?'build-web-native/web/cpp/Release/pbrt4_web_pack.exe':'build-web-native/web/cpp/pbrt4_web_pack');
function open(path){const b=new TextEncoder().encode(path+'\0'),p=wasm._malloc(b.length);wasm.HEAPU8.set(b,p);const h=wasm._p4_open(p);wasm._free(p);assert.ok(h);return h;}
function snapshot(h){return {metadata:JSON.parse(wasm.UTF8ToString(wasm._p4_metadata(h))),buffers:Array.from({length:5},(_,k)=>{const p=wasm._p4_data(h,k),n=wasm._p4_bytes(h,k);return Buffer.from(wasm.HEAPU8.slice(p,p+n));})};}
const scenes=[['cornell-box',36,8]];
// libm and native contraction can differ at the last double bits in LookAt.
// Allow only derived matrices a small double tolerance; preserve exact IR values.
function compareMetadata(actual,expected,path='metadata'){
 if(actual&&expected&&typeof actual==='object'&&typeof expected==='object'){
  assert.equal(Array.isArray(actual),Array.isArray(expected),path+' container type');
  assert.deepEqual(Object.keys(actual),Object.keys(expected),path+' keys');
  for(const key of Object.keys(actual)){
   const a=actual[key],b=expected[key],name=path+'.'+key;
   if(key==='transform'||key==='endTransform'){
    assert.equal(a.length,16,name);assert.equal(b.length,16,name);
    for(let i=0;i<16;i++)assert.ok(Number.isFinite(a[i])&&Number.isFinite(b[i])&&Math.abs(a[i]-b[i])<=32*Number.EPSILON*Math.max(1,Math.abs(a[i]),Math.abs(b[i])),`${name}[${i}]: ${a[i]} versus ${b[i]}`);
   }else compareMetadata(a,b,name);
  }
 }else assert.deepEqual(actual,expected,path);
}
test('parity comparison limits tolerance to derived double matrices',()=>{
 const matrix=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];
 const a={transform:matrix,parameters:[{values:[0.1]}],references:[{id:1}]};
 compareMetadata(a,{...a,transform:matrix.map((v,i)=>i===0?v+Number.EPSILON:v)});
 assert.throws(()=>compareMetadata(a,{...a,transform:matrix.map((v,i)=>i===0?v+1e-10:v)}));
 assert.throws(()=>compareMetadata(a,{...a,parameters:[{values:[0.1+Number.EPSILON]}]}));
 assert.throws(()=>compareMetadata(a,{...a,references:[{id:2}]}));
});
if(process.env.PBRT4_WEB_LARGE_SCENES==='1')scenes.push(['spaceship',457560,91],['bathroom',592188,856]);
for(const [name,triangles,placements] of scenes) {
 test('native/WASM identical GPU arrays: '+name,async()=>{
   const dir=resolve(root,'web/public/scenes',name);const manifest=JSON.parse(await readFile(resolve(dir,'manifest.json'),'utf8'));const data=await readFile(resolve(dir,'resources.bin'));
   for(const file of manifest.files){const p='/'+name+'/'+file.path;wasm.FS.mkdirTree(dirname(p).replaceAll('\\','/'));const b=data.subarray(file.offset,file.offset+file.size);assert.equal(createHash('sha256').update(b).digest('hex'),file.sha256);wasm.FS.writeFile(p,b);}
   const h=open('/'+name+'/'+manifest.root);const scene=snapshot(h);wasm._p4_close(h);
   assert.equal(wasm._p4_bytes(h,0),0,'disposed handles cannot access buffers');
   assert.equal(scene.metadata.counts.triangles,triangles);assert.equal(scene.metadata.counts.placements,placements);
   assert.equal(scene.metadata.renderable,true);assert.ok(scene.metadata.maxBvhDepth<61);
   const output=resolve(root,'out/web-native-compare',name);await mkdir(output,{recursive:true});
   const result=spawnSync(native,[resolve(root,'scenes',name,'scene-v4.pbrt'),output],{encoding:'utf8'});assert.equal(result.status,0,result.stderr);
   const normalize=(meta,prefix)=>JSON.parse(JSON.stringify(meta,(_key,value)=>{if(typeof value!=='string')return value;const path=value.replaceAll('\\','/'),base=prefix.replaceAll('\\','/');return path===base||path.startsWith(base+'/')?'<scene>'+path.slice(base.length):path;}));
   compareMetadata(normalize(scene.metadata,'/'+name),normalize(JSON.parse(await readFile(resolve(output,'metadata.json'),'utf8')),resolve(root,'scenes',name)));
   for(let k=0;k<5;k++){
     const reference=await readFile(resolve(output,`buffer${k}.bin`));assert.equal(scene.buffers[k].length,reference.length,`buffer${k} length`);
     if(!scene.buffers[k].equals(reference)){let first=0;while(scene.buffers[k][first]===reference[first])first++;assert.fail(`buffer${k}: first mismatch at byte ${first}, WASM ${scene.buffers[k][first]}, native ${reference[first]}`);}
   }
   if(name==='bathroom'){assert.equal(scene.metadata.images.length,11);assert.equal(scene.metadata.textures.length,12);assert.ok(scene.buffers[4].length>60*1024*1024);}
   assert.equal(scene.buffers[0].length,scene.metadata.counts.vertices*48);
   for(const file of manifest.files)wasm.FS.unlink('/'+name+'/'+file.path);
 });
}
test('WASM diagnostics survive exceptions, Include and Import isolation',()=>{
 wasm.FS.mkdirTree('/fixture/sub');
 wasm.FS.writeFile('/fixture/sub/include.pbrt','Translate 1 0 0 Include "other.pbrt"');
 wasm.FS.writeFile('/fixture/other.pbrt','Shape "trianglemesh" "point3 P" [0 0 0 1 0 0 0 1 0]');
 wasm.FS.writeFile('/fixture/import.pbrt','Translate 3 0 0 Shape "trianglemesh" "point3 P" [0 0 0 1 0 0 0 1 0]');
 wasm.FS.writeFile('/fixture/root.pbrt','Integrator "path" PixelFilter "box" WorldBegin Include "sub/include.pbrt" Import "import.pbrt" Shape "trianglemesh" "point3 P" [0 0 0 1 0 0 0 1 0]');
 let h=open('/fixture/root.pbrt'),s=snapshot(h);wasm._p4_close(h);
 assert.equal(s.metadata.counts.shapes,3);assert.deepEqual(s.metadata.shapes.map(e=>e.transform[3]),[1,4,1]);
 for(const bad of ['WorldBegin Shape "sphere" "normal N" [0 1]','WorldBegin Include "missing.pbrt"','WorldBegin AttributeEnd']) {
   wasm.FS.writeFile('/fixture/bad.pbrt',bad);h=open('/fixture/bad.pbrt');s=snapshot(h);wasm._p4_close(h);
   assert.equal(s.metadata.renderable,false);assert.ok(s.metadata.diagnostics.some(d=>d.severity==='error'));
 }
 h=open('/fixture/root.pbrt');s=snapshot(h);wasm._p4_close(h);assert.equal(s.metadata.renderable,true,'failed loads do not poison later scenes');
});
test('shared BLAS and placement transforms survive WASM ownership',()=>{
 wasm.FS.mkdirTree('/shared');wasm.FS.writeFile('/shared/root.pbrt','WorldBegin ObjectBegin "o" Shape "trianglemesh" "point3 P" [0 0 0 1 0 0 0 1 0] ObjectEnd ObjectInstance "o" Translate 2 0 0 ObjectInstance "o"');
 const h=open('/shared/root.pbrt'),s=snapshot(h);wasm._p4_close(h);assert.equal(s.metadata.counts.triangles,1);assert.equal(s.metadata.counts.placements,2);assert.equal(s.metadata.objects[0].name,'o');
 const words=new Uint32Array(s.buffers[3].buffer,s.buffers[3].byteOffset,s.buffers[3].byteLength/4);const base=words[4];assert.equal(words[base+36],words[base+40+36]);
});
test('malformed images are reported without poisoning the WASM module',()=>{
 wasm.FS.mkdirTree('/bad-image');wasm.FS.writeFile('/bad-image/bad.tga',new Uint8Array([1,2,3]));
 wasm.FS.writeFile('/bad-image/root.pbrt','Integrator "path" PixelFilter "box" WorldBegin Texture "t" "spectrum" "imagemap" "string filename" "bad.tga"');
 const h=open('/bad-image/root.pbrt'),s=snapshot(h);wasm._p4_close(h);assert.equal(s.metadata.renderable,false);assert.ok(s.metadata.diagnostics.some(d=>d.code==='web.pack'&&d.message.includes('TGA')));
 const good=open('/fixture/root.pbrt');assert.equal(snapshot(good).metadata.renderable,true);wasm._p4_close(good);
});
