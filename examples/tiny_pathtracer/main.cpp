// SPDX-License-Identifier: Apache-2.0
// A deliberately small SDK consumer: no scene syntax or resource parsing here.
#include <pbrt4/geometry.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <numbers>
#include <random>
#include <set>
#include <stdexcept>
using namespace pbrt4;
namespace {
constexpr double pi=std::numbers::pi, infinity=std::numeric_limits<double>::infinity();
struct Ray { Vec3 o,d; };
struct Triangle {
    Vec3 a,b,c,na,nb,nc,albedo,emission;
    double area=0;
    bool two_sided=false;
};
double component(Vec3 v,int i) { return i==0?v.x:i==1?v.y:v.z; }
struct Bounds {
    Vec3 lo{infinity,infinity,infinity},hi{-infinity,-infinity,-infinity};
    void add(Vec3 p) { lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)}; hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)}; }
    bool hit(Ray r,double limit) const {
        double first=1e-6,last=limit;
        for(int i=0;i<3;++i) {
            double d=component(r.d,i),o=component(r.o,i),a=component(lo,i),b=component(hi,i);
            if(d==0) { if(o<a || o>b) return false; continue; }
            double t0=(a-o)/d,t1=(b-o)/d; if(t0>t1) std::swap(t0,t1);
            first=std::max(first,t0); last=std::min(last,t1); if(first>last) return false;
        }
        return true;
    }
};
struct Node { Bounds bounds; int start=0,count=0,left=-1,right=-1; };
struct Hit { int triangle=-1; double distance=infinity,u=0,v=0; };
struct World {
    std::vector<Triangle> triangles;
    std::vector<int> order,emitters;
    std::vector<Node> nodes;
    Vec3 environment{};
    double light_area=0;
    int build(int start,int end) {
        int node=static_cast<int>(nodes.size()); nodes.push_back({});
        Bounds bounds,centers;
        for(int i=start;i<end;++i) { auto& t=triangles[order[i]]; bounds.add(t.a); bounds.add(t.b); bounds.add(t.c); centers.add((t.a+t.b+t.c)/3); }
        nodes[node].bounds=bounds;
        if(end-start<=4) { nodes[node].start=start; nodes[node].count=end-start; return node; }
        Vec3 extent=centers.hi-centers.lo; int axis=extent.x>extent.y ? 0 : 1; if(extent.z>component(extent,axis)) axis=2;
        int mid=(start+end)/2;
        std::nth_element(order.begin()+start,order.begin()+mid,order.begin()+end,[&](int a,int b) {
            auto& x=triangles[a]; auto& y=triangles[b]; return component(x.a+x.b+x.c,axis)<component(y.a+y.b+y.c,axis);
        });
        int left=build(start,mid),right=build(mid,end); nodes[node].left=left; nodes[node].right=right; return node;
    }
    void finish() {
        for(int i=0;i<static_cast<int>(triangles.size());++i) {
            order.push_back(i); auto& t=triangles[i];
            if(dot(t.emission,t.emission)>0 && t.area>0) { emitters.push_back(i); light_area+=t.area; }
        }
        if(!order.empty()) build(0,static_cast<int>(order.size()));
    }
    Hit hit(Ray r,double max_t=infinity) const {
        Hit h; h.distance=max_t; if(nodes.empty()) return h;
        std::vector<int> stack{0};
        while(!stack.empty()) {
            auto& n=nodes[stack.back()]; stack.pop_back(); if(!n.bounds.hit(r,h.distance)) continue;
            if(!n.count) { stack.push_back(n.left); stack.push_back(n.right); continue; }
            for(int i=n.start;i<n.start+n.count;++i) {
                int index=order[i]; auto& t=triangles[index]; Vec3 e1=t.b-t.a,e2=t.c-t.a,p=cross(r.d,e2);
                double det=dot(e1,p); if(std::abs(det)<1e-12) continue;
                Vec3 offset=r.o-t.a; double u=dot(offset,p)/det; if(u<0 || u>1) continue;
                Vec3 q=cross(offset,e1); double v=dot(r.d,q)/det; if(v<0 || u+v>1) continue;
                double distance=dot(e2,q)/det;
                if(distance>1e-6 && distance<h.distance) h={index,distance,u,v};
            }
        }
        return h;
    }
};
void require(bool b,const std::string& message) { if(!b) throw std::runtime_error(message); }
// Unsupported renderer inputs stop with source context. They remain intact in the SDK IR.
void supported(const Entity& e,const std::set<std::string>& names) {
    for(const auto* p:{&e.parameters,&e.inherited_parameters}) for(const auto& v:*p)
        require(names.contains(v.name),format({Severity::error,"tiny.unsupported","Unsupported "+e.type+" parameter: "+v.name,v.location}));
    require(e.references.empty() || std::all_of(e.references.begin(),e.references.end(),[](auto& r){return r.name.empty();}),"Tiny renderer does not evaluate textures or media");
    require(!e.transform.animated(),"Tiny renderer does not sample animated transforms");
}
Vec3 rgb(const Entity& e,std::string_view name,Vec3 fallback) {
    auto p=find_parameter(e,name); if(!p) return fallback;
    require(p->type=="rgb" && p->values.size()==3,"Tiny renderer requires rgb "+std::string(name)+"; spectral values remain in Scene IR");
    require(p->color_space.empty() || p->color_space=="srgb","Tiny renderer only evaluates linear sRGB color values");
    Vec3 c{std::get<double>(p->values[0]),std::get<double>(p->values[1]),std::get<double>(p->values[2])};
    require(c.x>=0 && c.y>=0 && c.z>=0,"Negative RGB input"); return c;
}
World make_world(const Scene& scene) {
    GeometryOptions options; options.tessellate_spheres=true;
    auto geometry=extract_geometry(scene,options);
    for(auto& d:geometry.diagnostics) std::cerr<<format(d)<<'\n';
    require(bool(geometry),"Geometry extraction failed"); World world;
    for(const auto& light:scene.lights) {
        require(light.type=="infinite","Tiny renderer supports constant infinite and diffuse area lights"); supported(light,{"L","scale"});
        world.environment=world.environment+rgb(light,"L",{1,1,1})*scalar(light,"scale").value_or(1);
    }
    for(const auto& instance:geometry.instances) {
        auto& shape=scene.shapes.at(instance.source_shape);
        require(shape.inside_medium.empty() && shape.outside_medium.empty(),"Tiny renderer does not support participating media");
        supported(shape,{"P","N","uv","indices","filename","radius","zmin","zmax","phimax"});
        const auto& material=scene.materials.at(instance.material);
        require(material.type=="diffuse","Unsupported tiny renderer material: "+material.type);
        supported(material,{"type","reflectance"}); Vec3 albedo=rgb(material,"reflectance",{.5,.5,.5});
        require(std::max({albedo.x,albedo.y,albedo.z})<=1,"Diffuse reflectance exceeds one");
        Vec3 emission{}; bool two_sided=false;
        if(instance.area_light!=invalid_id) {
            const auto& area=scene.area_lights.at(instance.area_light); require(area.type=="diffuse","Unsupported area light: "+area.type);
            supported(area,{"L","scale","twosided"}); emission=rgb(area,"L",{1,1,1})*scalar(area,"scale").value_or(1);
            if(auto p=find_parameter(area,"twosided")) { require(p->type=="bool" && p->values.size()==1,"twosided requires one bool"); two_sided=std::get<bool>(p->values[0]); }
        }
        auto baked=bake_instance(geometry.meshes.at(instance.mesh),instance);
        for(auto& d:baked.diagnostics) std::cerr<<format(d)<<'\n'; require(bool(baked),"Static geometry baking failed");
        auto& mesh=baked.mesh;
        for(std::size_t i=0;i<mesh.indices.size();i+=3) {
            auto ia=mesh.indices[i],ib=mesh.indices[i+1],ic=mesh.indices[i+2];
            Vec3 a=mesh.positions[ia],b=mesh.positions[ib],c=mesh.positions[ic],g=cross(b-a,c-a); double area=.5*std::sqrt(dot(g,g));
            if(area<1e-14) continue;
            Vec3 n=normalize(g),na=n,nb=n,nc=n;
            if(!mesh.normals.empty()) { na=mesh.normals[ia]; nb=mesh.normals[ib]; nc=mesh.normals[ic]; }
            world.triangles.push_back({a,b,c,na,nb,nc,albedo,emission,area,two_sided});
        }
    }
    world.finish(); return world;
}
struct Random {
    std::mt19937 engine{12345};
    double operator()() { return std::generate_canonical<double,53>(engine); }
};
Vec3 cosine_direction(Vec3 n,Random& random) {
    double radius=std::sqrt(random()),angle=2*pi*random();
    Vec3 tangent=normalize(cross(std::abs(n.x)>.5 ? Vec3{0,1,0}:Vec3{1,0,0},n)),bitangent=cross(n,tangent);
    return normalize(tangent*(radius*std::cos(angle))+bitangent*(radius*std::sin(angle))+n*std::sqrt(1-radius*radius));
}
Vec3 trace(const World& world,Ray ray,Random& random,int max_depth) {
    Vec3 result{},throughput{1,1,1};
    for(int depth=0;depth<max_depth;++depth) {
        auto hit=world.hit(ray);
        if(hit.triangle<0) { result=result+throughput*world.environment; break; }
        auto& t=world.triangles[hit.triangle]; Vec3 p=ray.o+ray.d*hit.distance,geometric=normalize(cross(t.b-t.a,t.c-t.a));
        bool front=dot(geometric,ray.d)<0;
        if(depth==0 && (front || t.two_sided)) result=result+throughput*t.emission;
        Vec3 n=normalize(t.na*(1-hit.u-hit.v)+t.nb*hit.u+t.nc*hit.v);
        if(dot(n,geometric)<0) n=-n;
        if(dot(n,ray.d)>0) n=-n;
        Vec3 offset=p+(front?geometric:-geometric)*1e-5;
        // Sample a point uniformly over total emitter area. Continuation emission is
        // excluded after diffuse bounces to avoid double counting this estimator.
        if(!world.emitters.empty()) {
            double pick=random()*world.light_area; int chosen=world.emitters.back();
            for(int emitter:world.emitters) { pick-=world.triangles[emitter].area; if(pick<=0) { chosen=emitter; break; } }
            auto& light=world.triangles[chosen]; double u=std::sqrt(random()),v=random();
            Vec3 point=light.a*(1-u)+light.b*(u*(1-v))+light.c*(u*v),delta=point-p;
            double distance2=dot(delta,delta); Vec3 direction=normalize(delta),ln=normalize(cross(light.b-light.a,light.c-light.a));
            double surface=std::max(0.,dot(n,direction)),emitter=dot(ln,-direction);
            emitter=light.two_sided ? std::abs(emitter) : std::max(0.,emitter);
            if(surface>0 && emitter>0 && distance2>1e-10) {
                auto shadow=world.hit({offset,normalize(point-offset)},std::sqrt(dot(point-offset,point-offset))-2e-5);
                if(shadow.triangle<0) result=result+throughput*t.albedo*light.emission*(surface*emitter*world.light_area/(pi*distance2));
            }
        }
        throughput=throughput*t.albedo;
        if(depth>=3) {
            double survive=std::clamp(std::max({throughput.x,throughput.y,throughput.z}),.05,.95);
            if(random()>survive) break; throughput=throughput/survive;
        }
        ray={offset,cosine_direction(n,random)};
    }
    return result;
}
int integer_setting(const Entity& e,std::string_view name,int fallback,int limit) {
    double v=scalar(e,name).value_or(fallback); require(v>=1 && v<=limit && v==std::floor(v),"Invalid render setting: "+std::string(name)); return static_cast<int>(v);
}
}
int main(int argc,char** argv) {
    try {
        if(argc<3 || argc>4) { std::cerr<<"Usage: tiny_pathtracer scene.pbrt output.ppm [samples-per-pixel]\n"; return 2; }
        auto loaded=load(argv[1]); for(auto& d:loaded.diagnostics) std::cerr<<format(d)<<'\n'; require(bool(loaded),"Scene load failed"); auto& scene=loaded.scene;
        require(scene.camera.type=="perspective","Tiny renderer requires perspective camera");
        supported(scene.camera,{"fov"}); supported(scene.film,{"xresolution","yresolution","filename"});
        supported(scene.sampler,{"pixelsamples"}); supported(scene.integrator,{"maxdepth"});
        supported(scene.accelerator,{});
        require(scene.accelerator.type=="bvh","Tiny renderer uses its own BVH accelerator");
        require(scene.integrator.type=="path" || scene.integrator.type=="volpath","Tiny renderer requires path or default volpath settings without media");
        require(scene.options.empty(),"Tiny renderer does not apply Option directives");
        require(scene.pixel_filter.parameters.empty(),"Tiny renderer uses a box pixel filter");
        std::cerr<<"Preview uses cosine Lambertian sampling, median BVH, independent RNG, box filter and linear sRGB; PBRT sampler/integrator/filter algorithms are not reproduced.\n";
        int width=integer_setting(scene.film,"xresolution",128,4096),height=integer_setting(scene.film,"yresolution",128,4096);
        int samples=argc==4 ? std::stoi(argv[3]) : integer_setting(scene.sampler,"pixelsamples",32,100000),depth=integer_setting(scene.integrator,"maxdepth",8,64);
        require(samples>0 && samples<=100000,"Invalid sample count");
        double fov=scalar(scene.camera,"fov").value_or(90); require(fov>0 && fov<179,"Invalid field of view");
        double scale=std::tan(fov*pi/360),aspect=double(width)/height;
        auto world=make_world(scene); require(!world.triangles.empty(),"No renderable geometry");
        std::cerr<<world.triangles.size()<<" triangles, "<<world.nodes.size()<<" BVH nodes, "<<samples<<" spp\n";
        std::ofstream out(argv[2],std::ios::binary); require(bool(out),"Cannot create output image"); out<<"P6\n"<<width<<' '<<height<<"\n255\n";
        Random random; const auto& camera=scene.camera.transform.start;
        for(int y=0;y<height;++y) {
            for(int x=0;x<width;++x) {
                Vec3 color{};
                for(int sample=0;sample<samples;++sample) {
                    double sx=(2*(x+random())/width-1)*scale*(aspect>1?aspect:1),sy=(1-2*(y+random())/height)*scale*(aspect<1?1/aspect:1);
                    Ray ray{transform_point(camera,{}),normalize(transform_vector(camera,{sx,sy,1}))}; color=color+trace(world,ray,random,depth);
                }
                color=color/samples;
                for(double channel:{color.x,color.y,color.z}) { auto byte=static_cast<unsigned char>(255*std::pow(std::clamp(channel,0.,1.),1/2.2)+.5); out.put(static_cast<char>(byte)); }
            }
            if(y%16==0) std::cerr<<"row "<<y<<'/'<<height<<'\n';
        }
        require(bool(out),"Failed writing output image"); std::cerr<<"Wrote "<<argv[2]<<'\n';
    } catch(const std::exception& e) { std::cerr<<"tiny_pathtracer: "<<e.what()<<'\n'; return 1; }
}
