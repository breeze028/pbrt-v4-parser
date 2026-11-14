#include <pbrt4/geometry.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace pbrt4;
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
void near(double a,double b) { check(std::abs(a-b)<1e-9,"numeric expectation"); }
std::string header(std::string format="ascii",std::string extra="") {
    return "ply\nformat "+format+" 1.0\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\n"+extra+"element face 1\nproperty list uchar int vertex_indices\nend_header\n";
}
GeometryOptions memory(std::string data) {
    GeometryOptions o; o.resources.read_file=[data=std::move(data)](const auto&)->std::optional<std::string>{return data;}; return o;
}
const std::map<std::string,std::function<void()>> tests={
{"ascii_ply",[]{
    auto r=load_ply(std::filesystem::path(PBRT4_TESTDATA)/"triangle.ply"); check(bool(r),"ASCII PLY file");
    check(r.mesh.positions.size()==3 && r.mesh.normals.size()==3 && r.mesh.texcoords.size()==3,"PLY attributes"); check(r.mesh.indices==std::vector<std::uint32_t>{0,1,2},"PLY triangle indices");
    auto bare=load_ply("virtual.ply",memory(header()+"0 0 0\n1 0 0\n0 1 0\n3 0 1 2\n")); check(bool(bare),"bare PLY");
    check(bare.mesh.normals.empty() && bare.mesh.texcoords.empty(),"Missing normals and UV are not fabricated");
}},
{"binary_ply",[]{
    for(bool little:{true,false}) {
        auto data=header(little?"binary_little_endian":"binary_big_endian");
        auto add=[&](auto value) { std::array<char,sizeof(value)> bytes; std::memcpy(bytes.data(),&value,sizeof(value)); if(little!=(std::endian::native==std::endian::little)) std::reverse(bytes.begin(),bytes.end()); data.append(bytes.data(),bytes.size()); };
        for(float v:{0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f,0.f}) add(v);
        add(std::uint8_t(3)); for(std::int32_t i:{0,1,2}) add(i);
        auto r=load_ply("virtual.ply",memory(data)); check(bool(r),"binary PLY endian decoding"); near(r.mesh.positions[1].x,1); near(r.mesh.positions[2].y,1);
        data.pop_back(); r=load_ply("virtual.ply",memory(data)); check(!r,"truncated binary PLY");
    }
}},
{"ply_errors",[]{
    for(auto data:{header()+"0 0 0\n1 0 0\n0 1 0\n3 0 1 9",header()+"0 0 0\n1 0 0\n0 1 0\n-1",header()+"0 0 0\n1 0 0\n0 1 0\n5 0 1 2 0 1",std::string("ply\nformat unknown 1.0\nend_header\n"),std::string("ply\nformat ascii 1.0\nelement vertex -1\nend_header\n")})
        check(!load_ply("virtual.ply",memory(data)),"Malformed PLY rejected");
    auto o=memory(header()+"0 0 0\n1 0 0\n0 1 0\n3 0 1 2"); o.max_vertices=2; check(!load_ply("virtual.ply",o),"PLY vertex limit");
}},
{"triangle_mesh",[]{
    auto r=load_string(R"(WorldBegin Shape "trianglemesh" "point3 P" [0 0 0 1 0 0 0 1 0])"); check(bool(r),"triangle source");
    auto mesh=extract_triangle_mesh(r.scene.shapes[0]); check(bool(mesh),"implicit single triangle indices"); check(mesh.mesh.indices.size()==3,"one triangle"); check(mesh.mesh.texcoords.empty(),"No generated UV");
    r=load_string(R"(WorldBegin Shape "trianglemesh" "point3 P" [0 0 0 1 0 0 0 1 0] "integer indices" [0 1 3])");
    check(bool(r) && !extract_triangle_mesh(r.scene.shapes[0]),"Semantic geometry validation belongs to geometry utility");
}},
{"shared_geometry",[]{
    auto r=load_string(R"(WorldBegin ObjectBegin "o" Translate 1 0 0 Shape "trianglemesh" "point3 P" [0 0 0 1 0 0 0 1 0] ObjectEnd Translate 3 0 0 ObjectInstance "o" Translate 2 0 0 ObjectInstance "o")"); check(bool(r),"instance source");
    auto g=extract_geometry(r.scene); check(bool(g),"instance extraction"); check(g.meshes.size()==1 && g.instances.size()==2,"geometry stays shared");
    near(g.instances[0].transform.start.m[3],4); near(g.instances[1].transform.start.m[3],6); near(g.meshes[0].positions[0].x,0);
    auto baked=bake_instance(g.meshes[0],g.instances[1]); check(bool(baked),"explicit baking"); near(baked.mesh.positions[0].x,6);
}},
{"sphere",[]{
    auto r=load_string(R"(WorldBegin Shape "sphere" "float radius" 2)"); check(bool(r),"sphere source");
    check(!extract_triangle_mesh(r.scene.shapes[0]),"Sphere tessellation opt-in"); GeometryOptions o; o.tessellate_spheres=true;
    auto mesh=extract_triangle_mesh(r.scene.shapes[0],o); check(bool(mesh),"Sphere tessellation"); near(mesh.mesh.positions[0].z,2);
    for(std::size_t i=0;i<mesh.mesh.indices.size();i+=3) {
        auto a=mesh.mesh.positions[mesh.mesh.indices[i]],b=mesh.mesh.positions[mesh.mesh.indices[i+1]],c=mesh.mesh.positions[mesh.mesh.indices[i+2]];
        check(dot(cross(b-a,c-a),a+b+c)>0,"Sphere winding is outward and nondegenerate");
    }
}},
{"bake_normals",[]{
    TriangleMesh m; m.positions={{0,0,0},{1,0,0},{0,1,0}}; m.normals={{0,0,1},{0,0,1},{0,0,1}}; m.indices={0,1,2};
    MeshInstance i; i.transform.start.m[0]=-2; i.transform.start.m[5]=3; i.transform.end=i.transform.start;
    auto r=bake_instance(m,i); check(bool(r),"reflection baking"); auto& p=r.mesh.positions; auto& idx=r.mesh.indices;
    check(dot(cross(p[idx[1]]-p[idx[0]],p[idx[2]]-p[idx[0]]),r.mesh.normals[0])>0,"normal and winding agree after reflection");
    i.reverse_orientation=true; r=bake_instance(m,i); check(bool(r),"reverse orientation baking"); check(r.mesh.normals[0].z<0,"reversed normal");
    i.transform.end.m[3]=1; check(!bake_instance(m,i),"Animated baking explicitly rejected");
}}
};
int main(int argc,char** argv) {
    int count=0;
    for(auto& [name,run]:tests) if(argc==1 || name==argv[1]) {
        try { run(); std::cout<<"PASS "<<name<<'\n'; ++count; }
        catch(const std::exception& e) { std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n'; return 1; }
    }
    return count?0:2;
}
