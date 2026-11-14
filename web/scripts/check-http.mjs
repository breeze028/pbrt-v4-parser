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
const worker=script.match(/\/(?:[\w.~-]+\/)*assets\/load-[\w-]+\.js/);assert.ok(worker,'module worker URL');await get(worker[0],/javascript/);
const glue=(await get('wasm/pbrt4_wasm.mjs',/javascript/)).toString();assert.ok(glue.includes('WebAssembly'));
const wasm=await get('wasm/pbrt4_wasm.wasm',/application\/wasm/);assert.deepEqual([...wasm.subarray(0,4)],[0,97,115,109]);await WebAssembly.compile(wasm);
const manifest=JSON.parse((await get('scenes/cornell-box/manifest.json',/json/)).toString());
const packed=await get('scenes/cornell-box/resources.bin',/octet-stream/);assert.equal(packed.length,manifest.bytes);
for(const file of manifest.files)assert.equal(createHash('sha256').update(packed.subarray(file.offset,file.offset+file.size)).digest('hex'),file.sha256,file.path);
assert.ok(manifest.files.some(f=>f.path===manifest.root));
console.log('PASS production HTML / JS / CSS / worker / embedded WGSL / WASM compile / Cornell bundle SHA256:',base.href);
