// SPDX-License-Identifier: Apache-2.0
#include "scene.h"
#include <map>
namespace { std::map<unsigned,pbrt4::web::RenderScene> scenes;unsigned next=1; }
extern "C" {
unsigned p4_open(const char* path) {try {auto h=next++;scenes.emplace(h,pbrt4::web::open(pbrt4::path_from_utf8(path)));return h;}catch(...){return 0;}}
void p4_close(unsigned h) {scenes.erase(h);}
const char* p4_metadata(unsigned h) {auto i=scenes.find(h);return i==scenes.end()?"{}":i->second.metadata.c_str();}
const void* p4_data(unsigned h,unsigned k) {auto i=scenes.find(h);return i==scenes.end()?nullptr:pbrt4::web::data(i->second,k);}
unsigned p4_bytes(unsigned h,unsigned k) {auto i=scenes.find(h);return i==scenes.end()?0:static_cast<unsigned>(pbrt4::web::bytes(i->second,k));}
}
