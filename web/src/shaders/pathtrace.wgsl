// LightTransport-derived PCG RNG and diffuse trace/NEE/MIS/RR control flow.
// Source: breeze028/LightTransport@47a461d2 src/gpu/{math,shading,kernel}.cuh.
// Modified: WGSL, shared BLAS/TLAS, explicit records, per-pixel sample count.
struct Vertex { p:vec4f, n:vec4f, uv:vec4f }
struct Primitive { v:vec3u, shape:u32 }
struct Node { lo:vec3f, left:u32, hi:vec3f, count:u32 }
struct Frame { camera:array<vec4f,4>, size:vec4u, params:vec4f, sampling:vec4f }
struct Ray { o:vec3f, d:vec3f }
struct Hit { t:f32, uv:vec2f, prim:u32, inst:u32, valid:u32, failed:u32 }
@group(0) @binding(0) var<uniform> frame:Frame;
@group(0) @binding(1) var<storage,read> nodes:array<Node>;
@group(0) @binding(2) var<storage,read> vertices:array<Vertex>;
@group(0) @binding(3) var<storage,read> primitives:array<Primitive>;
@group(0) @binding(4) var<storage,read> records:array<u32>;
@group(0) @binding(5) var<storage,read_write> accumulation:array<vec4f>;
@group(0) @binding(6) var<storage,read_write> diagnostics:array<atomic<u32>>;
const PI:f32=3.141592653589793;
const NONE:u32=0xffffffffu;
fn random(state:ptr<function,u32>)->f32 {
  *state=(*state)*747796405u+2891336453u;
  let word=(((*state)>>(((*state)>>28u)+4u))^(*state))*277803737u;
  return f32(((word>>22u)^word)>>8u)/16777216.;
}
fn floats(offset:u32)->vec4f { return bitcast<vec4f>(vec4u(records[offset],records[offset+1],records[offset+2],records[offset+3])); }
fn ibase(i:u32)->u32 {return records[4]+i*40u;}
fn info(i:u32)->vec4u {let k=ibase(i)+36u;return vec4u(records[k],records[k+1],records[k+2],records[k+3]);}
fn xform(i:u32,base:u32,v:vec4f)->vec3f {let k=ibase(i)+base;return vec3f(dot(floats(k),v),dot(floats(k+4),v),dot(floats(k+8),v));}
fn point(i:u32,v:vec3f)->vec3f {return xform(i,0,vec4f(v,1));}
fn normal(i:u32,n:vec3f)->vec3f {
  let k=ibase(i)+16u;let a=floats(k).xyz;let b=floats(k+4).xyz;let c=floats(k+8).xyz;
  return normalize(vec3f(a.x*n.x+b.x*n.y+c.x*n.z,a.y*n.x+b.y*n.y+c.y*n.z,a.z*n.x+b.z*n.y+c.z*n.z));
}
fn bounds(ray:Ray,node:Node,limit:f32)->bool {
  var near=0.00001;var far=limit;
  for(var i=0u;i<3u;i++) {
    if(abs(ray.d[i])<1e-20) {if(ray.o[i]<node.lo[i] || ray.o[i]>node.hi[i]) {return false;}}
    else {let a=(node.lo[i]-ray.o[i])/ray.d[i];let b=(node.hi[i]-ray.o[i])/ray.d[i];near=max(near,min(a,b));far=min(far,max(a,b));if(far<near){return false;}}
  } return true;
}
fn triangle(ray:Ray,p:Primitive,limit:f32)->vec3f {
  let a=vertices[p.v.x].p.xyz;let e1=vertices[p.v.y].p.xyz-a;let e2=vertices[p.v.z].p.xyz-a;
  let q=cross(ray.d,e2);let det=dot(e1,q);if(abs(det)<1e-10){return vec3f(-1);}
  let s=ray.o-a;let u=dot(s,q)/det;let r=cross(s,e1);let v=dot(ray.d,r)/det;let t=dot(e2,r)/det;
  if(u<0 || v<0 || u+v>1 || t<0.00001 || t>=limit){return vec3f(-1);}return vec3f(t,u,v);
}
fn blas(ray:Ray,root:u32,instance:u32,previous:Hit)->Hit {
  var result=previous; if(root==NONE){return result;}
  var stack:array<u32,64>;var count=1u;stack[0]=root;
  loop {
    if(count==0u){break;}count--;let node=nodes[stack[count]];if(!bounds(ray,node,result.t)){continue;}
    if(node.count>0u) {
      for(var j=0u;j<node.count;j++){let index=node.left+j;let candidate=triangle(ray,primitives[index],result.t);if(candidate.x>0){result=Hit(candidate.x,candidate.yz,index,instance,1u,0u);}}
    } else {
      if(count+2u>64u){atomicAdd(&diagnostics[0],1u);result.failed=1u;return result;}
      stack[count]=node.left;stack[count+1u]=node.left+1u;count+=2u;
    }
  }return result;
}
fn intersect(ray:Ray,limit:f32)->Hit {
  var result=Hit(limit,vec2f(0),0u,0u,0u,0u);if(records[1]==NONE){return result;}
  var stack:array<u32,64>;var count=1u;stack[0]=records[1];
  loop {
    if(count==0u){break;}count--;let node=nodes[stack[count]];if(!bounds(ray,node,result.t)){continue;}
    if(node.count>0u) {
      for(var j=0u;j<node.count;j++){
        let i=node.left+j;let local=Ray(xform(i,16u,vec4f(ray.o,1)),xform(i,16u,vec4f(ray.d,0)));
        result=blas(local,info(i).x,i,result);if(result.failed!=0u){return result;}
      }
    } else {
      if(count+2u>64u){atomicAdd(&diagnostics[0],1u);result.failed=1u;return result;}
      stack[count]=node.left;stack[count+1u]=node.left+1u;count+=2u;
    }
  }return result;
}
fn geometric(i:u32,p:Primitive)->vec3f {
  var n=normalize(cross(point(i,vertices[p.v.y].p.xyz)-point(i,vertices[p.v.x].p.xyz),point(i,vertices[p.v.z].p.xyz)-point(i,vertices[p.v.x].p.xyz)));
  let flags=info(i).w;if(((flags&1u)!=0u)!=((flags&2u)!=0u)){n=-n;}return n;
}
fn shading(hit:Hit,ray:Ray)->vec3f {
  let g=geometric(hit.inst,primitives[hit.prim]);var n=g;
  let p=primitives[hit.prim];let a=vertices[p.v.x];let b=vertices[p.v.y];let c=vertices[p.v.z];
  if(a.n.w>0 && b.n.w>0 && c.n.w>0){n=normal(hit.inst,a.n.xyz*(1-hit.uv.x-hit.uv.y)+b.n.xyz*hit.uv.x+c.n.xyz*hit.uv.y);if((info(hit.inst).w&1u)!=0u){n=-n;}if(dot(n,g)<0){n=-n;}}
  if(dot(n,-ray.d)<0){n=-n;}return n;
}
fn area(i:u32,p:Primitive)->f32 {return .5*length(cross(point(i,vertices[p.v.y].p.xyz)-point(i,vertices[p.v.x].p.xyz),point(i,vertices[p.v.z].p.xyz)-point(i,vertices[p.v.x].p.xyz)));}
fn power(a:f32,b:f32)->f32 {return a*a/max(a*a+b*b,1e-20);}
fn offset(p:vec3f,n:vec3f,d:vec3f)->vec3f {return p+n*select(-1.,1.,dot(n,d)>=0)*max(1e-5,2e-5*max(abs(p.x),max(abs(p.y),abs(p.z))));}
fn clamp_sample(v:vec3f,depth:u32)->vec3f {
  if(any(v!=v) || any(abs(v)>vec3f(1e30))){atomicAdd(&diagnostics[2],1u);return vec3f(0);}
  if(frame.sampling.w==0){return v;}return clamp(v,vec3f(0),vec3f(select(8.,64.,depth==0u)));
}
fn light_pdf(origin:vec3f,p:vec3f,i:u32,primitive:u32)->f32 {
  let n=geometric(i,primitives[primitive]);let delta=p-origin;let emission=floats(ibase(i)+32u);
  let cosine=select(max(0.,dot(n,-normalize(delta))),abs(dot(n,-normalize(delta))),emission.w>0);
  return dot(delta,delta)/max(1e-20,cosine*area(i,primitives[primitive])*f32(records[7]));
}
fn trace(initial:Ray,state:ptr<function,u32>)->vec3f {
  var ray=initial;var radiance=vec3f(0);var throughput=vec3f(1);var previous=ray.o;var previous_pdf=0.;
  for(var bounce=0u;bounce<frame.size.w;bounce++) {
    let hit=intersect(ray,1e30);if(hit.failed!=0u){break;}if(hit.valid==0u){break;}
    let p=ray.o+ray.d*hit.t;let primitive=primitives[hit.prim];let instance_info=info(hit.inst);
    let g=geometric(hit.inst,primitive);let emission=floats(ibase(hit.inst)+32u);
    if(any(emission.xyz>vec3f(0))) {
      if(emission.w>0 || dot(g,-ray.d)>0){var weight=1.;if(bounce>0u){weight=power(previous_pdf,light_pdf(previous,p,hit.inst,hit.prim));}radiance+=clamp_sample(throughput*emission.xyz*weight,bounce);}break;
    }
    if(instance_info.y>=records[3]){atomicAdd(&diagnostics[1],1u);break;}
    let material=records[2]+instance_info.y*16u;if(records[material+12u]!=0u){atomicAdd(&diagnostics[1],1u);break;}
    let albedo=floats(material).xyz;let n=shading(hit,ray);
    if(records[7]>0u) {
      let chosen=min(u32(random(state)*f32(records[7])),records[7]-1u);let base=records[6]+chosen*4u;let li=records[base];let lp=records[base+1u];let light=primitives[lp];
      let u=sqrt(random(state));let v=random(state);let lightPoint=point(li,vertices[light.v.x].p.xyz*(1-u)+vertices[light.v.y].p.xyz*(u*(1-v))+vertices[light.v.z].p.xyz*(u*v));
      let direction=normalize(lightPoint-p);let cosine=max(0.,dot(n,direction));let ln=geometric(li,light);let le=floats(ibase(li)+32u);
      let lightcos=select(max(0.,dot(ln,-direction)),abs(dot(ln,-direction)),le.w>0);
      if(cosine>0 && lightcos>0) {
        let origin=offset(p,g,direction);let shadow_delta=lightPoint-origin;
        let shadow=intersect(Ray(origin,normalize(shadow_delta)),length(shadow_delta)-max(2e-5,length(shadow_delta)*1e-5));
        if(shadow.failed!=0u){break;}
        if(shadow.valid==0u){let pdf=light_pdf(p,lightPoint,li,lp);let bsdf=cosine/PI;let weight=power(pdf,bsdf);radiance+=clamp_sample(throughput*albedo*le.xyz*(bsdf*weight/max(pdf,1e-20)),bounce);}
      }
    }
    if(bounce>=3u){let survive=clamp(max(throughput.x,max(throughput.y,throughput.z)),.05,.95);if(random(state)>survive){break;}throughput/=survive;}
    let r=sqrt(random(state));let angle=2*PI*random(state);let local=vec3f(r*cos(angle),r*sin(angle),sqrt(max(0.,1-r*r)));
    let helper=select(vec3f(1,0,0),vec3f(0,0,1),abs(n.z)<.999);let tangent=normalize(cross(helper,n));let bitangent=cross(n,tangent);
    let direction=normalize(tangent*local.x+bitangent*local.y+n*local.z);
    previous=p;previous_pdf=max(0.,dot(n,direction))/PI;throughput*=albedo;ray=Ray(offset(p,g,direction),direction);
  }return radiance;
}
fn tent(state:ptr<function,u32>,radius:f32)->f32 {let u=random(state);if(u<.5){return radius*(sqrt(2*u)-1);}return radius*(1-sqrt(2-2*u));}
@compute @workgroup_size(8,8)
fn main(@builtin(global_invocation_id) id:vec3u) {
  if(id.x>=frame.size.x || id.y>=frame.size.y){return;}let index=id.y*frame.size.x+id.x;
  var state=(id.x*1973u+id.y*9277u+(frame.size.z+1u)*26699u)|1u;
  var jitter=vec2f((2*random(&state)-1)*frame.params.w,(2*random(&state)-1)*frame.sampling.x);
  if(frame.params.z>0){jitter=vec2f(tent(&state,frame.params.w),tent(&state,frame.sampling.x));}
  let aspect=f32(frame.size.x)/f32(frame.size.y);let sx=(2*(f32(id.x)+.5+jitter.x)/f32(frame.size.x)-1)*frame.params.x*max(aspect,1.);
  let sy=(1-2*(f32(id.y)+.5+jitter.y)/f32(frame.size.y))*frame.params.x*max(1/aspect,1.);
  let local=vec4f(sx,sy,1,0);let d=normalize(vec3f(dot(frame.camera[0],local),dot(frame.camera[1],local),dot(frame.camera[2],local)));
  let o=vec3f(frame.camera[0].w,frame.camera[1].w,frame.camera[2].w);let color=trace(Ray(o,d),&state);
  if(frame.size.z==0u){accumulation[index]=vec4f(color,1);}else{accumulation[index]+=vec4f(color,1);}
}
// Query mode reuses an isolated accumulation buffer as input/output, not image history.
@compute @workgroup_size(64)
fn probe(@builtin(global_invocation_id) id:vec3u) {
  let index=id.x;if(index>=arrayLength(&accumulation)/4u){return;}
  let ray=Ray(accumulation[index*4u].xyz,accumulation[index*4u+1u].xyz);let hit=intersect(ray,1e30);
  accumulation[index*4u]=vec4f(hit.t,hit.uv,bitcast<f32>(hit.prim));
  accumulation[index*4u+1u]=bitcast<vec4f>(vec4u(hit.inst,hit.valid,hit.failed,0));
  if(hit.valid!=0u){accumulation[index*4u+2u]=vec4f(geometric(hit.inst,primitives[hit.prim]),0);accumulation[index*4u+3u]=vec4f(shading(hit,ray),0);}
}
