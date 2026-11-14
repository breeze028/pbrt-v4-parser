import shader from '../shaders/pathtrace.wgsl?raw';
import presentation from '../shaders/present.wgsl?raw';
import bsdf from '../shaders/bsdf.wgsl?raw';
import integrator from '../shaders/integrator.wgsl?raw';
import textures from '../shaders/textures.wgsl?raw';
import type { Metadata } from '../model';
import type { probes } from './probe';
import { checkBsdf } from './bsdf-check';
import { checkTextures } from './texture-check';
export class Renderer {
  device!:GPUDevice;context!:GPUCanvasContext;compute!:GPUComputePipeline;present!:GPURenderPipeline;
  uniform!:GPUBuffer;accumulation!:GPUBuffer;diagnostics!:GPUBuffer;computeGroup?:GPUBindGroup;presentGroup?:GPUBindGroup;
  owned:GPUBuffer[]=[];metadata?:Metadata;camera:number[]=[];sample=0;width=384;height=384;previewSize=384;exposure=1;paused=false;failed=false;clamp=true;lod0=false;alpha=true;bump=true;maxSpp=0;seed=1;depth=8;
  private raf=0;private serial=0;private checksPending=false;
  private sceneGPU:GPUBuffer[]=[];private sceneBytes:ArrayBuffer[]=[];private probePipeline!:GPUComputePipeline;
  private textureLayout!:GPUBindGroupLayout;private textureGroup?:GPUBindGroup;private imageSampler!:GPUSampler;private white!:GPUTexture;private imageTextures:GPUTexture[]=[];
  gpuName='';
  constructor(private canvas:HTMLCanvasElement,private status:(s:string,error?:boolean)=>void,private stats:(s:number)=>void){}
  async init(){
    if(!isSecureContext || !navigator.gpu)throw Error('此浏览器未提供 WebGPU；需要 HTTPS / localhost 与硬件加速。');
    const adapter=await navigator.gpu.requestAdapter({powerPreference:'high-performance'});if(!adapter)throw Error('无法获得 WebGPU Adapter。');
    this.gpuName=adapter.info.description||[adapter.info.vendor,adapter.info.architecture].filter(Boolean).join(' ');
    this.device=await adapter.requestDevice();this.context=this.canvas.getContext('webgpu')!;
    if(!this.context)throw Error('无法创建 WebGPU Canvas');
    this.context.configure({device:this.device,format:navigator.gpu.getPreferredCanvasFormat(),alphaMode:'opaque'});
    this.device.addEventListener('uncapturederror',e=>{this.failed=true;this.status(e.error.message,true);console.error(e.error);});
    void this.device.lost.then(info=>{this.failed=true;this.status('GPU device lost: '+info.message,true);});
    const module=this.device.createShaderModule({code:shader+'\n'+bsdf+'\n'+textures+'\n'+integrator,label:'LightTransport RGB megakernel'});
    const info=await module.getCompilationInfo();const errors=info.messages.filter(m=>m.type==='error');if(errors.length)throw Error(errors.map(m=>`${m.lineNum}:${m.linePos} ${m.message}`).join('\n'));
    // An auto layout is exclusive to its pipeline; query and render share an explicit layout.
    const group=this.device.createBindGroupLayout({entries:Array.from({length:7},(_,binding)=>({binding,visibility:GPUShaderStage.COMPUTE,buffer:{type:binding===0?'uniform':binding<5?'read-only-storage':'storage'} as GPUBufferBindingLayout}))});
    this.textureLayout=this.device.createBindGroupLayout({entries:[{binding:0,visibility:GPUShaderStage.COMPUTE,sampler:{type:'filtering'}},...Array.from({length:12},(_,i)=>({binding:i+1,visibility:GPUShaderStage.COMPUTE,texture:{sampleType:'float' as GPUTextureSampleType,viewDimension:'2d' as GPUTextureViewDimension}}))]});
    const layout=this.device.createPipelineLayout({bindGroupLayouts:[group,this.textureLayout]});
    this.compute=await this.device.createComputePipelineAsync({layout,compute:{module,entryPoint:'main'}});
    this.probePipeline=await this.device.createComputePipelineAsync({layout,compute:{module,entryPoint:'probe'}});
    const screen=this.device.createShaderModule({code:presentation});
    this.present=await this.device.createRenderPipelineAsync({layout:'auto',vertex:{module:screen,entryPoint:'vertex'},fragment:{module:screen,entryPoint:'fragment',targets:[{format:navigator.gpu.getPreferredCanvasFormat()}]},primitive:{topology:'triangle-list'}});
    this.uniform=this.device.createBuffer({size:128,usage:GPUBufferUsage.UNIFORM|GPUBufferUsage.COPY_DST});
    this.imageSampler=this.device.createSampler({magFilter:'linear',minFilter:'linear',mipmapFilter:'linear',addressModeU:'repeat',addressModeV:'repeat'});
    this.white=this.device.createTexture({size:[1,1],format:'rgba8unorm',usage:GPUTextureUsage.TEXTURE_BINDING|GPUTextureUsage.COPY_DST});
    this.device.queue.writeTexture({texture:this.white},new Uint8Array([255,255,255,255]),{bytesPerRow:4},[1,1]);
    this.raf=requestAnimationFrame(()=>this.frame());
  }
  private buffer(data:ArrayBuffer,label:string){
    if(data.byteLength>this.device.limits.maxStorageBufferBindingSize)throw Error(label+' exceeds device storage binding limit');
    const b=this.device.createBuffer({label,size:Math.max(16,data.byteLength),usage:GPUBufferUsage.STORAGE|GPUBufferUsage.COPY_DST});if(data.byteLength)this.device.queue.writeBuffer(b,0,data);this.owned.push(b);return b;
  }
  async load(metadata:Metadata,buffers:ArrayBuffer[]){
    this.serial++;this.disposeScene();this.metadata=metadata;this.camera=[...metadata.camera.transform];this.depth=metadata.maxDepth;this.failed=false;
    if(metadata.layoutVersion!==2)throw Error('GPU layout version mismatch');
    if(!metadata.renderable){this.status('场景已解析；此版本尚未实现它需要的渲染特性。',true);return;}
    const scene=buffers.slice(0,4).map((b,i)=>this.buffer(b,'Scene buffer '+i));this.sceneGPU=scene;this.sceneBytes=buffers;this.diagnostics=this.device.createBuffer({size:16,usage:GPUBufferUsage.STORAGE|GPUBufferUsage.COPY_SRC|GPUBufferUsage.COPY_DST});this.owned.push(this.diagnostics);
    this.uploadImages(metadata,buffers[4]);
    this.resize(this.previewSize);
    this.status('WebGPU · GGX / glass / NEE / MIS');this.reset();
  }
  private uploadImages(metadata:Metadata,pixels:ArrayBuffer){
    if(metadata.images.length>12)throw Error('Scene exceeds 12 portable image slots');
    for(const image of metadata.images){
      const first=image.levels[0];if(!first || first.width<1 || first.height<1 || Math.max(first.width,first.height)>this.device.limits.maxTextureDimension2D)throw Error('Unsupported image dimensions: '+image.path);
      const texture=this.device.createTexture({label:image.path+' '+image.role,size:[first.width,first.height],mipLevelCount:image.levels.length,format:image.srgb?'rgba8unorm-srgb':'rgba8unorm',usage:GPUTextureUsage.TEXTURE_BINDING|GPUTextureUsage.COPY_DST});this.imageTextures.push(texture);
      image.levels.forEach((level,mipLevel)=>{
        if(level.width!==Math.max(1,first.width>>mipLevel) || level.height!==Math.max(1,first.height>>mipLevel) || level.bytes!==level.width*level.height*4 || level.offset+level.bytes>pixels.byteLength)throw Error('Invalid mip range: '+image.path);
        this.device.queue.writeTexture({texture,mipLevel},new Uint8Array(pixels,level.offset,level.bytes),{bytesPerRow:level.width*4,rowsPerImage:level.height},[level.width,level.height]);
      });
    }
    const views=Array.from({length:12},(_,i)=>(this.imageTextures[i]??this.white).createView());
    this.textureGroup=this.device.createBindGroup({layout:this.textureLayout,entries:[{binding:0,resource:this.imageSampler},...views.map((resource,i)=>({binding:i+1,resource}))]});
  }
  private allocatePixels(){
    const aspect=this.metadata!.width/this.metadata!.height;this.width=this.previewSize;this.height=this.previewSize;
    if(aspect>1)this.height=Math.max(1,Math.round(this.width/aspect));else this.width=Math.max(1,Math.round(this.height*aspect));
    this.canvas.width=this.width;this.canvas.height=this.height;
    this.accumulation=this.device.createBuffer({size:this.width*this.height*16,usage:GPUBufferUsage.STORAGE|GPUBufferUsage.COPY_SRC|GPUBufferUsage.COPY_DST});this.owned.push(this.accumulation);
    this.presentGroup=this.device.createBindGroup({layout:this.present.getBindGroupLayout(0),entries:[{binding:0,resource:{buffer:this.uniform}},{binding:1,resource:{buffer:this.accumulation}}]});
  }
  resize(size:number){
    if(![128,256,384,512].includes(size))throw Error('Invalid preview size');
    this.previewSize=size;if(!this.metadata?.renderable)return;
    this.serial++;
    if(this.owned.includes(this.accumulation)){this.accumulation.destroy();this.owned=this.owned.filter(b=>b!==this.accumulation);}
    this.allocatePixels();const scene=this.sceneGPU;
    this.computeGroup=this.device.createBindGroup({layout:this.compute.getBindGroupLayout(0),entries:[this.uniform,scene[2],scene[0],scene[1],scene[3],this.accumulation,this.diagnostics].map((buffer,binding)=>({binding,resource:{buffer}}))});
    this.reset();
  }
  reset(){this.sample=0;this.stats(0);if(this.owned.includes(this.accumulation)){const encoder=this.device.createCommandEncoder();encoder.clearBuffer(this.accumulation);this.device.queue.submit([encoder.finish()]);}}
  async readHdr(){
    if(!this.metadata?.renderable || this.failed || !this.sample)throw Error('先完成至少一个有效样本');
    const serial=this.serial,width=this.width,height=this.height;
    const buffer=this.device.createBuffer({size:width*height*16,usage:GPUBufferUsage.MAP_READ|GPUBufferUsage.COPY_DST});
    try{
      const encoder=this.device.createCommandEncoder();encoder.copyBufferToBuffer(this.accumulation,0,buffer,0,width*height*16);this.device.queue.submit([encoder.finish()]);await buffer.mapAsync(GPUMapMode.READ);
      if(serial!==this.serial)throw Error('导出期间场景发生变化');
      const pixels=new Float32Array(buffer.getMappedRange()),rgb=new Float32Array(width*height*3);let spp=0;
      for(let y=0;y<height;y++)for(let x=0;x<width;x++){const i=(y*width+x)*4,n=pixels[i+3];spp=Math.max(spp,n);if(!n)throw Error('不完整的累积帧');for(let c=0;c<3;c++){const v=pixels[i+c]/n;if(!Number.isFinite(v))throw Error('HDR 含非有限值');rgb[((height-1-y)*width+x)*3+c]=v;}}
      return {width,height,spp,rgb};
    }finally{buffer.destroy();}
  }
  async validate(){
    if(!this.metadata?.renderable)throw Error('先加载可渲染场景');
    const serial=this.serial;
    const cpuWorker=new Worker(new URL('./probe-worker.ts',import.meta.url),{type:'module'});
    const result=await new Promise<ReturnType<typeof probes>>((resolve,reject)=>{
      cpuWorker.onmessage=({data})=>{cpuWorker.terminate();if(data.error)reject(Error(data.error));else resolve(data.result);};
      cpuWorker.onerror=e=>{cpuWorker.terminate();reject(Error(e.message));};
      cpuWorker.postMessage({buffers:this.sceneBytes.slice(0,4),camera:this.camera});
    });
    if(serial!==this.serial)throw Error('自检期间场景发生变化，请重新运行');
    const {input,expected}=result,size=input.byteLength,scene=this.sceneGPU;
    const buffer=this.device.createBuffer({size,usage:GPUBufferUsage.STORAGE|GPUBufferUsage.COPY_DST|GPUBufferUsage.COPY_SRC});
    const readback=this.device.createBuffer({size:size+16,usage:GPUBufferUsage.MAP_READ|GPUBufferUsage.COPY_DST});
    const diagnostic=this.device.createBuffer({size:16,usage:GPUBufferUsage.STORAGE});
    try{
      this.device.queue.writeBuffer(buffer,0,input);
      const group=this.device.createBindGroup({layout:this.compute.getBindGroupLayout(0),entries:[this.uniform,scene[2],scene[0],scene[1],scene[3],buffer,diagnostic].map((b,binding)=>({binding,resource:{buffer:b}}))});
      const encoder=this.device.createCommandEncoder(),pass=encoder.beginComputePass();pass.setPipeline(this.probePipeline);pass.setBindGroup(0,group);pass.setBindGroup(1,this.textureGroup!);pass.dispatchWorkgroups(Math.ceil(expected.length/64));pass.end();encoder.copyBufferToBuffer(buffer,0,readback,0,size);encoder.copyBufferToBuffer(this.diagnostics,0,readback,size,16);this.device.queue.submit([encoder.finish()]);await readback.mapAsync(GPUMapMode.READ);
      const mapped=readback.getMappedRange(),f=new Float32Array(mapped),u=new Uint32Array(mapped);let checked=0;
      if(serial!==this.serial)throw Error('自检期间场景发生变化，请重新运行');
      const diagnostics=[...new Uint32Array(mapped,size,4)];if(diagnostics.some(v=>v))throw Error('GPU 累积诊断失败：'+JSON.stringify(diagnostics));
      expected.forEach((e,i)=>{const p=i*16;if(u[p+6] || Boolean(u[p+5])!==e.valid || (e.valid&&(Math.abs(f[p]-e.distance)>1e-3 || Math.abs(f[p+1]-e.u)>1e-3 || Math.abs(f[p+2]-e.v)>1e-3 || u[p+3]!==e.primitive || u[p+4]!==e.instance || e.geometric.some((v,j)=>Math.abs(v-f[p+8+j])>1e-3) || e.normal.some((v,j)=>Math.abs(v-f[p+12+j])>1e-3))))throw Error('GPU/CPU ray mismatch '+i);checked++;});
      const textureChecks=await checkTextures(this.device,textures,this.textureLayout,this.textureGroup!,this.metadata!,this.sceneBytes[4],()=>serial===this.serial);
      return {rays:checked,diagnostics,bsdf:await checkBsdf(this.device,bsdf),textures:textureChecks};
    }finally{buffer.destroy();readback.destroy();diagnostic.destroy();}
  }
  private frame(){
    this.raf=requestAnimationFrame(()=>this.frame());
    if(!this.metadata?.renderable || !this.computeGroup || this.failed)return;
    const bytes=new ArrayBuffer(128),f=new Float32Array(bytes),u=new Uint32Array(bytes);f.set(this.camera);u.set([this.width,this.height,this.sample,Math.max(1,Math.min(1024,this.depth))],16);
    f.set([Math.tan(this.metadata.fov*Math.PI/360),this.exposure,this.metadata.filter==='triangle'?1:0,this.metadata.filterRadius[0]],20);f.set([this.metadata.filterRadius[1],this.lod0?1:0,0,this.clamp?1:0],24);u[26]=this.seed>>>0;
    u[28]=(this.alpha?1:0)|(this.bump?2:0);
    this.device.queue.writeBuffer(this.uniform,0,bytes);const encoder=this.device.createCommandEncoder();
    if(!this.paused){const compute=encoder.beginComputePass();compute.setPipeline(this.compute);compute.setBindGroup(0,this.computeGroup);compute.setBindGroup(1,this.textureGroup!);compute.dispatchWorkgroups(Math.ceil(this.width/8),Math.ceil(this.height/8));compute.end();this.sample++;if(this.maxSpp>0 && this.sample>=this.maxSpp)this.paused=true;this.stats(this.sample);}
    const render=encoder.beginRenderPass({colorAttachments:[{view:this.context.getCurrentTexture().createView(),loadOp:'clear',storeOp:'store',clearValue:{r:0,g:0,b:0,a:1}}]});render.setPipeline(this.present);render.setBindGroup(0,this.presentGroup!);render.draw(3);render.end();this.device.queue.submit([encoder.finish()]);
    if(this.sample%32===1 && !this.checksPending)void this.checkDiagnostics();
  }
  private async checkDiagnostics(){
    this.checksPending=true;const serial=this.serial;const b=this.device.createBuffer({size:16,usage:GPUBufferUsage.COPY_DST|GPUBufferUsage.MAP_READ});
    try{const enc=this.device.createCommandEncoder();enc.copyBufferToBuffer(this.diagnostics,0,b,0,16);this.device.queue.submit([enc.finish()]);await b.mapAsync(GPUMapMode.READ);const counts=[...new Uint32Array(b.getMappedRange())];if(serial===this.serial && counts.some(v=>v)){this.failed=true;this.status('GPU 诊断失败：'+JSON.stringify(counts),true);}}
    catch(e){if(serial===this.serial){this.failed=true;this.status(String(e),true);}}finally{b.destroy();this.checksPending=false;}
  }
  orbit(dx:number,dy:number){
    if(!this.metadata?.renderable)return;
    const m=this.camera,eye=[m[3],m[7],m[11]],forward=[m[2],m[6],m[10]],target=eye.map((v,i)=>v+forward[i]*6);
    let off=eye.map((v,i)=>v-target[i]);let theta=Math.atan2(off[0],off[2])+dx*.005;let phi=Math.acos(Math.max(-.99,Math.min(.99,off[1]/6)))+dy*.005;phi=Math.max(.1,Math.min(Math.PI-.1,phi));
    off=[6*Math.sin(phi)*Math.sin(theta),6*Math.cos(phi),6*Math.sin(phi)*Math.cos(theta)];const pos=target.map((v,i)=>v+off[i]);
    const z=off.map(v=>-v/6);const {x,y}=this.cameraAxes(z);
    this.camera=[x[0],y[0],z[0],pos[0],x[1],y[1],z[1],pos[1],x[2],y[2],z[2],pos[2],0,0,0,1];this.reset();
  }
  private cameraAxes(z:number[]){
    const m=this.metadata!.camera.transform;
    const determinant=m[0]*(m[5]*m[10]-m[6]*m[9])-m[1]*(m[4]*m[10]-m[6]*m[8])+m[2]*(m[4]*m[9]-m[5]*m[8]);
    const sign=determinant<0?1:-1,right=[-z[2]*sign,0,z[0]*sign],len=Math.hypot(...right),x=right.map(v=>v/len);
    const y=[(x[1]*z[2]-x[2]*z[1])*sign,(x[2]*z[0]-x[0]*z[2])*sign,(x[0]*z[1]-x[1]*z[0])*sign];return {x,y};
  }
  look(dx:number,dy:number){
    if(!this.metadata?.renderable)return;const m=this.camera;
    const yaw=Math.atan2(m[2],-m[10])+dx,pitch=Math.max(-1.52,Math.min(1.52,Math.asin(Math.max(-1,Math.min(1,m[6])))+dy));
    const z=[Math.sin(yaw)*Math.cos(pitch),Math.sin(pitch),-Math.cos(yaw)*Math.cos(pitch)],{x,y}=this.cameraAxes(z);
    this.camera=[x[0],y[0],z[0],m[3],x[1],y[1],z[1],m[7],x[2],y[2],z[2],m[11],0,0,0,1];this.reset();
  }
  move(dx:number,dy:number,dz:number){
    if(!this.metadata?.renderable)return;for(let row=0;row<3;row++)this.camera[row*4+3]+=this.camera[row*4]*dx+this.camera[row*4+1]*dy+this.camera[row*4+2]*dz;this.reset();
  }
  disposeScene(){this.serial++;for(const b of this.owned)b.destroy();for(const t of this.imageTextures)t.destroy();this.imageTextures=[];this.textureGroup=undefined;this.owned=[];this.metadata=undefined;this.computeGroup=undefined;this.presentGroup=undefined;this.sceneGPU=[];this.sceneBytes=[];this.reset();}
  destroy(){cancelAnimationFrame(this.raf);this.disposeScene();this.white?.destroy();this.uniform?.destroy();this.device?.destroy();}
}
