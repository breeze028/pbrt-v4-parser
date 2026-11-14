#include <pbrt4/pbrt4.h>
#ifdef HAS_GEOMETRY
#include <pbrt4/geometry.h>
#endif
int main() {
    auto scene=pbrt4::load_string("WorldBegin Shape \"trianglemesh\" \"point3 P\" [0 0 0 1 0 0 0 1 0]");
    if(!scene || scene.scene.shapes.size()!=1) return 1;
#ifdef HAS_GEOMETRY
    auto geometry=pbrt4::extract_geometry(scene.scene);
    if(!geometry || geometry.meshes.size()!=1) return 2;
#endif
}
