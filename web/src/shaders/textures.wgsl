// SPDX-License-Identifier: Apache-2.0
@group(1) @binding(0) var image_sampler:sampler;
@group(1) @binding(1) var image0:texture_2d<f32>;
@group(1) @binding(2) var image1:texture_2d<f32>;
@group(1) @binding(3) var image2:texture_2d<f32>;
@group(1) @binding(4) var image3:texture_2d<f32>;
@group(1) @binding(5) var image4:texture_2d<f32>;
@group(1) @binding(6) var image5:texture_2d<f32>;
@group(1) @binding(7) var image6:texture_2d<f32>;
@group(1) @binding(8) var image7:texture_2d<f32>;
@group(1) @binding(9) var image8:texture_2d<f32>;
@group(1) @binding(10) var image9:texture_2d<f32>;
@group(1) @binding(11) var image10:texture_2d<f32>;
@group(1) @binding(12) var image11:texture_2d<f32>;
fn image_sample(slot:u32,uv:vec2f,lod:f32)->vec3f {
  switch slot {
    case 0u:{return textureSampleLevel(image0,image_sampler,uv,lod).xyz;}
    case 1u:{return textureSampleLevel(image1,image_sampler,uv,lod).xyz;}
    case 2u:{return textureSampleLevel(image2,image_sampler,uv,lod).xyz;}
    case 3u:{return textureSampleLevel(image3,image_sampler,uv,lod).xyz;}
    case 4u:{return textureSampleLevel(image4,image_sampler,uv,lod).xyz;}
    case 5u:{return textureSampleLevel(image5,image_sampler,uv,lod).xyz;}
    case 6u:{return textureSampleLevel(image6,image_sampler,uv,lod).xyz;}
    case 7u:{return textureSampleLevel(image7,image_sampler,uv,lod).xyz;}
    case 8u:{return textureSampleLevel(image8,image_sampler,uv,lod).xyz;}
    case 9u:{return textureSampleLevel(image9,image_sampler,uv,lod).xyz;}
    case 10u:{return textureSampleLevel(image10,image_sampler,uv,lod).xyz;}
    case 11u:{return textureSampleLevel(image11,image_sampler,uv,lod).xyz;}
    default:{return vec3f(0);}
  }
}
struct UVContext {uv:vec2f, valid:u32, du:vec3f, dv:vec3f}
fn surface_uv(hit:Hit)->UVContext {
  let p=primitives[hit.prim];let a=vertices[p.v.x];let b=vertices[p.v.y];let c=vertices[p.v.z];
  let uv=a.uv.xy*(1-hit.uv.x-hit.uv.y)+b.uv.xy*hit.uv.x+c.uv.xy*hit.uv.y;
  let duv1=b.uv.xy-a.uv.xy;let duv2=c.uv.xy-a.uv.xy;let det=duv1.x*duv2.y-duv1.y*duv2.x;
  if(a.uv.z==0 || b.uv.z==0 || c.uv.z==0 || abs(det)<1e-12){return UVContext(uv,0u,vec3f(1,0,0),vec3f(0,1,0));}
  let e1=point(hit.inst,b.p.xyz)-point(hit.inst,a.p.xyz);let e2=point(hit.inst,c.p.xyz)-point(hit.inst,a.p.xyz);
  return UVContext(uv,1u,(e1*duv2.y-e2*duv1.y)/det,(-e1*duv2.x+e2*duv1.x)/det);
}
fn uv_density(uv:UVContext)->vec2f {
  let area=length(cross(uv.du,uv.dv));return vec2f(length(uv.dv),length(uv.du))/max(1e-20,area);
}
fn texture_lod(id:u32,uv:UVContext,width:f32)->f32 {
  if(id==NONE || id>=records[9] || frame.sampling.y>0){return 0;}
  let b=records[8]+id*8u;let scale=floats(b+4u).xy;let density=uv_density(uv);
  let footprint=width*density*abs(scale)*vec2f(f32(records[b+1u]),f32(records[b+2u]));
  return clamp(log2(max(1.,max(footprint.x,footprint.y))),0,f32(records[b+3u]-1u));
}
fn texture_value(id:u32,uv:vec2f,lod:f32)->vec3f {
  if(id==NONE){return vec3f(1);}if(id>=records[9]){atomicAdd(&diagnostics[1],1u);return vec3f(0);}
  let b=records[8]+id*8u;let slot=records[b];if(slot>=12u){atomicAdd(&diagnostics[1],1u);return vec3f(0);}
  let transform=floats(b+4u);let mapped=uv*transform.xy+transform.zw;
  return image_sample(slot,vec2f(mapped.x,1-mapped.y),lod);
}
fn texture_scalar(id:u32,uv:vec2f,lod:f32)->f32 {return dot(texture_value(id,uv,lod),vec3f(1./3));}
fn bump_normal(du:vec3f,dv:vec3f,n:vec3f,dh:vec2f,h:f32,dndu:vec3f,dndv:vec3f)->vec3f {
  let displaced_u=du+dh.x*n+h*dndu;let displaced_v=dv+dh.y*n+h*dndv;
  let v=cross(displaced_u,displaced_v);if(dot(v,v)<1e-20){return n;}let result=normalize(v);return select(result,-result,dot(result,n)<0);
}
fn mapped_normal(hit:Hit,ray:Ray,width:f32,uv:UVContext)->vec3f {
  let base=shading(hit,ray);let m=records[2]+info(hit.inst).y*16u;let id=records[m+14u];
  if(id==NONE || uv.valid==0u || (frame.features.x&2u)==0u){return base;}
  let t=records[8]+id*8u;let density=uv_density(uv);
  let step_uv=max(vec2f(1/f32(records[t+1u]),1/f32(records[t+2u])),.5*width*density);
  let lod=texture_lod(id,uv,width);let h=texture_scalar(id,uv.uv,lod);
  let dh=vec2f(texture_scalar(id,uv.uv+vec2f(step_uv.x,0),lod)-h,texture_scalar(id,uv.uv+vec2f(0,step_uv.y),lod)-h)/step_uv;
  var dndu=vec3f(0);var dndv=vec3f(0);let primitive=primitives[hit.prim];let a=vertices[primitive.v.x];let b=vertices[primitive.v.y];let c=vertices[primitive.v.z];
  if(a.n.w>0 && b.n.w>0 && c.n.w>0){
    let n0=normal_vector(hit.inst,a.n.xyz);let n1=normal_vector(hit.inst,b.n.xyz);let n2=normal_vector(hit.inst,c.n.xyz);
    let raw=n0*(1-hit.uv.x-hit.uv.y)+n1*hit.uv.x+n2*hit.uv.y;let len=max(1e-10,length(raw));let n=raw/len;
    let u1=b.uv.xy-a.uv.xy;let u2=c.uv.xy-a.uv.xy;let det=u1.x*u2.y-u1.y*u2.x;
    let nu=((n1-n0)*u2.y-(n2-n0)*u1.y)/det;let nv=(-(n1-n0)*u2.x+(n2-n0)*u1.x)/det;
    let sign=select(1.,-1.,dot(n,base)<0);dndu=sign*(nu-n*dot(n,nu))/len;dndv=sign*(nv-n*dot(n,nv))/len;
  }
  var n=bump_normal(uv.du,uv.dv,base,dh,h,dndu,dndv);let g=geometric(hit.inst,primitive);
  if(dot(n,g)*dot(base,g)<0){n=-n;}
  // Preserve the geometric hemisphere while avoiding a view-facing flip across it.
  if(dot(n,-ray.d)<1e-5){n=normalize(n+base*((1e-5-dot(n,-ray.d))/max(1e-5,dot(base,-ray.d))));}return n;
}
