// SPDX-License-Identifier: Apache-2.0
#include "scene.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <map>
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
    bool first=true;for(auto& ref:e.references){if(!first)result+=',';first=false;result+="{\"parameter\":"+quote(ref.parameter)+",\"name\":"+quote(ref.name)+",\"kind\":"+std::to_string(static_cast<int>(ref.kind))+",\"id\":"+std::to_string(ref.id==invalid_id?none:ref.id)+",\"alternateId\":"+std::to_string(ref.alternate_id==invalid_id?none:ref.alternate_id)+"}";}
    result+="],\"resources\":[";first=true;for(auto& resource:e.resources){if(!first)result+=',';first=false;result+="{\"parameter\":"+quote(resource.parameter)+",\"literal\":"+quote(resource.literal)+",\"resolved\":"+quote(path_to_utf8(resource.resolved))+"}";}
    return result+"],\"startTime\":"+std::to_string(e.transform.start_time)+",\"endTime\":"+std::to_string(e.transform.end_time)+"}";
}
std::string entity(const Shape& e) {
    auto result=entity(static_cast<const Entity&>(e));result.pop_back();
    return result+",\"material\":"+std::to_string(e.material)+",\"materialName\":"+quote(e.material_name)+",\"areaLight\":"+std::to_string(e.area_light==invalid_id?none:e.area_light)+",\"reverseOrientation\":"+(e.reverse_orientation?"true":"false")+",\"insideMedium\":"+quote(e.inside_medium)+",\"outsideMedium\":"+quote(e.outside_medium)+",\"insideMediumId\":"+std::to_string(e.inside_medium_id==invalid_id?none:e.inside_medium_id)+",\"outsideMediumId\":"+std::to_string(e.outside_medium_id==invalid_id?none:e.outside_medium_id)+"}";
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
bool boolean(const Entity& e,std::string_view name,bool fallback){auto p=find_parameter(e,name);return p&&!p->values.empty()?std::get<bool>(p->values[0]):fallback;}
Vec3 optical_color(RenderScene& r,const Entity& e,std::string_view name,Vec3 fallback){
    if(auto p=find_parameter(e,name);p && p->type=="spectrum"){
        const auto n=string_value(e,name).value_or("");
        if(n=="metal-Ag-eta")return {.139410,.124777,.151691};
        if(n=="metal-Ag-k")return {4.128773,3.339600,2.470420};
        diagnostic(r,e,"web.spectrum.pending","No RGB approximation for spectrum: "+n);
    }
    return color(e,name,fallback);
}
std::uint32_t texture_ref(RenderScene& r,const Scene& s,const Entity& e,std::string_view parameter,std::string_view type){
    const auto p=find_parameter(e,parameter);if(!p || p->type!="texture")return none;
    for(auto& ref:e.references)if(ref.kind==ReferenceKind::texture && ref.parameter==parameter)
        for(auto i:{ref.id,ref.alternate_id})if(i<s.textures.size() && s.textures[i].value_type==type)return static_cast<std::uint32_t>(i);
    diagnostic(r,e,"web.texture.reference","Expected a resolved "+std::string(type)+" texture for "+std::string(parameter),Severity::error);return none;
}
void images(RenderScene& r,const Scene& s,const LoadOptions& resources){
    std::map<std::string,std::uint32_t> slots;
    for(auto& e:s.textures){
        TextureRecord record;record.info[0]=none;
        supported_parameters(r,e,{"filename","filter","encoding","mapping","uscale","vscale","udelta","vdelta"});
        if(e.type!="imagemap" || string_value(e,"mapping").value_or("uv")!="uv" || string_value(e,"filter").value_or("trilinear")!="trilinear"){
            diagnostic(r,e,"web.texture.pending","Only trilinear UV imagemaps are implemented");r.textures.push_back(record);continue;
        }
        const auto literal=string_value(e,"filename").value_or("");const auto encoding=string_value(e,"encoding").value_or(e.value_type=="spectrum"?"srgb":"linear");
        if(encoding!="srgb" && encoding!="linear"){diagnostic(r,e,"web.encoding.pending","Unsupported image encoding: "+encoding);r.textures.push_back(record);continue;}
        auto path=(s.base_directory/path_from_utf8(literal)).lexically_normal();
        for(auto& resource:e.resources)if(resource.parameter=="filename")path=resource.resolved;
        const auto key=path_to_utf8(path)+"|"+e.value_type+"|"+encoding;
        auto found=slots.find(key);std::uint32_t slot;
        if(found==slots.end()){
            if(r.images.size()>=12)throw std::runtime_error("Scene exceeds the portable 12 image slots");
            std::string bytes;
            if(resources.read_file){auto result=resources.read_file(path);if(!result)throw std::runtime_error("Cannot read image: "+literal);bytes=std::move(*result);}
            else {std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Cannot read image: "+literal);bytes.assign(std::istreambuf_iterator<char>(input),{});}
            if(bytes.size()>resources.max_file_bytes)throw std::runtime_error("Image exceeds resource byte limit");
            if(path.extension()!=".tga" && path.extension()!=".TGA")throw std::runtime_error("This scene consumer currently decodes TGA images only: "+literal);
            auto decoded=decode_tga({reinterpret_cast<const std::uint8_t*>(bytes.data()),bytes.size()});
            auto mips=make_mips(std::move(decoded),encoding=="srgb",e.value_type=="float");
            Image image;image.path=literal;image.role=e.value_type;image.srgb=encoding=="srgb" && e.value_type!="float";image.levels=std::move(mips.levels);
            for(auto& level:image.levels)level.offset+=static_cast<std::uint32_t>(r.image_pixels.size());
            r.image_pixels.insert(r.image_pixels.end(),mips.bytes.begin(),mips.bytes.end());
            slot=static_cast<std::uint32_t>(r.images.size());r.images.push_back(std::move(image));slots[key]=slot;
        }else slot=found->second;
        const auto& image=r.images[slot];record.info[0]=slot;record.info[1]=image.levels[0].width;record.info[2]=image.levels[0].height;record.info[3]=static_cast<std::uint32_t>(image.levels.size());
        record.uv[0]=f32(scalar(e,"uscale").value_or(1));record.uv[1]=f32(scalar(e,"vscale").value_or(1));record.uv[2]=f32(scalar(e,"udelta").value_or(0));record.uv[3]=f32(scalar(e,"vdelta").value_or(0));
        r.textures.push_back(record);
    }
}
void metadata(RenderScene& r,const Scene& s) {
    std::ostringstream o; o<<std::setprecision(17);
    o<<"{\"layoutVersion\":"<<layout_version<<",\"renderable\":"<<(r.renderable && !has_errors(r.diagnostics)?"true":"false")
     <<",\"tlasRoot\":"<<r.tlas_root<<",\"maxBvhDepth\":"<<r.max_depth<<",\"counts\":{\"shapes\":"<<s.shapes.size()<<",\"objects\":"<<s.objects.size()<<",\"instances\":"<<s.instances.size()<<",\"vertices\":"<<r.vertices.size()<<",\"triangles\":"<<r.primitives.size()<<",\"placements\":"<<r.instances.size()<<",\"nodes\":"<<r.nodes.size()<<",\"emitters\":"<<r.emitters.size()<<"},\"camera\":"<<entity(s.camera)
     <<",\"film\":"<<entity(s.film)<<",\"sampler\":"<<entity(s.sampler)<<",\"integrator\":"<<entity(s.integrator)<<",\"pixelFilter\":"<<entity(s.pixel_filter)<<",\"accelerator\":"<<entity(s.accelerator)<<",\"fov\":"<<scalar(s.camera,"fov").value_or(90)<<",\"width\":"<<scalar(s.film,"xresolution").value_or(128)<<",\"height\":"<<scalar(s.film,"yresolution").value_or(128)
     <<",\"maxDepth\":"<<scalar(s.integrator,"maxdepth").value_or(8)<<",\"filter\":"<<quote(s.pixel_filter.type)<<",\"filterRadius\":["<<scalar(s.pixel_filter,"xradius").value_or(s.pixel_filter.type=="box"?.5:2)<<','<<scalar(s.pixel_filter,"yradius").value_or(s.pixel_filter.type=="box"?.5:2)<<']';
    auto list=[&](const char* name,const auto& items) { o<<','<<quote(name)<<":["; bool first=true; for(auto& e:items) {if(!first)o<<',';first=false;o<<entity(e);}o<<']'; };
    list("shapes",s.shapes); list("materials",s.materials); list("textures",s.textures); list("lights",s.lights); list("areaLights",s.area_lights); list("media",s.media);
    o<<",\"images\":[";bool image_first=true;
    for(auto& image:r.images){if(!image_first)o<<',';image_first=false;o<<"{\"path\":"<<quote(image.path)<<",\"role\":"<<quote(image.role)<<",\"srgb\":"<<(image.srgb?"true":"false")<<",\"levels\":[";bool level_first=true;for(auto& level:image.levels){if(!level_first)o<<',';level_first=false;o<<"{\"width\":"<<level.width<<",\"height\":"<<level.height<<",\"offset\":"<<level.offset<<",\"bytes\":"<<level.bytes<<'}';}o<<"]}";}o<<']';
    o<<",\"worldShapes\":[";for(Id j=0;j<s.world_shapes.size();++j){if(j)o<<',';o<<s.world_shapes[j];}o<<']';
    o<<",\"objects\":["; bool first=true;
    for(auto& e:s.objects) { if(!first)o<<',';first=false; o<<"{\"name\":"<<quote(e.name)<<",\"scope\":"<<e.scope<<",\"source\":"<<quote(e.location.filename)<<",\"line\":"<<e.location.line<<",\"shapes\":["; for(Id j=0;j<e.shapes.size();++j){if(j)o<<',';o<<e.shapes[j];}o<<"]}"; }
    o<<"],\"instances\":["; first=true; for(auto& e:s.instances) {if(!first)o<<',';first=false;o<<"{\"object\":"<<e.object<<",\"name\":"<<quote(e.object_name)<<",\"scope\":"<<e.scope<<",\"source\":"<<quote(e.location.filename)<<",\"line\":"<<e.location.line<<",\"transform\":"<<matrix(e.transform.start)<<",\"endTransform\":"<<matrix(e.transform.end)<<'}';}
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
        images(r,s,resources);
        for(auto& e:s.materials) {
            if(e.type=="diffuse")supported_parameters(r,e,{"type","reflectance","displacement"});
            else supported_parameters(r,e,{"type","reflectance","eta","k","roughness","uroughness","vroughness","remaproughness","interface.roughness","displacement"});
            Material m; auto c=color(e,"reflectance",e.type=="conductor" || e.type=="dielectric"?Vec3{1,1,1}:Vec3{.5,.5,.5}); for(int a=0;a<3;++a)m.color[a]=f32(axis(c,a));
            m.color[3]=1; m.info[0]=e.type=="diffuse"?0:e.type=="coateddiffuse"?1:e.type=="conductor"?2:e.type=="dielectric"?3:none;
            m.info[1]=texture_ref(r,s,e,"reflectance","spectrum");m.info[2]=texture_ref(r,s,e,"displacement","float");
            if(m.info[1]!=none)m.color[0]=m.color[1]=m.color[2]=1;
            auto rough=scalar(e,"roughness").value_or(e.type=="coateddiffuse"?.5:0);
            auto u=scalar(e,"uroughness").value_or(rough),v=scalar(e,"vroughness").value_or(rough);
            if(u<0 || v<0 || u>1 || v>1)diagnostic(r,e,"web.roughness.invalid","Expected roughness in [0,1]",Severity::error);
            if(e.type=="coateddiffuse"){
                // LightTransport's Principled + clearcoat approximation; explicit .02 clamp.
                m.k_ior[0]=f32(std::pow(std::clamp(scalar(e,"interface.roughness").value_or(u),.02,1.),2));
                m.k_ior[1]=f32(std::pow(std::clamp(scalar(e,"interface.roughness").value_or(v),.02,1.),2));
                u=v=1; // The LT loader fixes the underlying Principled roughness to 1.
            }else if(boolean(e,"remaproughness",true)){u=std::sqrt(std::max(0.,u));v=std::sqrt(std::max(0.,v));}
            m.eta_roughness[3]=f32(u);m.k_ior[3]=f32(v);
            auto eta=e.type=="conductor"?optical_color(r,e,"eta",{.200438,.924033,1.102212}):Vec3{scalar(e,"eta").value_or(1.5),0,0};
            auto k=e.type=="conductor"?optical_color(r,e,"k",{3.912949,2.452848,2.142188}):Vec3{m.k_ior[0],m.k_ior[1],0};
            for(int a=0;a<3;++a){m.eta_roughness[a]=f32(axis(eta,a));m.k_ior[a]=f32(axis(k,a));}
            if(e.type=="dielectric" && eta.x<=0)diagnostic(r,e,"web.eta.invalid","Expected positive dielectric IOR",Severity::error);
            if(m.info[0]==none)diagnostic(r,e,"web.material.pending","WGSL material implementation pending: "+e.type);
            for(auto* ps:{&e.parameters,&e.inherited_parameters}) for(auto& p:*ps){
                if((p.type=="texture" && p.name!="reflectance" && p.name!="displacement") || p.type=="blackbody" || (p.type=="spectrum" && p.name!="eta" && p.name!="k"))diagnostic(r,e,"web.parameter.pending","Parameter evaluation pending: "+p.type+" "+p.name);
            }
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
        for(auto& light:s.lights){
            supported_parameters(r,light,{"L","scale"});
            if(light.type!="infinite" || (find_parameter(light,"L") && find_parameter(light,"L")->type!="rgb"))diagnostic(r,light,"web.light.pending","Only constant RGB infinite lights are supported");
            else {auto c=color(light,"L",{1,1,1})*scalar(light,"scale").value_or(1);for(int a=0;a<3;++a)r.environment[a]+=f32(axis(c,a));}
        }
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
        r.records[10]=id(r.records.size());r.records[11]=(r.environment[0]+r.environment[1]+r.environment[2])>0?1:0;append(r.records,std::vector{r.environment});
        r.records[8]=id(r.records.size());r.records[9]=id(r.textures.size());append(r.records,r.textures);
        std::vector<Alpha> alphas;for(auto& instance:r.instances){auto& shape=s.shapes[instance.info[2]];Alpha alpha;alpha.value=f32(scalar(shape,"alpha").value_or(1));alpha.texture=texture_ref(r,s,shape,"alpha","float");if(alpha.value<0 || alpha.value>1)diagnostic(r,shape,"web.alpha.invalid","Expected alpha in [0,1]",Severity::error);alphas.push_back(alpha);}
        r.records[12]=id(r.records.size());r.records[13]=id(alphas.size());append(r.records,alphas);
    } catch(const std::exception& e) {r.renderable=false;r.diagnostics.push_back({Severity::error,"web.pack",e.what(),{}});}
    metadata(r,s);return r;
}
RenderScene open(const std::filesystem::path& p) {
    auto source=load(p); if(!source) {RenderScene r;r.renderable=false;r.diagnostics=source.diagnostics;metadata(r,source.scene);return r;}
    auto r=build(source.scene);r.diagnostics.insert(r.diagnostics.begin(),source.diagnostics.begin(),source.diagnostics.end());metadata(r,source.scene);return r;
}
const void* data(const RenderScene& r,unsigned k) {switch(k){case 0:return r.vertices.data();case 1:return r.primitives.data();case 2:return r.nodes.data();case 3:return r.records.data();case 4:return r.image_pixels.data();default:return nullptr;}}
std::size_t bytes(const RenderScene& r,unsigned k) {switch(k){case 0:return r.vertices.size()*sizeof(Vertex);case 1:return r.primitives.size()*sizeof(Primitive);case 2:return r.nodes.size()*sizeof(Node);case 3:return r.records.size()*4;case 4:return r.image_pixels.size();default:return 0;}}
}
