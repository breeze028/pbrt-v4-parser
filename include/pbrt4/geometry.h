// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <pbrt4/pbrt4.h>
namespace pbrt4 {
struct TriangleMesh {
    std::vector<Vec3> positions, normals;
    std::vector<Vec2> texcoords;
    std::vector<std::uint32_t> indices;
    Id source_shape = invalid_id;
};
struct MeshResult {
    TriangleMesh mesh;
    std::vector<Diagnostic> diagnostics;
    explicit operator bool() const { return !has_errors(diagnostics); }
};
struct GeometryOptions {
    bool tessellate_spheres = false;
    unsigned sphere_segments = 32, sphere_rings = 16;
    std::size_t max_vertices = 10'000'000, max_indices = 60'000'000;
    LoadOptions resources; // Virtual filesystem and file size policy, including binary PLY.
};
struct MeshInstance {
    Id mesh = invalid_id, source_shape = invalid_id;
    Id material = invalid_id, area_light = invalid_id;
    AnimatedTransform transform;
    bool reverse_orientation = false;
};
struct GeometryResult {
    std::vector<TriangleMesh> meshes; // Shared object-space geometry, once per used Shape.
    std::vector<MeshInstance> instances;
    std::vector<Diagnostic> diagnostics;
    explicit operator bool() const { return !has_errors(diagnostics); }
};
MeshResult load_ply(const std::filesystem::path&, const GeometryOptions& = {});
MeshResult extract_triangle_mesh(const Shape&, const GeometryOptions& = {});
GeometryResult extract_geometry(const Scene&, const GeometryOptions& = {});
// Explicit static baking; animated transforms are rejected instead of silently sampling t=0.
MeshResult bake_instance(const TriangleMesh&, const MeshInstance&);
}
