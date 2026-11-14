import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';

const base=new URL(process.argv[2]??'http://127.0.0.1:5181/renamed-project/');
async function get(path,mime){
  const url=new URL(path,base),response=await fetch(url);assert.equal(response.status,200,url.href);
  assert.match(response.headers.get('content-type')??'',mime,url.href);
  const bytes=Buffer.from(await response.arrayBuffer());assert.ok(bytes.length,url.href);
  return bytes;
}
const html=(await get('',/text\/html/)).toString();
const js=html.match(/src="([^"]+\.js)"/),css=html.match(/href="([^"]+\.css)"/);
assert.ok(js&&css,'built HTML assets');
const script=(await get(js[1],/javascript/)).toString();await get(css[1],/text\/css/);
assert.ok(script.includes('747796405'),'WGSL included in production JS');
const workers=[...new Set(script.match(/\/(?:[\w.~-]+\/)*assets\/(?:load|probe-worker)-[\w-]+\.js/g)??[])];assert.ok(workers.some(w=>w.includes('/load-'))&&workers.some(w=>w.includes('/probe-worker-')),'both module worker URLs');for(const worker of workers)await get(worker,/javascript/);
const glue=(await get('wasm/pbrt4_wasm.mjs',/javascript/)).toString();assert.ok(glue.includes('WebAssembly'));
const wasm=await get('wasm/pbrt4_wasm.wasm',/application\/wasm/);assert.deepEqual([...wasm.subarray(0,4)],[0,97,115,109]);await WebAssembly.compile(wasm);
for(const license of ['NOTICE.txt','pbrt-v4-parser-Apache-2.0.txt','pbrt-v4-Apache-2.0.txt','emscripten-7.0.0.txt','libcxx.txt','musl.txt']) await get(`licenses/${license}`,/text\/plain/);
const requested=process.argv.slice(3),scenes=requested.length?requested:['cornell-box'];
for(const scene of scenes){
 assert.ok(['cornell-box','spaceship','bathroom'].includes(scene));
 const manifest=JSON.parse((await get(`scenes/${scene}/manifest.json`,/json/)).toString());
 const packed=await get(`scenes/${scene}/resources.bin`,/octet-stream/);assert.equal(packed.length,manifest.bytes);
 for(const file of manifest.files){assert.doesNotMatch(file.path,/\.(lt|ivol)$/i);assert.equal(createHash('sha256').update(packed.subarray(file.offset,file.offset+file.size)).digest('hex'),file.sha256,file.path);}
 assert.ok(manifest.files.some(f=>f.path===manifest.root));assert.ok(manifest.files.some(f=>f.path==='LICENSE.txt'));
 console.log('PASS original scene HTTP resources / per-file SHA256 / LICENSE:',scene,packed.length,'bytes');
}
console.log('PASS production HTML / JS / CSS / both workers / embedded WGSL / WASM compile:',base.href);
