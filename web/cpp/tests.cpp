#include "scene.h"
#include <iostream>
#include <stdexcept>
using namespace pbrt4;
void check(bool v,const char* s){if(!v)throw std::runtime_error(s);}
int main(){try{
    auto r=web::open(std::filesystem::path(PBRT4_ROOT)/"scenes/cornell-box/scene-v4.pbrt");
    check(r.renderable && !has_errors(r.diagnostics),"Cornell adaptation");check(r.primitives.size()==36 && r.instances.size()==8 && r.emitters.size()==2,"Cornell expected counts");
    check(r.tlas_root<r.nodes.size() && r.max_depth<60,"BVH root/depth");
    for(auto& n:r.nodes){check(n.count || n.left+1<r.nodes.size(),"BVH child bounds");for(int a=0;a<3;++a)check(n.lo[a]<=n.hi[a],"valid bounds");}
    auto source=load_string(R"(Integrator "path" PixelFilter "box" WorldBegin ObjectBegin "o" Shape "trianglemesh" "point3 P" [0 0 0 1 0 0 0 1 0] ObjectEnd Translate 2 0 0 ObjectInstance "o" Translate 3 0 0 ObjectInstance "o")");
    r=web::build(source.scene);check(r.renderable && r.primitives.size()==1 && r.instances.size()==2,"shared instances are not flattened");check(r.instances[0].info[0]==r.instances[1].info[0],"shared BLAS");
    for(const char* options:{R"(Material "coatedconductor")",R"(Camera "perspective" "float lensradius" [1])",R"(PixelFilter "gaussian")",R"(Material "diffuse" "float roughness" [0.5])",R"(AreaLightSource "diffuse" "blackbody L" [6500])"}){
        const bool world=std::string_view(options).starts_with("Material") || std::string_view(options).starts_with("AreaLightSource");
        auto bad=load_string(std::string("Integrator \"path\" PixelFilter \"box\" ")+(world?" WorldBegin ":"")+options+(world?"":" WorldBegin"));
        check(static_cast<bool>(bad),"valid unsupported-feature fixture");
        if(web::build(bad.scene).renderable)throw std::runtime_error(std::string("Unsupported feature silently rendered: ")+options);
    }
    std::cout<<"PASS GPU layout, Cornell counts, BVH and shared BLAS contracts\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
