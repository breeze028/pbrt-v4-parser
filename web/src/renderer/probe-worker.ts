import { probes } from './probe';
const scope=self as unknown as DedicatedWorkerGlobalScope;
scope.onmessage=({data}:{data:{buffers:ArrayBuffer[];camera:number[]}})=>{
  try{const result=probes(data.buffers,data.camera);scope.postMessage({result},[result.input.buffer]);}
  catch(e){scope.postMessage({error:String(e)});}
};
