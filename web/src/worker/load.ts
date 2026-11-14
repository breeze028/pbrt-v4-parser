import type { Loaded,Metadata } from '../model';
interface Module {
  FS:{mkdirTree(p:string):void;writeFile(p:string,b:Uint8Array):void;unlink(p:string):void;rmdir(p:string):void};
  HEAPU8:Uint8Array;UTF8ToString(p:number):string;
  _malloc(n:number):number;_free(p:number):void;_p4_open(p:number):number;_p4_close(h:number):void;_p4_metadata(h:number):number;_p4_data(h:number,k:number):number;_p4_bytes(h:number,k:number):number;
}
let modulePromise:Promise<Module>|undefined;
const workerScope=self as unknown as DedicatedWorkerGlobalScope;
workerScope.onmessage=async ({data}:{data:{generation:number;base:string;scene:string}})=>{
  const start=performance.now();const files:string[]=[],dirs=new Set<string>();let handle=0,pointer=0,wasm:Module|undefined;
  const send=(message:unknown)=>workerScope.postMessage(message);
  try {
    if(!modulePromise) modulePromise=(async()=>{
      const url=new URL('wasm/pbrt4_wasm.mjs',data.base).href;
      const factory=(await import(/* @vite-ignore */ url)).default;
      return factory({locateFile:(p:string)=>new URL('wasm/'+p,data.base).href});
    })();
    const sceneURL=new URL('scenes/'+data.scene+'/',data.base);
    const manifestResponse=await fetch(new URL('manifest.json',sceneURL));if(!manifestResponse.ok)throw Error('场景包未准备：'+data.scene);
    const manifest=await manifestResponse.json() as {version:number;root:string;bytes:number;files:{path:string;offset:number;size:number;sha256:string}[]};
    if(manifest.version!==1)throw Error('Unsupported resource bundle version');
    send({type:'progress',generation:data.generation,message:'正在下载场景资源…'});
    const response=await fetch(new URL('resources.bin',sceneURL));if(!response.ok)throw Error('Resource HTTP '+response.status);
    const packed=await response.arrayBuffer();if(packed.byteLength!==manifest.bytes)throw Error('Resource bundle length mismatch');
    wasm=await modulePromise;const root='/scene-'+data.generation;wasm.FS.mkdirTree(root);dirs.add(root);
    const seen=new Set<string>();
    for(const file of manifest.files) {
      if(!file.path || file.path.startsWith('/') || file.path.includes('\\') || file.path.split('/').some(p=>p==='..' || p==='.' || !p) || seen.has(file.path))throw Error('Unsafe/duplicate virtual path');
      if(!Number.isSafeInteger(file.offset)||!Number.isSafeInteger(file.size)||file.offset<0||file.size<0||file.offset+file.size>packed.byteLength)throw Error('Invalid resource range');
      seen.add(file.path);const bytes=new Uint8Array(packed,file.offset,file.size);
      const hash=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes))).map(v=>v.toString(16).padStart(2,'0')).join('');if(hash!==file.sha256)throw Error('Resource checksum: '+file.path);
      const path=root+'/'+file.path;const parts=path.split('/');parts.pop();const parent=parts.join('/');wasm.FS.mkdirTree(parent);
      for(let j=2;j<parts.length+1;j++)dirs.add(parts.slice(0,j).join('/'));
      wasm.FS.writeFile(path,bytes);files.push(path);
    }
    if(!seen.has(manifest.root))throw Error('Root scene is absent from manifest');
    send({type:'progress',generation:data.generation,message:'C++ WASM 正在解析并构建 BLAS/TLAS…'});
    const path=new TextEncoder().encode(root+'/'+manifest.root+'\0');pointer=wasm._malloc(path.length);wasm.HEAPU8.set(path,pointer);
    handle=wasm._p4_open(pointer);if(!handle)throw Error('WASM scene allocation failed');
    const metadata=JSON.parse(wasm.UTF8ToString(wasm._p4_metadata(handle))) as Metadata;
    // Reacquire HEAPU8 for every buffer: parsing/building may grow WASM memory.
    const buffers=Array.from({length:4},(_,k)=>{const p=wasm!._p4_data(handle,k),n=wasm!._p4_bytes(handle,k);return wasm!.HEAPU8.slice(p,p+n).buffer;});
    const result:Loaded={type:'loaded',generation:data.generation,metadata,buffers,milliseconds:performance.now()-start};workerScope.postMessage(result,buffers);
  }catch(e){send({type:'error',generation:data.generation,message:String(e)});}
  finally {
    if(wasm){if(handle)wasm._p4_close(handle);if(pointer)wasm._free(pointer);for(const f of files)wasm.FS.unlink(f);for(const dir of [...dirs].sort((a,b)=>b.length-a.length))wasm.FS.rmdir(dir);}
  }
};
