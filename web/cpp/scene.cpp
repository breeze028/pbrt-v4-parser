// SPDX-License-Identifier: Apache-2.0
#include "scene.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace pbrt4::web {
namespace {
std::string quote(std::string_view s) {
    std::ostringstream o; o<<'"'; for(unsigned char c:s) {
        if(c=='"' || c=='\\') o<<'\\'<<char(c);
        else if(c<32) o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c)<<std::dec;
        else o<<char(c);
    } o<<'"'; return o.str();
}
std::string params(const std::vector<Parameter>& p) {
    std::ostringstream o; o<<std::setprecision(17)<<'['; bool first=true;
    for(auto& x:p) {
        if(!first) o<<','; first=false;
        o<<"{\"name\":"<<quote(x.name)<<",\"type\":"<<quote(x.type)<<",\"colorSpace\":"<<quote(x.color_space)<<",\"values\":[";
        bool vfirst=true;
        for(auto& v:x.values) { if(!vfirst) o<<','; vfirst=false;
            std::visit([&](const auto& value) { using T=std::decay_t<decltype(value)>;
                if constexpr(std::is_same_v<T,std::string>) o<<quote(value);
                else if constexpr(std::is_same_v<T,bool>) o<<(value?"true":"false"); else o<<value;
            },v);
        } o<<"]}";
    } o<<']'; return o.str();
}
std::string matrix(const Matrix4& m) { std::ostringstream o; o<<std::setprecision(17)<<'['; for(int i=0;i<16;++i) {if(i) o<<','; o<<m.m[i];} o<<']'; return o.str(); }
std::string entity(const Entity& e) {
    auto result="{\"name\":"+quote(e.name)+",\"type\":"+quote(e.type)+",\"valueType\":"+quote(e.value_type)+",\"colorSpace\":"+quote(e.color_space)+",\"scope\":"+std::to_string(e.scope)+",\"source\":"+quote(e.location.filename)+",\"line\":"+std::to_string(e.location.line)+",\"parameters\":"+params(e.parameters)+",\"inheritedParameters\":"+params(e.inherited_parameters)+",\"transform\":"+matrix(e.transform.start)+",\"endTransform\":"+matrix(e.transform.end)+",\"references\":[";
    bool first=true;for(auto& ref:e.references){if(!first)result+=',';first=false;result+="{\"parameter\":"+quote(ref.parameter)+",\"name\":"+quote(ref.name)+",\"kind\":"+std::to_string(static_cast<int>(ref.kind))+",\"id\":"+std::to_string(ref.id==invalid_id?none:ref.id)+"}";}
    return result+"]}";
}
std::string entity(const Shape& e) {
    auto result=entity(static_cast<const Entity&>(e));result.pop_back();
    return result+",\"material\":"+std::to_string(e.material)+",\"areaLight\":"+std::to_string(e.area_light==invalid_id?none:e.area_light)+",\"reverseOrientation\":"+(e.reverse_orientation?"true":"false")+",\"insideMedium\":"+quote(e.inside_medium)+",\"outsideMedium\":"+quote(e.outside_medium)+"}";
}
Vec3 color(const Entity& e,std::string_view n,Vec3 fallback) {
    auto p=find_parameter(e,n); if(!p) return fallback;
    if(p->type=="rgb" && p->values.size()==3) return {std::get<double>(p->values[0]),std::get<double>(p->values[1]),std::get<double>(p->values[2])};
    return fallback;
}
float f32(double d) { if(!std::isfinite(d) || std::abs(d)>std::numeric_limits<float>::max()) throw std::runtime_error("Value cannot be represented as finite GPU f32"); return static_cast<float>(d); }
std::uint32_t id(Id i) { if(i==invalid_id) return none; if(i>=none) throw std::runtime_error("GPU ID overflow"); return static_cast<std::uint32_t>(i); }
void mcopy(float* out,const Matrix4& m) { for(int i=0;i<16;++i) out[i]=f32(m.m[i]); }
struct Box {
    Vec3 lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};
    void add(Vec3 p) { lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)}; hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)}; }
    void add(const Box& b) { add(b.lo); add(b.hi); }
};
Vec3 pos(const Vertex& v) { return {v.position[0],v.position[1],v.position[2]}; }
double axis(Vec3 v,int a) {return a==0?v.x:a==1?v.y:v.z;}
struct Tree {
    RenderScene& out;
    const std::vector<Box>& boxes;
    std::vector<std::uint32_t> order;
    void node(std::uint32_t index,std::uint32_t begin,std::uint32_t end,unsigned depth) {
        out.max_depth=std::max(out.max_depth,depth);
        if(depth>60) throw std::runtime_error("BVH exceeds GPU traversal stack contract");
        Box b,centers;
        for(auto k=begin;k<end;++k) { b.add(boxes[order[k]]); centers.add((boxes[order[k]].lo+boxes[order[k]].hi)*.5); }
        Node n; for(int a=0;a<3;++a) { n.lo[a]=std::nextafter(f32(axis(b.lo,a)),-INFINITY); n.hi[a]=std::nextafter(f32(axis(b.hi,a)),INFINITY); }
        if(end-begin<=4) { n.left=begin; n.count=end-begin; out.nodes[index]=n; return; }
        Vec3 ext=centers.hi-centers.lo; int a=ext.x>ext.y?0:1; if(ext.z>axis(ext,a)) a=2;
        // Total ordering makes native libc++/MSVC and WASM build identical packs.
        std::sort(order.begin()+begin,order.begin()+end,[&](auto x,auto y) {
            double cx=axis(boxes[x].lo+boxes[x].hi,a),cy=axis(boxes[y].lo+boxes[y].hi,a); return cx==cy?x<y:cx<cy;
        });
        n.left=id(out.nodes.size()); out.nodes.resize(out.nodes.size()+2); out.nodes[index]=n;
        auto mid=begin+(end-begin)/2; node(n.left,begin,mid,depth+1); node(n.left+1,mid,end,depth+1);
    }
    std::uint32_t build() {
        if(boxes.empty()) return none;
        order.resize(boxes.size()); std::iota(order.begin(),order.end(),0u);
        auto root=id(out.nodes.size()); out.nodes.emplace_back(); node(root,0,id(boxes.size()),0); return root;
    }
};
void diagnostic(RenderScene& r,const Entity& e,std::string code,std::string message,Severity severity=Severity::warning) {
    r.diagnostics.push_back({severity,std::move(code),std::move(message),e.location}); r.renderable=false;
}
void supported_parameters(RenderScene& r,const Entity& e,std::initializer_list<std::string_view> names) {
    for(auto* ps:{&e.parameters,&e.inherited_parameters}) for(auto& p:*ps)
        if(std::find(names.begin(),names.end(),p.name)==names.end())
            diagnostic(r,e,"web.parameter.pending","Parameter not evaluated by this renderer: "+e.type+"."+p.name);
}
void metadata(RenderScene& r,const Scene& s) {
    std::ostringstream o; o<<std::setprecision(17);
    o<<"{\"layoutVersion\":"<<layout_version<<",\"renderable\":"<<(r.renderable && !has_errors(r.diagnostics)?"true":"false")
     <<",\"tlasRoot\":"<<r.tlas_root<<",\"maxBvhDepth\":"<<r.max_depth<<",\"counts\":{\"shapes\":"<<s.shapes.size()<<",\"objects\":"<<s.objects.size()<<",\"instances\":"<<s.instances.size()<<",\"vertices\":"<<r.vertices.size()<<",\"triangles\":"<<r.primitives.size()<<",\"placements\":"<<r.instances.size()<<",\"nodes\":"<<r.nodes.size()<<",\"emitters\":"<<r.emitters.size()<<"},\"camera\":"<<entity(s.camera)
     <<",\"film\":"<<entity(s.film)<<",\"sampler\":"<<entity(s.sampler)<<",\"integrator\":"<<entity(s.integrator)<<",\"pixelFilter\":"<<entity(s.pixel_filter)<<",\"accelerator\":"<<entity(s.accelerator)<<",\"fov\":"<<scalar(s.camera,"fov").value_or(90)<<",\"width\":"<<scalar(s.film,"xresolution").value_or(128)<<",\"height\":"<<scalar(s.film,"yresolution").value_or(128)
     <<",\"maxDepth\":"<<scalar(s.integrator,"maxdepth").value_or(8)<<",\"filter\":"<<quote(s.pixel_filter.type)<<",\"filterRadius\":["<<scalar(s.pixel_filter,"xradius").value_or(s.pixel_filter.type=="box"?.5:2)<<','<<scalar(s.pixel_filter,"yradius").value_or(s.pixel_filter.type=="box"?.5:2)<<']';
    auto list=[&](const char* name,const auto& items) { o<<','<<quote(name)<<":["; bool first=true; for(auto& e:items) {if(!first)o<<',';first=false;o<<entity(e);}o<<']'; };
    list("shapes",s.shapes); list("materials",s.materials); list("textures",s.textures); list("lights",s.lights); list("areaLights",s.area_lights); list("media",s.media);
    o<<",\"objects\":["; bool first=true;
    for(auto& e:s.objects) { if(!first)o<<',';first=false; o<<"{\"name\":"<<quote(e.name)<<",\"shapes\":["; for(Id j=0;j<e.shapes.size();++j){if(j)o<<',';o<<e.shapes[j];}o<<"]}"; }
    o<<"],\"instances\":["; first=true; for(auto& e:s.instances) {if(!first)o<<',';first=false;o<<"{\"object\":"<<e.object<<",\"transform\":"<<matrix(e.transform.start)<<",\"endTransform\":"<<matrix(e.transform.end)<<'}';}
    o<<"],\"diagnostics\":["; first=true; for(auto& d:r.diagnostics) {if(!first)o<<',';first=false; o<<"{\"severity\":"<<quote(d.severity==Severity::error?"error":"warning")<<",\"code\":"<<quote(d.code)<<",\"message\":"<<quote(d.message)<<",\"source\":"<<quote(d.location.filename)<<",\"line\":"<<d.location.line<<'}';} o<<"]}";
    r.metadata=o.str();
}
template<class T> void append(std::vector<std::uint32_t>& dst,const std::vector<T>& v) {
    static_assert(sizeof(T)%4==0); auto offset=dst.size(); dst.resize(offset+v.size()*sizeof(T)/4); if(!v.empty()) std::memcpy(dst.data()+offset,v.data(),v.size()*sizeof(T));
}
}
RenderScene build(const Scene& s,const LoadOptions& resources) {
    RenderScene r;
    try {
        GeometryOptions opts; opts.resources=resources; auto g=extract_geometry(s,opts); r.diagnostics=g.diagnostics;
        if(!g) {r.renderable=false;metadata(r,s);return r;}
        for(auto& e:s.materials) {
            if(e.type=="diffuse")supported_parameters(r,e,{"type","reflectance"});
            Material m; auto c=color(e,"reflectance",{.5,.5,.5}); for(int a=0;a<3;++a)m.color[a]=f32(axis(c,a));
            m.color[3]=1; m.info[0]=e.type=="diffuse"?0:e.type=="coateddiffuse"?1:e.type=="conductor"?2:e.type=="dielectric"?3:none;
            m.eta_roughness[3]=f32(scalar(e,"roughness").value_or(scalar(e,"uroughness").value_or(.5))); m.k_ior[3]=f32(scalar(e,"eta").value_or(1.5));
            if(e.type!="diffuse") diagnostic(r,e,"web.material.pending","WGSL material implementation pending: "+e.type);
            for(auto* ps:{&e.parameters,&e.inherited_parameters}) for(auto& p:*ps) if(p.type=="texture" || p.type=="spectrum" || p.type=="blackbody") diagnostic(r,e,"web.parameter.pending","Parameter evaluation pending: "+p.type+" "+p.name);
            r.materials.push_back(m);
        }
        if(s.camera.type!="perspective" || s.camera.transform.animated()) diagnostic(r,s.camera,"web.camera.pending","Only static perspective camera is rendered",Severity::error);
        supported_parameters(r,s.camera,{"fov"});
        const auto fov=scalar(s.camera,"fov").value_or(90);
        if(!(fov>0 && fov<180))diagnostic(r,s.camera,"web.camera.invalid","Expected fov between 0 and 180",Severity::error);
        if(s.film.type!="rgb")diagnostic(r,s.film,"web.film.pending","Only RGB film is rendered");
        supported_parameters(r,s.film,{"filename","xresolution","yresolution"});
        for(auto name:{"xresolution","yresolution"}) if(auto v=scalar(s.film,name);v && (*v<=0 || std::floor(*v)!=*v))diagnostic(r,s.film,"web.film.invalid","Expected positive integer film dimensions",Severity::error);
        if(s.integrator.type!="path")diagnostic(r,s.integrator,"web.integrator.pending","This renderer implements its own surface path integrator; requested: "+s.integrator.type);
        supported_parameters(r,s.integrator,{"maxdepth"});
        if(auto depth=scalar(s.integrator,"maxdepth");depth && (*depth<1 || *depth>1024 || std::floor(*depth)!=*depth))diagnostic(r,s.integrator,"web.depth.invalid","Supported maxdepth is an integer in [1,1024]",Severity::error);
        if(s.pixel_filter.type!="triangle" && s.pixel_filter.type!="box")diagnostic(r,s.pixel_filter,"web.filter.pending","Pixel filter not implemented: "+s.pixel_filter.type);
        supported_parameters(r,s.pixel_filter,{"xradius","yradius"});
        for(auto name:{"xradius","yradius"})if(auto v=scalar(s.pixel_filter,name);v && *v<=0)diagnostic(r,s.pixel_filter,"web.filter.invalid","Expected positive filter radius",Severity::error);
        for(auto& e:s.area_lights){
            if(e.type!="diffuse")diagnostic(r,e,"web.areaLight.pending","Area light not implemented: "+e.type);
            supported_parameters(r,e,{"L","scale","twosided"});
            if(auto p=find_parameter(e,"L");p && p->type!="rgb")diagnostic(r,e,"web.emission.pending","Only RGB area light emission is implemented");
        }
        for(auto& light:s.lights) diagnostic(r,light,"web.light.pending","WGSL light implementation pending: "+light.type);
        for(auto& e:s.media) diagnostic(r,e,"web.medium.pending","Medium rendering is pending");
        std::vector<std::uint32_t> roots; std::vector<Box> mesh_bounds;
        for(auto& mesh:g.meshes) {
            auto base=id(r.vertices.size());
            for(Id i=0;i<mesh.positions.size();++i) {
                Vertex v; for(int a=0;a<3;++a)v.position[a]=f32(axis(mesh.positions[i],a)); v.position[3]=1;
                if(!mesh.normals.empty()) {for(int a=0;a<3;++a)v.normal[a]=f32(axis(mesh.normals[i],a));v.normal[3]=1;}
                if(!mesh.texcoords.empty()) {v.uv[0]=f32(mesh.texcoords[i].x);v.uv[1]=f32(mesh.texcoords[i].y);v.uv[2]=1;}
                r.vertices.push_back(v);
            }
            std::vector<Primitive> local; std::vector<Box> boxes;
            for(Id j=0;j<mesh.indices.size();j+=3) {
                Primitive p; Box b; for(int a=0;a<3;++a) {p.vertices[a]=base+mesh.indices[j+a];b.add(pos(r.vertices[p.vertices[a]]));} p.shape=id(mesh.source_shape); local.push_back(p); boxes.push_back(b);
            }
            Tree tree{r,boxes,{}}; auto root=tree.build(); roots.push_back(root); Box bounds;
            for(auto& b:boxes)bounds.add(b); mesh_bounds.push_back(bounds);
            auto first=id(r.primitives.size());
            if(root!=none) for(Id n=root;n<r.nodes.size();++n) if(r.nodes[n].count)r.nodes[n].left+=first;
            for(auto k:tree.order)r.primitives.push_back(local[k]);
        }
        std::vector<Box> instance_bounds;
        for(auto& i:g.instances) {
            auto& shape=s.shapes.at(i.source_shape);
            if(i.transform.animated()) {diagnostic(r,shape,"web.animation.pending","Animated placement rendering pending",Severity::error);continue;}
            auto inv=inverse(i.transform.start);
            if(!inv || i.transform.start.m[12]!=0 || i.transform.start.m[13]!=0 || i.transform.start.m[14]!=0 || i.transform.start.m[15]!=1) {diagnostic(r,shape,"web.transform.invalid","Expected invertible affine placement",Severity::error);continue;}
            Instance placed; mcopy(placed.world,i.transform.start);mcopy(placed.local,*inv);
            placed.info[0]=roots.at(i.mesh);placed.info[1]=id(i.material);placed.info[2]=id(i.source_shape);placed.info[3]=(i.reverse_orientation?1u:0u)|(determinant3(i.transform.start)<0?2u:0u);
            if(i.area_light!=invalid_id) {auto& e=s.area_lights.at(i.area_light); auto emission=color(e,"L",{1,1,1})*scalar(e,"scale").value_or(1);for(int a=0;a<3;++a)placed.emission[a]=f32(axis(emission,a)); if(auto p=find_parameter(e,"twosided"))placed.emission[3]=std::get<bool>(p->values.at(0))?1.f:0.f;}
            if(find_parameter(shape,"alpha")) diagnostic(r,shape,"web.alpha.pending","Shape alpha rendering pending");
            if(!shape.inside_medium.empty() || !shape.outside_medium.empty())diagnostic(r,shape,"web.medium.pending","Shape medium rendering pending");
            Box b; auto& mb=mesh_bounds.at(i.mesh);
            for(int c=0;c<8;++c)b.add(transform_point(i.transform.start,{c&1?mb.hi.x:mb.lo.x,c&2?mb.hi.y:mb.lo.y,c&4?mb.hi.z:mb.lo.z}));
            instance_bounds.push_back(b);r.instances.push_back(placed);
        }
        Tree tlas{r,instance_bounds,{}};r.tlas_root=tlas.build(); auto unsorted=std::move(r.instances);for(auto k:tlas.order)r.instances.push_back(unsorted[k]);
        for(std::uint32_t i=0;i<r.instances.size();++i) {
            auto& instance=r.instances[i]; if(instance.emission[0]+instance.emission[1]+instance.emission[2]<=0)continue;
            for(std::uint32_t p=0;p<r.primitives.size();++p)if(r.primitives[p].shape==instance.info[2])r.emitters.push_back({i,p,{0,0}});
        }
        r.records.resize(16);r.records[0]=layout_version;r.records[1]=r.tlas_root;
        r.records[2]=id(r.records.size());r.records[3]=id(r.materials.size());append(r.records,r.materials);
        r.records[4]=id(r.records.size());r.records[5]=id(r.instances.size());append(r.records,r.instances);
        r.records[6]=id(r.records.size());r.records[7]=id(r.emitters.size());append(r.records,r.emitters);
    } catch(const std::exception& e) {r.renderable=false;r.diagnostics.push_back({Severity::error,"web.pack",e.what(),{}});}
    metadata(r,s);return r;
}
RenderScene open(const std::filesystem::path& p) {
    auto source=load(p); if(!source) {RenderScene r;r.renderable=false;r.diagnostics=source.diagnostics;metadata(r,source.scene);return r;}
    auto r=build(source.scene);r.diagnostics.insert(r.diagnostics.begin(),source.diagnostics.begin(),source.diagnostics.end());metadata(r,source.scene);return r;
}
const void* data(const RenderScene& r,unsigned k) {switch(k){case 0:return r.vertices.data();case 1:return r.primitives.data();case 2:return r.nodes.data();case 3:return r.records.data();default:return nullptr;}}
std::size_t bytes(const RenderScene& r,unsigned k) {switch(k){case 0:return r.vertices.size()*sizeof(Vertex);case 1:return r.primitives.size()*sizeof(Primitive);case 2:return r.nodes.size()*sizeof(Node);case 3:return r.records.size()*4;default:return 0;}}
}
