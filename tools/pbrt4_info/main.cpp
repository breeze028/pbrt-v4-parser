#include <pbrt4/pbrt4.h>
#ifdef PBRT4_INFO_GEOMETRY
#include <pbrt4/geometry.h>
#endif
#include <iostream>
#include <algorithm>
#include <map>
#include <set>

namespace {
void usage() { std::cerr<<"Usage: pbrt4_info [--details] [--check-resources] [--geometry] scene.pbrt\n"; }
template<class T> void types(const char* label,const std::vector<T>& entities) {
    std::map<std::string,std::size_t> counts;
    for(const auto& e:entities) ++counts[e.type];
    for(const auto& [type,count]:counts) std::cout<<label<<" ["<<type<<"]: "<<count<<'\n';
}
}
int main(int argc,char** argv) {
    bool details=false,resources=false,geometry=false;
    std::string filename;
    for(int i=1;i<argc;++i) {
        std::string arg=argv[i];
        if(arg=="--details") details=true;
        else if(arg=="--check-resources") resources=true;
        else if(arg=="--geometry") geometry=true;
        else if(arg=="--help") { usage(); return 0; }
        else if(arg.starts_with("--") || !filename.empty()) { usage(); return 2; }
        else filename=arg;
    }
    if(filename.empty()) { usage(); return 2; }
#ifndef PBRT4_INFO_GEOMETRY
    if(geometry) { std::cerr<<"Geometry utilities were disabled at build time\n"; return 2; }
#endif
    auto r=pbrt4::load(pbrt4::path_from_utf8(filename));
    for(const auto& d:r.diagnostics) std::cerr<<pbrt4::format(d)<<'\n';
    auto& s=r.scene;
    std::cout<<"shapes: "<<s.shapes.size()<<"\nworld shapes: "<<s.world_shapes.size()
        <<"\nanimated shapes: "<<std::count_if(s.shapes.begin(),s.shapes.end(),[](auto& shape){return shape.transform.animated();})
        <<"\nobject definitions: "<<s.objects.size()<<"\nobject instances: "<<s.instances.size()
        <<"\nmaterials (including default): "<<s.materials.size()<<"\ntextures: "<<s.textures.size()
        <<"\nlights: "<<s.lights.size()<<"\narea light declarations: "<<s.area_lights.size()
        <<"\nmedia: "<<s.media.size()<<"\ncamera: "<<s.camera.type<<"\nfilm: "<<s.film.type
        <<"\ndiagnostics: "<<r.diagnostics.size()<<'\n';
    if(!r) return 1;
    bool success=true;
    if(details) {
        types("shape type",s.shapes); types("material type",s.materials); types("texture type",s.textures);
        types("light type",s.lights); types("area light type",s.area_lights);
        std::size_t alpha=0,displacement=0;
        for(const auto& shape:s.shapes) if(pbrt4::find_parameter(shape,"alpha")) ++alpha;
        for(const auto& material:s.materials) if(pbrt4::find_parameter(material,"displacement")) ++displacement;
        std::cout<<"shapes with alpha: "<<alpha<<"\nmaterials with displacement: "<<displacement<<'\n';
    }
    if(resources) {
        std::set<std::filesystem::path> paths;
        std::size_t spectral=0,missing=0;
        auto collect=[&](const pbrt4::Entity& e) {
            for(const auto& ref:e.resources) {
                if(ref.spectrum_name_or_file) { ++spectral; continue; }
                paths.insert(ref.resolved);
            }
        };
        // Film filename is an output, so it is intentionally not an input dependency.
        collect(s.camera); collect(s.integrator); collect(s.sampler); collect(s.accelerator); collect(s.pixel_filter);
        for(auto* list:{&s.materials,&s.textures,&s.lights,&s.area_lights,&s.media}) for(const auto& e:*list) collect(e);
        for(const auto& shape:s.shapes) collect(shape);
        for(const auto& path:paths) {
            std::error_code ec;
            if(!std::filesystem::is_regular_file(path,ec)) { ++missing; std::cerr<<"Missing input resource: "<<pbrt4::path_to_utf8(path)<<'\n'; }
        }
        std::cout<<"unique input resources: "<<paths.size()<<"\nmissing input resources: "<<missing
            <<"\nspectrum strings (name or file, not checked): "<<spectral<<'\n';
        success=success && missing==0;
    }
#ifdef PBRT4_INFO_GEOMETRY
    if(geometry) {
        auto extracted=pbrt4::extract_geometry(s);
        for(const auto& d:extracted.diagnostics) std::cerr<<pbrt4::format(d)<<'\n';
        std::uint64_t vertices=0,triangles=0,placed=0,normal_meshes=0,uv_meshes=0;
        for(const auto& mesh:extracted.meshes) {
            vertices+=mesh.positions.size(); triangles+=mesh.indices.size()/3;
            normal_meshes+=!mesh.normals.empty(); uv_meshes+=!mesh.texcoords.empty();
        }
        for(const auto& i:extracted.instances) placed+=extracted.meshes.at(i.mesh).indices.size()/3;
        std::cout<<"geometry meshes: "<<extracted.meshes.size()<<"\ngeometry placements: "<<extracted.instances.size()
            <<"\ngeometry vertices: "<<vertices<<"\ngeometry triangles: "<<triangles<<"\nplaced triangles: "<<placed
            <<"\nmeshes with normals: "<<normal_meshes<<"\nmeshes with UV: "<<uv_meshes
            <<"\ngeometry diagnostics: "<<extracted.diagnostics.size()<<'\n';
        success=success && bool(extracted);
    }
#endif
    return success ? 0 : 1;
}
