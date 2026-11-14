// Actual GPU evaluations checked against analytic values, conservation bounds,
// Fresnel event frequencies and f*cos/pdf consistency, not a second shader port.
const PI=Math.PI;
type V=[number,number,number];
interface Material {kind:number;color:V;eta:V;k:V;ax:number;ay:number}
interface Case {m:Material;wo:V;wi:V;eta:number;u:V;value?:V;pdf?:number;weight?:V;trans?:number;energy?:number;sampleEnergy?:number;frequency?:boolean}
export async function checkBsdf(device:GPUDevice,source:string){
  const cases:Case[]=[],normal:V=[0,0,1],front=1.5;
  const material=(kind:number,ax=.2,ay=ax):Material=>({kind,color:[.6,.3,.1],eta:[1.5,1.5,1.5],k:[.2,.2,0],ax,ay});
  const add=(m:Material,more:Partial<Case>={})=>cases.push({m,wo:normal,wi:normal,eta:front,u:[.13,.67,.7],...more});
  add(material(0),{value:[.6/PI,.3/PI,.1/PI],pdf:1/PI,weight:[.6,.3,.1]});
  const metal={...material(2,.1),color:[1,1,1] as V,eta:[2,2,2] as V,k:[0,0,0] as V};
  add(metal,{value:Array(3).fill(1/(9*4*PI*.01)) as V,pdf:1/(4*PI*.01)});
  add({...metal,ax:0,ay:0},{value:[0,0,0],pdf:0,weight:[1/9,1/9,1/9]});
  const glass={...material(3,0),color:[1,1,1] as V};
  add(glass,{u:[.5,.5,.01],weight:[1,1,1],trans:0});
  add(glass,{u:[.5,.5,.5],weight:[1/2.25,1/2.25,1/2.25],trans:1});
  add(glass,{wo:[Math.sqrt(.75),0,.5],eta:1/1.5,u:[.2,.7,.99],weight:[1,1,1],trans:0});
  add({...glass,ax:.01,ay:.01},{value:Array(3).fill(.04/(4*PI*.0001)) as V,pdf:.04/(4*PI*.0001)});
  add({...glass,ax:.1,ay:.3},{wi:[.2,.1,Math.sqrt(.95)]});
  add({...material(1),color:[.5,.5,.5]},{value:Array(3).fill(.5/PI+2*.04/(4*PI*.04)) as V});
  const analytic=cases.length;
  const energyMaterials=[material(0),{...metal,ax:.3,ay:.2},{...material(1),color:[1,1,1] as V},{...glass,ax:.3,ay:.2}];
  for(let k=0;k<energyMaterials.length;k++)for(let row=0;row<32;row++)for(let column=0;column<64;column++){
    const z=-1+2*(row+.5)/32,phi=2*PI*(column+.5)/64,r=Math.sqrt(1-z*z);
    add(energyMaterials[k],{wi:[r*Math.cos(phi),r*Math.sin(phi),z],energy:k});
  }
  let seed=1234567;const random=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return (seed>>>8)/16777216;};
  for(let j=0;j<4096;j++)add({...glass,ax:.01,ay:.01},{u:[random(),random(),random()],frequency:true});
  for(let k=0;k<energyMaterials.length;k++)for(let j=0;j<4096;j++)add(energyMaterials[k],{u:[random(),random(),random()],sampleEnergy:k});
  const stride=48,data=new Float32Array(cases.length*stride),bits=new Uint32Array(data.buffer);
  cases.forEach((c,i)=>{const b=i*stride;data.set(c.m.color,b);bits[b+3]=c.m.kind;data.set([...c.m.eta,c.m.ax],b+4);data.set([...c.m.k,c.m.ay],b+8);data.set([...c.wo,c.eta],b+12);data.set(c.wi,b+16);data.set(c.u,b+20);});
  const query=`
const PI:f32=3.141592653589793;
${source}
@group(0) @binding(0) var<storage,read_write> query:array<vec4f>;
@compute @workgroup_size(64) fn main(@builtin(global_invocation_id) id:vec3u){
 let b=id.x*12u;if(b>=arrayLength(&query)){return;}
 let m=Bsdf(query[b].xyz,bitcast<u32>(query[b].w),query[b+1u].xyz,query[b+1u].w,query[b+2u].xyz,query[b+2u].w);
 let wo=query[b+3u].xyz;let eta=query[b+3u].w;let wi=query[b+4u].xyz;let u=query[b+5u].xyz;
 let s=bsdf_sample(m,wo,eta,u);
 query[b+6u]=vec4f(bsdf_eval(m,wo,wi,eta),bsdf_pdf(m,wo,wi,eta));
 query[b+7u]=vec4f(s.wi,s.pdf);query[b+8u]=vec4f(s.weight,f32(s.delta));
 query[b+9u]=vec4f(bsdf_eval(m,wo,s.wi,eta),bsdf_pdf(m,wo,s.wi,eta));
 query[b+10u]=vec4f(s.eta,f32(s.transmitted),dielectric_fresnel(wo.z,eta),0);
 query[b+11u]=vec4f(power(1e25,1e24),power(1e-25,2e-25),power(0,0),0);
}`;
  const module=device.createShaderModule({code:query,label:'BSDF analytic / energy / sampling checks'});
  const errors=(await module.getCompilationInfo()).messages.filter(m=>m.type==='error');if(errors.length)throw Error(errors.map(e=>e.message).join('\n'));
  const pipeline=await device.createComputePipelineAsync({layout:'auto',compute:{module,entryPoint:'main'}});
  const storage=device.createBuffer({size:data.byteLength,usage:GPUBufferUsage.STORAGE|GPUBufferUsage.COPY_DST|GPUBufferUsage.COPY_SRC});
  const readback=device.createBuffer({size:data.byteLength,usage:GPUBufferUsage.MAP_READ|GPUBufferUsage.COPY_DST});
  try{
    device.queue.writeBuffer(storage,0,data);
    const group=device.createBindGroup({layout:pipeline.getBindGroupLayout(0),entries:[{binding:0,resource:{buffer:storage}}]});
    const encoder=device.createCommandEncoder(),pass=encoder.beginComputePass();pass.setPipeline(pipeline);pass.setBindGroup(0,group);pass.dispatchWorkgroups(Math.ceil(cases.length/64));pass.end();encoder.copyBufferToBuffer(storage,0,readback,0,data.byteLength);device.queue.submit([encoder.finish()]);await readback.mapAsync(GPUMapMode.READ);
    const result=new Float32Array(readback.getMappedRange());const energy=energyMaterials.map(()=>[0,0,0]),sampledEnergy=energyMaterials.map(()=>[0,0,0]);let reflected=0;
    const close=(a:number,b:number,label:string,tolerance=4e-4)=>{if(!Number.isFinite(a)||Math.abs(a-b)>tolerance*Math.max(1,Math.abs(b)))throw Error(label+`: ${a} != ${b}`);};
    close(result[44],100/101,'MIS high PDF overflow');close(result[45],.2,'MIS small PDF underflow');close(result[46],0,'MIS zero PDFs');
    cases.forEach((c,i)=>{
      const b=i*stride;
      for(let j=24;j<44;j++)if(!Number.isFinite(result[b+j]))throw Error('Nonfinite GPU BSDF output '+i);
      c.value?.forEach((v,j)=>close(result[b+24+j],v,'analytic f '+i));if(c.pdf!==undefined)close(result[b+27],c.pdf,'analytic pdf '+i);
      c.weight?.forEach((v,j)=>close(result[b+32+j],v,'analytic weight '+i));if(c.trans!==undefined)close(result[b+41],c.trans,'TIR/transmission '+i);
      const sampledPdf=result[b+31],delta=result[b+35];
      if(sampledPdf>0){
        close(Math.hypot(...result.subarray(b+28,b+31)),1,'sample unit direction');
        if(delta===0){close(sampledPdf,result[b+39],'sample/eval PDF '+i);for(let j=0;j<3;j++)close(result[b+32+j],result[b+36+j]*Math.abs(result[b+30])/sampledPdf,'f*cos/pdf '+i);}
      }
      if(c.energy!==undefined){const scale=c.wi[2]<0&&c.m.kind===3?c.eta*c.eta:1;for(let j=0;j<3;j++)energy[c.energy][j]+=result[b+24+j]*Math.abs(c.wi[2])*scale*4*PI/(32*64);}
      if(c.sampleEnergy!==undefined){const scale=result[b+41]===1?c.eta*c.eta:1;for(let j=0;j<3;j++)sampledEnergy[c.sampleEnergy][j]+=result[b+32+j]*scale/4096;}
      if(c.frequency && result[b+41]===0)reflected++;
    });
    energy.forEach((e,i)=>e.forEach(v=>{if(v<0 || v>(i===2?1.15:1.025))throw Error('BSDF energy bound '+i+': '+v);}));
    energy[0].forEach((v,j)=>close(v,energyMaterials[0].color[j],'diffuse integral',.002));
    sampledEnergy.forEach((e,i)=>e.forEach(v=>{if(v<0 || v>(i===2?1.15:1.025))throw Error('Sampled BSDF energy bound '+i+': '+v);}));
    if(sampledEnergy[3][0]<.85)throw Error('Rough glass lost transmission energy: '+sampledEnergy[3][0]);
    if(Math.abs(reflected/4096-.04)>.015)throw Error('Rough glass Fresnel frequency: '+reflected/4096);
    return {checked:cases.length,analytic,energy,sampledEnergy,reflectedFraction:reflected/4096};
  }finally{storage.destroy();readback.destroy();}
}
