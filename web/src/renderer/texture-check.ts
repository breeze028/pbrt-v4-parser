import type { Metadata } from '../model';
const linear=(v:number)=>v<=.04045?v/12.92:((v+.055)/1.055)**2.4;
const mod=(a:number,n:number)=>(a%n+n)%n;
export async function checkTextures(device:GPUDevice,source:string,layout:GPUBindGroupLayout,group:GPUBindGroup,metadata:Metadata,pixels:ArrayBuffer,current=()=>true){
  const cases:{slot:number;uv:number[];lod:number;expected:number[];du:number[];dv:number[];dh:number[];normal:number[]}[]=[];
  const bytes=new Uint8Array(pixels);
  function sample(slot:number,uv:number[],lod:number){
    const image=metadata.images[slot];if(!image)return [1,1,1];
    const level=(m:number)=>{
      const l=image.levels[m],x=uv[0]*l.width-.5,y=(1-uv[1])*l.height-.5,ix=Math.floor(x),iy=Math.floor(y),u=x-ix,v=y-iy;
      const value=[0,0,0];
      for(let dy=0;dy<2;dy++)for(let dx=0;dx<2;dx++)for(let c=0;c<3;c++){
        let a=bytes[l.offset+(mod(iy+dy,l.height)*l.width+mod(ix+dx,l.width))*4+c]/255;if(image.srgb)a=linear(a);
        value[c]+=a*(dx?u:1-u)*(dy?v:1-v);
      }return value;
    };
    const a=Math.min(image.levels.length-1,Math.max(0,lod)),low=Math.floor(a),high=Math.ceil(a),lo=level(low),hi=level(high);
    return lo.map((v,i)=>v*(1-a+low)+hi[i]*(a-low));
  }
  for(let slot=0;slot<metadata.images.length;slot++)for(const uv of [[0,0],[.2,.7],[1.2,-.3],[.9999,.0001],[.5,.5]])for(const lod of [0,.5,2.25,metadata.images[slot].levels.length-1]){
    cases.push({slot,uv,lod,expected:sample(slot,uv,lod),du:[1,0,0],dv:[0,1,0],dh:[0,0],normal:[0,0,1]});
  }
  const texels=cases.length;
  const bumps=[{du:[1,0,0],dv:[0,1,0],dh:[1,2],n:[-1,-2,1]},{du:[-1,0,0],dv:[0,1,0],dh:[1,2],n:[1,-2,1]},{du:[2,0,0],dv:[.5,3,0],dh:[.4,-.2],n:[-1.2,.6,6]}];
  for(const b of bumps){const length=Math.hypot(...b.n);cases.push({slot:0,uv:[.2,.7],lod:0,expected:sample(0,[.2,.7],0),du:b.du,dv:b.dv,dh:b.dh,normal:b.n.map(v=>v/length)});}
  const stride=32,input=new Float32Array(cases.length*stride);
  cases.forEach((c,i)=>{const b=i*stride;input.set([...c.uv,c.lod,c.slot],b);input.set([...c.du,c.dh[0]],b+4);input.set([...c.dv,c.dh[1]],b+8);input.set([0,0,1,0],b+12);});
  const sampling=source.slice(0,source.indexOf('struct UVContext'));
  const bump=source.slice(source.indexOf('fn bump_normal('),source.indexOf('fn mapped_normal('));
  const code=sampling+'\n'+bump+`
@group(0) @binding(0) var<storage,read_write> query:array<vec4f>;
@compute @workgroup_size(64) fn main(@builtin(global_invocation_id) id:vec3u){
 let b=id.x*8u;if(b>=arrayLength(&query)){return;}let q=query[b];
 query[b+6u]=vec4f(image_sample(u32(q.w),vec2f(q.x,1-q.y),q.z),1);
 query[b+7u]=vec4f(bump_normal(query[b+1u].xyz,query[b+2u].xyz,query[b+3u].xyz,vec2f(query[b+1u].w,query[b+2u].w),query[b+3u].w,query[b+4u].xyz,query[b+5u].xyz),0);
}`;
  const module=device.createShaderModule({code,label:'Texture role / encoding / wrap / mip / bump checks'});
  const errors=(await module.getCompilationInfo()).messages.filter(m=>m.type==='error');if(errors.length)throw Error(errors.map(e=>e.message).join('\n'));
  const dataLayout=device.createBindGroupLayout({entries:[{binding:0,visibility:GPUShaderStage.COMPUTE,buffer:{type:'storage'}}]});
  const pipeline=await device.createComputePipelineAsync({layout:device.createPipelineLayout({bindGroupLayouts:[dataLayout,layout]}),compute:{module,entryPoint:'main'}});
  if(!current())throw Error('The scene changed during the self-test.');
  const storage=device.createBuffer({size:input.byteLength,usage:GPUBufferUsage.STORAGE|GPUBufferUsage.COPY_SRC|GPUBufferUsage.COPY_DST});
  const readback=device.createBuffer({size:input.byteLength,usage:GPUBufferUsage.MAP_READ|GPUBufferUsage.COPY_DST});
  try{
    device.queue.writeBuffer(storage,0,input);
    const dataGroup=device.createBindGroup({layout:dataLayout,entries:[{binding:0,resource:{buffer:storage}}]});
    const encoder=device.createCommandEncoder(),pass=encoder.beginComputePass();pass.setPipeline(pipeline);pass.setBindGroup(0,dataGroup);pass.setBindGroup(1,group);pass.dispatchWorkgroups(Math.ceil(cases.length/64));pass.end();encoder.copyBufferToBuffer(storage,0,readback,0,input.byteLength);device.queue.submit([encoder.finish()]);await readback.mapAsync(GPUMapMode.READ);
    const data=new Float32Array(readback.getMappedRange());let maxError=0;
    cases.forEach((c,i)=>{for(let channel=0;channel<3;channel++){
      const value=data[i*stride+24+channel],error=Math.abs(value-c.expected[channel]);maxError=Math.max(maxError,error);
      if(!Number.isFinite(value)||error>.006)throw Error(`Texture CPU/GPU mismatch at ${i}, ${channel}: ${value} / ${c.expected[channel]}`);
      const n=data[i*stride+28+channel];if(!Number.isFinite(n)||Math.abs(n-c.normal[channel])>1e-5)throw Error('GPU bump normal mismatch '+i);
    }});
    return {texels,bumpNormals:bumps.length,maxError};
  }finally{storage.destroy();readback.destroy();}
}
