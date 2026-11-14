#include <pbrt4/pbrt4.h>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2) { std::cerr<<"Usage: minimal_loader scene.pbrt\n"; return 2; }
    auto result=pbrt4::load(argv[1]);
    for(const auto& d:result.diagnostics) std::cerr<<pbrt4::format(d)<<'\n';
    if(!result) return 1;
    const auto& scene=result.scene;
    std::cout<<scene.world_shapes.size()<<" world shapes\n";
    for(const auto& instance:scene.instances) {
        const auto& object=scene.objects.at(instance.object);
        std::cout<<object.name<<": "<<object.shapes.size()<<" shared shapes\n";
    }
}
