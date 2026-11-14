#include <pbrt4/pbrt4.h>
#include <iostream>
#include <algorithm>
int main(int argc,char** argv) {
    if(argc!=2) { std::cerr<<"Usage: pbrt4_info scene.pbrt\n"; return 2; }
    auto r=pbrt4::load(argv[1]);
    for(const auto& d:r.diagnostics) std::cerr<<pbrt4::format(d)<<'\n';
    auto& s=r.scene;
    std::cout<<"shapes: "<<s.shapes.size()<<"\nworld shapes: "<<s.world_shapes.size()
        <<"\nanimated shapes: "<<std::count_if(s.shapes.begin(),s.shapes.end(),[](auto& shape){return shape.transform.animated();})
        <<"\nobject definitions: "<<s.objects.size()<<"\nobject instances: "<<s.instances.size()
        <<"\nmaterials (including default): "<<s.materials.size()<<"\ntextures: "<<s.textures.size()
        <<"\nlights: "<<s.lights.size()<<"\narea light declarations: "<<s.area_lights.size()
        <<"\nmedia: "<<s.media.size()<<"\ncamera: "<<s.camera.type<<"\nfilm: "<<s.film.type
        <<"\ndiagnostics: "<<r.diagnostics.size()<<'\n';
    return r ? 0 : 1;
}
