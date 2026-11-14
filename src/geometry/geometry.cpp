// SPDX-License-Identifier: Apache-2.0
#include <pbrt4/geometry.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <locale>
#include <map>
#include <numbers>
#include <sstream>
#include <stdexcept>

namespace pbrt4 {
namespace {
struct Bad : std::runtime_error { using std::runtime_error::runtime_error; };
void require(bool b,const char* m) { if (!b) throw Bad(m); }
void diag(std::vector<Diagnostic>& d,SourceLocation l,std::string message) { d.push_back({Severity::error,"geometry.invalid",std::move(message),std::move(l)}); }
void validate(const TriangleMesh& m,const GeometryOptions& o) {
    require(!m.positions.empty(),"Mesh has no vertices");
    require(m.positions.size()<=o.max_vertices && m.indices.size()<=o.max_indices,"Geometry exceeds configured limits");
    require(m.indices.size()%3==0,"Triangle indices must be a multiple of 3");
    for (auto i:m.indices) require(i<m.positions.size(),"Vertex index out of range");
    require(m.normals.empty() || m.normals.size()==m.positions.size(),"Normal count must match vertex count");
    require(m.texcoords.empty() || m.texcoords.size()==m.positions.size(),"UV count must match vertex count");
    for (auto p:m.positions) require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"Non-finite vertex position");
    for (auto n:m.normals) require(std::isfinite(n.x)&&std::isfinite(n.y)&&std::isfinite(n.z),"Non-finite normal");
    for (auto uv:m.texcoords) require(std::isfinite(uv.x)&&std::isfinite(uv.y),"Non-finite UV");
}
std::vector<double> numbers(const Parameter* p,std::string_view type) {
    if (!p) return {};
    require(p->type==type,"Unexpected geometry parameter type");
    std::vector<double> v; for (auto& x:p->values) { auto d=std::get_if<double>(&x); require(d!=nullptr,"Expected floating-point values"); v.push_back(*d); }
    return v;
}
struct ScalarType { int bytes; bool floating, signed_value; };
ScalarType ply_type(const std::string& t) {
    if (t=="char" || t=="int8") return {1,false,true};
    if (t=="uchar" || t=="uint8") return {1,false,false};
    if (t=="short" || t=="int16") return {2,false,true};
    if (t=="ushort" || t=="uint16") return {2,false,false};
    if (t=="int" || t=="int32") return {4,false,true};
    if (t=="uint" || t=="uint32") return {4,false,false};
    if (t=="float" || t=="float32") return {4,true,true};
    if (t=="double" || t=="float64") return {8,true,true};
    throw Bad("Unknown PLY scalar type: "+t);
}
struct Property { std::string name; ScalarType type{}, count{}; bool list=false; };
struct Element { std::string name; std::size_t count=0; std::vector<Property> properties; };
double ply_scalar(std::istream& in,ScalarType type,bool ascii,bool little) {
    if (ascii) {
        double v; require(bool(in>>v) && std::isfinite(v),"Invalid/truncated ASCII PLY value");
        if (!type.floating) {
            double min=type.signed_value ? -std::ldexp(1.,type.bytes*8-1) : 0;
            double max=std::ldexp(1.,type.bytes*8-(type.signed_value?1:0))-1;
            require(v==std::floor(v) && v>=min && v<=max,"PLY integer out of range");
        }
        return v;
    }
    std::array<unsigned char,8> data{};
    require(bool(in.read(reinterpret_cast<char*>(data.data()),type.bytes)),"Truncated binary PLY");
    if (little!=(std::endian::native==std::endian::little)) std::reverse(data.begin(),data.begin()+type.bytes);
    double v=0;
    if (type.floating) {
        if (type.bytes==4) { float f; std::memcpy(&f,data.data(),4); v=f; }
        else std::memcpy(&v,data.data(),8);
    } else if (type.signed_value) {
        if (type.bytes==1) { std::int8_t x; std::memcpy(&x,data.data(),1); v=x; }
        if (type.bytes==2) { std::int16_t x; std::memcpy(&x,data.data(),2); v=x; }
        if (type.bytes==4) { std::int32_t x; std::memcpy(&x,data.data(),4); v=x; }
    } else {
        if (type.bytes==1) v=data[0];
        if (type.bytes==2) { std::uint16_t x; std::memcpy(&x,data.data(),2); v=x; }
        if (type.bytes==4) { std::uint32_t x; std::memcpy(&x,data.data(),4); v=x; }
    }
    require(std::isfinite(v),"Non-finite binary PLY value"); return v;
}
TriangleMesh read_ply(std::istream& in,const GeometryOptions& o) {
    std::string line; auto getline=[&] { require(bool(std::getline(in,line)),"Truncated PLY header"); if (!line.empty()&&line.back()=='\r') line.pop_back(); };
    getline(); require(line=="ply","Not a PLY file");
    bool ascii=false,little=true,format=false;
    std::vector<Element> elements;
    for(std::size_t lines=0;;++lines) {
        require(lines<4096,"PLY header too long"); getline(); std::istringstream row(line); row.imbue(std::locale::classic()); std::string key; row>>key;
        if (key=="comment" || key=="obj_info" || key.empty()) continue;
        if (key=="end_header") break;
        if (key=="format") {
            std::string encoding,version; require(bool(row>>encoding>>version) && version=="1.0","Invalid PLY format declaration");
            require(!format,"Duplicate PLY format declaration"); format=true; ascii=encoding=="ascii"; little=encoding!="binary_big_endian";
            require(ascii || encoding=="binary_little_endian" || encoding=="binary_big_endian","Unsupported PLY encoding");
        } else if (key=="element") {
            Element e; std::string count; require(bool(row>>e.name>>count),"Invalid PLY element");
            require(!count.empty() && count[0]!='-',"Negative PLY element count");
            std::size_t consumed=0; e.count=std::stoull(count,&consumed); require(consumed==count.size() && e.count<=o.max_indices,"PLY element count limit");
            if (e.name=="vertex") require(e.count<=o.max_vertices,"PLY vertex limit");
            elements.push_back(std::move(e));
        } else if (key=="property") {
            require(!elements.empty(),"PLY property outside element"); Property p; std::string type;
            require(bool(row>>type),"Invalid PLY property");
            if (type=="list") { std::string count; require(bool(row>>count>>type),"Invalid PLY list"); p.list=true; p.count=ply_type(count); require(!p.count.floating,"List count must be integer"); }
            p.type=ply_type(type); require(bool(row>>p.name),"Missing PLY property name");
            require(std::none_of(elements.back().properties.begin(),elements.back().properties.end(),[&](auto& previous){return previous.name==p.name;}),"Duplicate PLY property");
            elements.back().properties.push_back(p);
        } else throw Bad("Unknown PLY header directive: "+key);
    }
    require(format,"Missing PLY format"); TriangleMesh mesh; bool vertices=false,faces=false;
    for (const auto& e:elements) {
        bool vertex=e.name=="vertex", face=e.name=="face";
        auto has=[&](const char* n) { return std::any_of(e.properties.begin(),e.properties.end(),[&](auto& p){return p.name==n && !p.list;}); };
        if (vertex) { require(!vertices,"Duplicate PLY vertex element"); vertices=true; require(has("x")&&has("y")&&has("z"),"PLY missing x/y/z"); }
        if (face) { require(!faces,"Duplicate PLY face element"); faces=true; }
        bool normals=has("nx")&&has("ny")&&has("nz");
        if(vertex) require(normals || !(has("nx") || has("ny") || has("nz")),"Incomplete PLY normals");
        std::string u=has("u") ? "u" : has("s") ? "s" : "texture_u", v=has("v") ? "v" : has("t") ? "t" : "texture_v";
        bool uv=has(u.c_str())&&has(v.c_str());
        if(vertex) require(uv || !(has(u.c_str()) || has(v.c_str())),"Incomplete PLY texture coordinates");
        for (std::size_t row=0;row<e.count;++row) {
            std::map<std::string,double> values; std::vector<std::uint32_t> indices;
            for (const auto& p:e.properties) {
                if (!p.list) { double n=ply_scalar(in,p.type,ascii,little); if (vertex) values[p.name]=n; }
                else {
                    double count=ply_scalar(in,p.count,ascii,little); require(count>=0 && count<=static_cast<double>(o.max_indices),"Invalid PLY list size");
                    bool index=face && (p.name=="vertex_indices" || p.name=="vertex_index");
                    if (index) require(count==3 || count==4,"Only triangle/quad PLY faces supported");
                    for (std::size_t j=0;j<static_cast<std::size_t>(count);++j) {
                        double value=ply_scalar(in,p.type,ascii,little);
                        if (index) { require(!p.type.floating && value>=0 && value<=UINT32_MAX,"Invalid PLY index"); indices.push_back(static_cast<std::uint32_t>(value)); }
                    }
                }
            }
            if (vertex) {
                mesh.positions.push_back({values.at("x"),values.at("y"),values.at("z")});
                if (normals) mesh.normals.push_back({values.at("nx"),values.at("ny"),values.at("nz")});
                if (uv) mesh.texcoords.push_back({values.at(u),values.at(v)});
            }
            if (face) {
                require(indices.size()==3 || indices.size()==4,"PLY face has no supported vertex index list");
                require(mesh.indices.size()+(indices.size()==4?6:3)<=o.max_indices,"PLY index limit");
                mesh.indices.insert(mesh.indices.end(),{indices[0],indices[1],indices[2]});
                if (indices.size()==4) mesh.indices.insert(mesh.indices.end(),{indices[0],indices[2],indices[3]});
            }
        }
    }
    validate(mesh,o); return mesh;
}
}
MeshResult load_ply(const std::filesystem::path& p,const GeometryOptions& o) {
    MeshResult r;
    try {
        if (o.resources.read_file) {
            auto data=o.resources.read_file(p); require(data.has_value(),"Cannot open PLY resource"); require(data->size()<=o.resources.max_file_bytes,"PLY file size limit");
            std::istringstream in(*data,std::ios::binary); in.imbue(std::locale::classic()); r.mesh=read_ply(in,o);
        } else {
            std::ifstream in(p,std::ios::binary|std::ios::ate); require(bool(in),"Cannot open PLY resource");
            auto size=in.tellg(); require(size>=0 && static_cast<std::uintmax_t>(size)<=o.resources.max_file_bytes,"PLY file size limit");
            in.seekg(0); in.imbue(std::locale::classic()); r.mesh=read_ply(in,o);
        }
    } catch (const std::exception& e) { diag(r.diagnostics,{path_to_utf8(p),1,1},e.what()); }
    return r;
}
MeshResult extract_triangle_mesh(const Shape& s,const GeometryOptions& o) {
    MeshResult r;
    try {
        if (s.type=="plymesh") {
            auto file=std::find_if(s.resources.begin(),s.resources.end(),[](auto& ref){return ref.parameter=="filename";});
            require(file!=s.resources.end(),"PLY mesh requires filename resource"); r=load_ply(file->resolved,o);
        } else if (s.type=="trianglemesh") {
            auto p=numbers(find_parameter(s,"P"),"point3"); require(p.size()%3==0,"Invalid point count"); require(p.size()/3<=o.max_vertices,"Vertex limit");
            for (std::size_t i=0;i<p.size();i+=3) r.mesh.positions.push_back({p[i],p[i+1],p[i+2]});
            auto n=numbers(find_parameter(s,"N"),"normal3"); require(n.size()%3==0,"Invalid normal count");
            for (std::size_t i=0;i<n.size();i+=3) r.mesh.normals.push_back({n[i],n[i+1],n[i+2]});
            auto uvp=find_parameter(s,"uv"); auto uv=numbers(uvp,uvp && uvp->type=="float" ? "float" : "point2");
            require(uv.size()%2==0,"Invalid UV count"); for (std::size_t i=0;i<uv.size();i+=2) r.mesh.texcoords.push_back({uv[i],uv[i+1]});
            auto indices=find_parameter(s,"indices");
            if (!indices) { require(r.mesh.positions.size()==3,"Indices may be omitted only for a single triangle"); r.mesh.indices={0,1,2}; }
            else { require(indices->type=="integer","Indices must be integers"); require(indices->values.size()<=o.max_indices,"Index limit");
                for (auto& v:indices->values) { auto i=std::get<std::int64_t>(v); require(i>=0 && i<=UINT32_MAX,"Invalid index"); r.mesh.indices.push_back(static_cast<std::uint32_t>(i)); }
            }
        } else if (s.type=="sphere" && o.tessellate_spheres) {
            unsigned seg=o.sphere_segments,rings=o.sphere_rings;
            require(seg>=3 && rings>=2 && seg<=4096 && rings<=4096,"Invalid sphere tessellation resolution");
            require((std::uint64_t(seg)+1)*(rings+1)<=o.max_vertices && std::uint64_t(seg)*rings*6<=o.max_indices,"Sphere size limit");
            double radius=scalar(s,"radius").value_or(1); require(radius>0,"Sphere radius must be positive");
            double zmin=scalar(s,"zmin").value_or(-radius),zmax=scalar(s,"zmax").value_or(radius),phi=scalar(s,"phimax").value_or(360);
            require(zmin==-radius && zmax==radius && phi==360,"Clipped spheres are preserved in IR but not tessellated yet");
            for(unsigned y=0;y<=rings;++y) for(unsigned x=0;x<=seg;++x) {
                double theta=std::numbers::pi*y/rings, angle=2*std::numbers::pi*x/seg;
                Vec3 n{std::sin(theta)*std::cos(angle),std::sin(theta)*std::sin(angle),std::cos(theta)};
                r.mesh.positions.push_back(n*radius); r.mesh.normals.push_back(n); r.mesh.texcoords.push_back({double(x)/seg,double(y)/rings});
            }
            for(unsigned y=0;y<rings;++y) for(unsigned x=0;x<seg;++x) {
                auto a=y*(seg+1)+x,b=a+seg+1;
                if(y>0) r.mesh.indices.insert(r.mesh.indices.end(),{a,b,a+1});
                if(y+1<rings) r.mesh.indices.insert(r.mesh.indices.end(),{a+1,b,b+1});
            }
        } else throw Bad("No geometry adapter for shape '"+s.type+"' (sphere tessellation is opt-in)");
        if (r) validate(r.mesh,o);
    } catch (const std::exception& e) { diag(r.diagnostics,s.location,e.what()); }
    return r;
}
GeometryResult extract_geometry(const Scene& s,const GeometryOptions& o) {
    GeometryResult r; std::map<Id,Id> cache;
    auto place=[&](Id shape,const AnimatedTransform& parent) {
        if (shape>=s.shapes.size()) { diag(r.diagnostics,{},"Invalid shape handle"); return; }
        const auto& source=s.shapes[shape]; Id mesh;
        if (auto it=cache.find(shape);it!=cache.end()) mesh=it->second;
        else {
            auto loaded=extract_triangle_mesh(source,o);
            r.diagnostics.insert(r.diagnostics.end(),loaded.diagnostics.begin(),loaded.diagnostics.end());
            mesh=loaded ? r.meshes.size() : invalid_id; cache[shape]=mesh;
            if (loaded) { loaded.mesh.source_shape=shape; r.meshes.push_back(std::move(loaded.mesh)); }
        }
        if (mesh==invalid_id) return;
        AnimatedTransform t=source.transform; t.start=parent.start*t.start; t.end=parent.end*t.end;
        r.instances.push_back({mesh,shape,source.material,source.area_light,t,source.reverse_orientation});
    };
    for (Id id:s.world_shapes) place(id,{});
    for (const auto& i:s.instances) {
        if (i.object>=s.objects.size()) { diag(r.diagnostics,i.location,"Invalid object reference"); continue; }
        for (auto shape:s.objects[i.object].shapes) place(shape,i.transform);
    }
    return r;
}
MeshResult bake_instance(const TriangleMesh& m,const MeshInstance& i) {
    MeshResult r;
    try { validate(m,{}); } catch(const std::exception& e) { diag(r.diagnostics,{},e.what()); return r; }
    if (i.transform.animated()) { diag(r.diagnostics,{},"Animated geometry baking requires a renderer time sampler"); return r; }
    auto& t=i.transform.start;
    if (!inverse(t) || t.m[12]!=0 || t.m[13]!=0 || t.m[14]!=0 || t.m[15]!=1) { diag(r.diagnostics,{},"Baking requires an invertible affine matrix"); return r; }
    r.mesh=m;
    for(auto& p:r.mesh.positions) p=transform_point(t,p);
    for(auto& n:r.mesh.normals) { n=transform_normal(t,n); if(i.reverse_orientation) n=-n; }
    // Correct winding after reflection, then apply ReverseOrientation.
    if(i.reverse_orientation != (determinant3(t)<0)) for(std::size_t j=0;j+2<r.mesh.indices.size();j+=3) std::swap(r.mesh.indices[j+1],r.mesh.indices[j+2]);
    return r;
}
}
