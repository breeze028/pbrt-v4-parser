// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <pbrt4/geometry.h>
#include <cstddef>
#include "image.h"
namespace pbrt4::web {
inline constexpr std::uint32_t none=0xffffffffu, layout_version=2;
struct alignas(16) Vertex { float position[4]{},normal[4]{},uv[4]{}; };
struct alignas(16) Primitive { std::uint32_t vertices[3]{},shape=0; };
struct alignas(16) Node { float lo[3]{}; std::uint32_t left=0; float hi[3]{}; std::uint32_t count=0; };
// Leaf: left=first item,count>0. Inner: left and left+1 are children,count=0.
struct alignas(16) Material { float color[4]{},eta_roughness[4]{},k_ior[4]{}; std::uint32_t info[4]{}; };
struct alignas(16) Instance { float world[16]{},local[16]{},emission[4]{}; std::uint32_t info[4]{}; };
struct alignas(16) Emitter { std::uint32_t instance=0,primitive=0,pad[2]{}; };
struct alignas(16) TextureRecord {std::uint32_t info[4]{};float uv[4]{1,1,0,0};};
struct alignas(16) Alpha {float value=1;std::uint32_t texture=none,pad[2]{};};
struct Image {std::string path,role;bool srgb=false;std::vector<Mip> levels;};
static_assert(sizeof(Vertex)==48 && sizeof(Primitive)==16 && sizeof(Node)==32);
static_assert(sizeof(Material)==64 && sizeof(Instance)==160 && sizeof(Emitter)==16);
static_assert(sizeof(TextureRecord)==32 && sizeof(Alpha)==16);
static_assert(offsetof(Node,left)==12 && offsetof(Node,count)==28 && offsetof(Instance,info)==144);
struct RenderScene {
    std::vector<Vertex> vertices;
    std::vector<Primitive> primitives;
    std::vector<Node> nodes;
    std::vector<Material> materials;
    std::vector<Instance> instances;
    std::vector<Emitter> emitters;
    std::vector<std::uint32_t> records;
    std::array<float,4> environment{};
    std::vector<TextureRecord> textures;
    std::vector<Image> images;
    std::vector<std::uint8_t> image_pixels;
    std::vector<Diagnostic> diagnostics;
    std::uint32_t tlas_root=none,max_depth=0;
    bool renderable=true;
    std::string metadata;
};
RenderScene build(const Scene&,const LoadOptions& = {});
RenderScene open(const std::filesystem::path&);
const void* data(const RenderScene&,unsigned kind);
std::size_t bytes(const RenderScene&,unsigned kind);
}
