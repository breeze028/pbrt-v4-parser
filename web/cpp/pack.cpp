#include "scene.h"
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=3){std::cerr<<"Usage: pbrt4_web_pack scene.pbrt output-directory\n";return 2;}
    auto r=pbrt4::web::open(pbrt4::path_from_utf8(argv[1]));std::filesystem::create_directories(argv[2]);
    std::ofstream(std::filesystem::path(argv[2])/"metadata.json")<<r.metadata;
    for(unsigned k=0;k<5;++k){std::ofstream out(std::filesystem::path(argv[2])/("buffer"+std::to_string(k)+".bin"),std::ios::binary);out.write(static_cast<const char*>(pbrt4::web::data(r,k)),pbrt4::web::bytes(r,k));}
    for(auto& d:r.diagnostics)std::cerr<<pbrt4::format(d)<<'\n';
    std::cout<<r.primitives.size()<<" triangles, "<<r.instances.size()<<" shared placements, "<<r.nodes.size()<<" BVH nodes\n";
    return pbrt4::has_errors(r.diagnostics)?1:0;
}
