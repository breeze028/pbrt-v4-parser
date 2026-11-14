// SPDX-License-Identifier: Apache-2.0
// LightTransport diffuse/conductor/Principled+coat port; rough dielectric and
// visible-normal GGX follow PBRT-v4's microfacet equations. See THIRD_PARTY.md.
// Directions are away from the surface, in a frame where wo.z > 0.
struct Bsdf { color:vec3f, kind:u32, eta:vec3f, ax:f32, k:vec3f, ay:f32 }
struct Scatter { wi:vec3f, pdf:f32, weight:vec3f, delta:u32, eta:f32, transmitted:u32 }
fn power(a:f32,b:f32)->f32 {
  let scale=max(a,b);if(scale<=0){return 0;}let x=a/scale;let y=b/scale;return x*x/(x*x+y*y);
}
fn invalid_scatter()->Scatter {return Scatter(vec3f(0),0,vec3f(0),0u,1,0u);}
fn square(v:f32)->f32 {return v*v;}
fn schlick(c:f32)->f32 {return .04+.96*pow(clamp(1-c,0,1),5);}
fn dielectric_fresnel(c:f32,eta:f32)->f32 {
  let cosine=clamp(abs(c),0,1);let sin2=(1-cosine*cosine)/(eta*eta);
  if(sin2>=1){return 1;}let ct=sqrt(max(0.,1-sin2));
  let parallel=(eta*cosine-ct)/max(1e-20,eta*cosine+ct);
  let perpendicular=(cosine-eta*ct)/max(1e-20,cosine+eta*ct);
  return clamp(.5*(parallel*parallel+perpendicular*perpendicular),0,1);
}
fn conductor_channel(c:f32,eta:f32,k:f32)->f32 {
  let cos2=c*c;let sin2=max(0.,1-cos2);let t0=eta*eta-k*k-sin2;
  let a2b2=sqrt(max(0.,t0*t0+4*eta*eta*k*k));let a=sqrt(max(0.,.5*(a2b2+t0)));
  let t1=a2b2+cos2;let t2=2*c*a;let rs=(t1-t2)/max(1e-20,t1+t2);
  let t3=cos2*a2b2+sin2*sin2;let t4=t2*sin2;
  return clamp(.5*(rs+rs*(t3-t4)/max(1e-20,t3+t4)),0,1);
}
fn conductor_fresnel(c:f32,m:Bsdf)->vec3f {
  let v=clamp(abs(c),0,1);return vec3f(conductor_channel(v,m.eta.x,m.k.x),conductor_channel(v,m.eta.y,m.k.y),conductor_channel(v,m.eta.z,m.k.z));
}
fn is_specular(m:Bsdf)->bool {return max(m.ax,m.ay)<.001;}
fn ggx_d(h:vec3f,alpha:vec2f)->f32 {
  if(h.z<=0){return 0;}let a=max(alpha,vec2f(1e-4));
  let d=square(h.x/a.x)+square(h.y/a.y)+h.z*h.z;
  return 1/max(1e-30,PI*a.x*a.y*d*d);
}
fn ggx_lambda(w:vec3f,alpha:vec2f)->f32 {
  return .5*(sqrt(1+(square(w.x*alpha.x)+square(w.y*alpha.y))/max(1e-20,w.z*w.z))-1);
}
fn ggx_g(wo:vec3f,wi:vec3f,alpha:vec2f)->f32 {return 1/(1+ggx_lambda(wo,alpha)+ggx_lambda(wi,alpha));}
fn visible_pdf(wo:vec3f,h:vec3f,alpha:vec2f)->f32 {
  return ggx_d(h,alpha)*max(0.,dot(wo,h))/max(1e-20,wo.z*(1+ggx_lambda(wo,alpha)));
}
fn visible_normal(wo:vec3f,alpha:vec2f,u:vec2f)->vec3f {
  let a=max(alpha,vec2f(1e-4));let view=normalize(vec3f(a*wo.xy,wo.z));
  var t1=vec3f(1,0,0);if(view.z<.99999){t1=normalize(cross(vec3f(0,0,1),view));}let t2=cross(view,t1);
  let r=sqrt(u.x);let phi=2*PI*u.y;let x=r*cos(phi);var y=r*sin(phi);
  let s=.5*(1+view.z);y=(1-s)*sqrt(max(0.,1-x*x))+s*y;
  let h=x*t1+y*t2+sqrt(max(0.,1-x*x-y*y))*view;
  return normalize(vec3f(a*h.xy,max(1e-6,h.z)));
}
fn reflected(wo:vec3f,h:vec3f)->vec3f {return -wo+2*dot(wo,h)*h;}
fn transmitted(wo:vec3f,h:vec3f,eta:f32)->vec3f {
  let c=dot(wo,h);let s=(1-c*c)/(eta*eta);if(s>=1){return vec3f(0);}
  return -wo/eta+(c/eta-sqrt(max(0.,1-s)))*h;
}
fn cosine_direction(u:vec2f)->vec3f {let r=sqrt(u.x);let phi=2*PI*u.y;return vec3f(r*cos(phi),r*sin(phi),sqrt(max(0.,1-u.x)));}
fn bsdf_eval(m:Bsdf,wo:vec3f,wi:vec3f,eta:f32)->vec3f {
  if(wo.z<=0 || abs(wi.z)<1e-8){return vec3f(0);}
  if(m.kind==0u){return select(vec3f(0),m.color/PI,wi.z>0);}
  if(m.kind==2u && is_specular(m)){return vec3f(0);}
  if(m.kind==3u && (is_specular(m) || abs(eta-1)<1e-6)){return vec3f(0);}
  let reflection=wi.z>0;var h=wo+wi;
  if(!reflection){h=wo+eta*wi;}if(dot(h,h)<1e-20){return vec3f(0);}h=normalize(h);if(h.z<0){h=-h;}
  let oh=dot(wo,h);let ih=dot(wi,h);if(oh<=0 || ih*wi.z<=0){return vec3f(0);}
  let alpha=vec2f(m.ax,m.ay);let d=ggx_d(h,alpha);let g=ggx_g(wo,wi,alpha);
  if(m.kind==3u){
    let f=dielectric_fresnel(oh,eta);
    if(reflection){return vec3f(d*g*f/max(1e-20,4*wo.z*wi.z));}
    let denom=square(ih+oh/eta)*abs(wo.z*wi.z);
    return vec3f((1-f)*d*g*abs(ih*oh)/max(1e-30,denom*eta*eta));
  }
  if(!reflection){return vec3f(0);}
  if(m.kind==2u){return m.color*conductor_fresnel(oh,m)*(d*g/max(1e-20,4*wo.z*wi.z));}
  if(m.kind==1u){
    let coat=vec2f(m.k.x,m.k.y);let spec=schlick(oh)*(d*g+ggx_d(h,coat)*ggx_g(wo,wi,coat))/max(1e-20,4*wo.z*wi.z);
    return m.color/PI+vec3f(spec);
  }return vec3f(0);
}
fn reflection_pdf(wo:vec3f,h:vec3f,alpha:vec2f)->f32 {return visible_pdf(wo,h,alpha)/max(1e-20,4*abs(dot(wo,h)));}
fn bsdf_pdf(m:Bsdf,wo:vec3f,wi:vec3f,eta:f32)->f32 {
  if(wo.z<=0 || abs(wi.z)<1e-8){return 0;}
  if(m.kind==0u){return max(0.,wi.z)/PI;}
  if((m.kind==2u || m.kind==3u) && is_specular(m)){return 0;}
  if(m.kind==3u && abs(eta-1)<1e-6){return 0;}
  let reflection=wi.z>0;var h=wo+wi;if(!reflection){h=wo+eta*wi;}
  if(dot(h,h)<1e-20){return 0;}h=normalize(h);if(h.z<0){h=-h;}
  let oh=dot(wo,h);let ih=dot(wi,h);if(oh<=0 || ih*wi.z<=0){return 0;}
  let alpha=vec2f(m.ax,m.ay);
  if(m.kind==3u){let f=dielectric_fresnel(oh,eta);if(reflection){return f*reflection_pdf(wo,h,alpha);}return (1-f)*visible_pdf(wo,h,alpha)*abs(ih)/max(1e-30,square(ih+oh/eta));}
  if(!reflection){return 0;}
  if(m.kind==2u){return reflection_pdf(wo,h,alpha);}
  if(m.kind==1u){return .5*wi.z/PI+.25*reflection_pdf(wo,h,alpha)+.25*reflection_pdf(wo,h,vec2f(m.k.x,m.k.y));}return 0;
}
fn bsdf_sample(m:Bsdf,wo:vec3f,eta:f32,u:vec3f)->Scatter {
  if(wo.z<=0){return invalid_scatter();}
  if(m.kind==2u && is_specular(m)){return Scatter(vec3f(-wo.xy,wo.z),1,m.color*conductor_fresnel(wo.z,m),1u,1,0u);}
  if(m.kind==3u && (is_specular(m) || abs(eta-1)<1e-6)){
    let f=dielectric_fresnel(wo.z,eta);if(u.z<f){return Scatter(vec3f(-wo.xy,wo.z),f,vec3f(1),1u,1,0u);}
    let wi=transmitted(wo,vec3f(0,0,1),eta);if(dot(wi,wi)<.1){return invalid_scatter();}
    return Scatter(wi,1-f,vec3f(1/(eta*eta)),1u,eta,1u);
  }
  var wi=cosine_direction(u.xy);
  if(m.kind==2u || m.kind==3u || (m.kind==1u && u.z>=.5)){
    var alpha=vec2f(m.ax,m.ay);if(m.kind==1u && u.z>=.75){alpha=vec2f(m.k.x,m.k.y);}
    let h=visible_normal(wo,alpha,u.xy);
    if(m.kind==3u && u.z>=dielectric_fresnel(dot(wo,h),eta)){wi=transmitted(wo,h,eta);if(wi.z>=0){return invalid_scatter();}}
    else {wi=reflected(wo,h);if(wi.z<=0){return invalid_scatter();}}
  }
  let pdf=bsdf_pdf(m,wo,wi,eta);if(pdf<=0){return invalid_scatter();}
  let weight=bsdf_eval(m,wo,wi,eta)*(abs(wi.z)/pdf);let trans=select(0u,1u,wi.z<0);
  return Scatter(normalize(wi),pdf,weight,0u,select(1.,eta,trans!=0u),trans);
}
