// SPDX-License-Identifier: Apache-2.0
fn surface_material(hit:Hit,width:f32)->Bsdf {
  let id=info(hit.inst).y;let b=records[2]+id*16u;let uv=surface_uv(hit);let texture=records[b+13u];
  let color=floats(b).xyz*texture_value(texture,uv.uv,texture_lod(texture,uv,width));let eta=floats(b+4u);let k=floats(b+8u);
  return Bsdf(color,records[b+12u],eta.xyz,eta.w,k.xyz,k.w);
}
fn surface_alpha(hit:Hit,width:f32)->f32 {if((frame.features.x&1u)==0u){return 1;}let b=records[12]+hit.inst*4u;let uv=surface_uv(hit);let id=records[b+1u];return clamp(bitcast<f32>(records[b])*texture_scalar(id,uv.uv,texture_lod(id,uv,width)),0,1);}
fn surface_frame(hit:Hit,ray:Ray,width:f32)->mat3x3f {
  let uv=surface_uv(hit);let n=mapped_normal(hit,ray,width,uv);let helper=select(vec3f(1,0,0),vec3f(0,0,1),abs(n.z)<.999);var x=normalize(cross(helper,n));
  if(uv.valid!=0u){let tangent=uv.du-n*dot(uv.du,n);if(dot(tangent,tangent)>1e-20){x=normalize(tangent);}}
  return mat3x3f(x,cross(n,x),n);
}
// Straight-through dielectric shadows are the documented LT approximation,
// not refracted caustics. Exhaustion is a diagnosed failure, never visibility.
fn visibility(initial:Ray,distance:f32,width:f32,state:ptr<function,u32>)->vec3f {
  var ray=initial;var remaining=distance;var weight=vec3f(1);
  for(var step=0u;step<128u;step++){
    let hit=intersect(ray,remaining);if(hit.failed!=0u){return vec3f(0);}if(hit.valid==0u){return weight;}
    let g=geometric(hit.inst,primitives[hit.prim]);let p=ray.o+ray.d*hit.t;
    if(random(state)<surface_alpha(hit,width)){
      let m=surface_material(hit,width);if(m.kind!=3u){return vec3f(0);}
      let eta=select(1/m.eta.x,m.eta.x,dot(g,-ray.d)>0);
      weight*=1-dielectric_fresnel(abs(dot(g,ray.d)),eta);if(max(weight.x,max(weight.y,weight.z))<1e-6){return vec3f(0);}
    }
    let origin=offset(p,g,ray.d);remaining-=hit.t+max(0.,dot(origin-p,ray.d));if(remaining<=1e-5){return weight;}ray=Ray(origin,ray.d);
  }atomicAdd(&diagnostics[3],1u);return vec3f(0);
}
fn trace(initial:Ray,state:ptr<function,u32>)->vec3f {
  var ray=initial;var radiance=vec3f(0);var throughput=vec3f(1);var previous=ray.o;var previous_pdf=0.;var previous_delta=true;
  var depth=0u;var transmission_steps=0u;var alpha_steps=0u;var eta_scale=1.;var cone_width=0.;
  var cone_spread=2*frame.params.x/f32(min(frame.size.x,frame.size.y));
  let light_count=records[7]+records[11];let environment=floats(records[10]).xyz;
  for(var event=0u;event<frame.size.w+160u;event++){
    if(event==frame.size.w+159u){atomicAdd(&diagnostics[3],1u);break;}
    if(depth>=frame.size.w){break;}
    let hit=intersect(ray,1e30);if(hit.failed!=0u){break;}
    if(hit.valid==0u){
      var weight=1.;if(!previous_delta && records[11]>0u){weight=power(previous_pdf,1/(4*PI*f32(light_count)));}
      radiance+=clamp_sample(throughput*environment*weight,depth);break;
    }
    cone_width+=hit.t*cone_spread;let p=ray.o+ray.d*hit.t;let primitive=primitives[hit.prim];let ids=info(hit.inst);let g=geometric(hit.inst,primitive);
    let footprint=cone_width/max(.05,abs(dot(g,ray.d)));
    if(random(state)>=surface_alpha(hit,footprint)){
      alpha_steps++;if(alpha_steps>=128u){atomicAdd(&diagnostics[3],1u);break;}ray=Ray(offset(p,g,ray.d),ray.d);continue;
    }alpha_steps=0u;
    let emission=floats(ibase(hit.inst)+32u);
    if(any(emission.xyz>vec3f(0))){
      if(emission.w>0 || dot(g,-ray.d)>0){var weight=1.;if(!previous_delta){weight=power(previous_pdf,light_pdf(previous,p,hit.inst,hit.prim));}radiance+=clamp_sample(throughput*emission.xyz*weight,depth);}break;
    }
    if(ids.y>=records[3]){atomicAdd(&diagnostics[1],1u);break;}
    let m=surface_material(hit,footprint);if(m.kind>3u){atomicAdd(&diagnostics[1],1u);break;}
    let basis=surface_frame(hit,ray,footprint);let inverse_basis=transpose(basis);let wo=inverse_basis*(-ray.d);
    let eta=select(1/m.eta.x,m.eta.x,dot(g,-ray.d)>0);
    if(light_count>0u && !(is_specular(m) && (m.kind==2u || m.kind==3u))){
      let chosen=min(u32(random(state)*f32(light_count)),light_count-1u);
      var direction=vec3f(0);var le=environment;var pdf=1/(4*PI*f32(light_count));var distance=1e30;
      if(chosen<records[7]){
        let b=records[6]+chosen*4u;let li=records[b];let lp=records[b+1u];let light=primitives[lp];let u=sqrt(random(state));let v=random(state);
        let light_point=point(li,vertices[light.v.x].p.xyz*(1-u)+vertices[light.v.y].p.xyz*(u*(1-v))+vertices[light.v.z].p.xyz*(u*v));
        let delta=light_point-p;distance=length(delta);direction=delta/max(1e-20,distance);let ln=geometric(li,light);let emission_light=floats(ibase(li)+32u);
        let lightcos=select(max(0.,dot(ln,-direction)),abs(dot(ln,-direction)),emission_light.w>0);
        le=emission_light.xyz;if(lightcos<=0){le=vec3f(0);}pdf=light_pdf(p,light_point,li,lp);
      }else{
        let z=1-2*random(state);let phi=2*PI*random(state);let r=sqrt(max(0.,1-z*z));direction=vec3f(r*cos(phi),z,r*sin(phi));
      }
      let wi=inverse_basis*direction;let value=bsdf_eval(m,wo,wi,eta);
      if(any(value>vec3f(0)) && any(le>vec3f(0))){
        let origin=offset(p,g,direction);let limit=distance-max(2e-5,distance*1e-5);
        let visible=visibility(Ray(origin,direction),limit,cone_width,state);let bsdf_density=bsdf_pdf(m,wo,wi,eta);
        radiance+=clamp_sample(throughput*value*le*visible*(abs(wi.z)*power(pdf,bsdf_density)/max(pdf,1e-30)),depth);
      }
    }
    if(depth>=3u){let survive=clamp(max(throughput.x,max(throughput.y,throughput.z))*eta_scale,.05,.95);if(random(state)>survive){break;}throughput/=survive;}
    let sample=bsdf_sample(m,wo,eta,vec3f(random(state),random(state),random(state)));
    if(sample.pdf<=0 || !any(sample.weight>vec3f(0))){break;}
    if(any(sample.weight!=sample.weight) || any(abs(sample.weight)>vec3f(1e30))){atomicAdd(&diagnostics[2],1u);break;}
    let direction=normalize(basis*sample.wi);let crosses=dot(direction,g)*dot(-ray.d,g)<0;
    if(crosses!=(sample.transmitted!=0u)){break;}
    previous=p;previous_pdf=sample.pdf;previous_delta=sample.delta!=0u;throughput*=sample.weight;
    if(sample.transmitted!=0u){eta_scale*=sample.eta*sample.eta;cone_spread/=sample.eta;}
    else if(sample.delta==0u){cone_spread+=select(.25,.5*max(m.ax,m.ay),m.kind==2u || m.kind==3u);}
    if(sample.transmitted!=0u && transmission_steps<12u){transmission_steps++;}else{depth++;}
    ray=Ray(offset(p,g,direction),direction);
  }return radiance;
}
