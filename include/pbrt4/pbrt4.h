// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <pbrt4/parser.h>
#include <array>
#include <limits>
#include <memory>

namespace pbrt4 {
using Id = std::size_t;
inline constexpr Id invalid_id = std::numeric_limits<Id>::max();
struct Vec2 { double x = 0, y = 0; };
struct Vec3 { double x = 0, y = 0, z = 0; };
Vec3 operator+(Vec3, Vec3);
Vec3 operator-(Vec3, Vec3);
Vec3 operator-(Vec3);
Vec3 operator*(Vec3, double);
Vec3 operator*(Vec3, Vec3);
Vec3 operator/(Vec3, double);
double dot(Vec3, Vec3);
Vec3 cross(Vec3, Vec3);
Vec3 normalize(Vec3);
struct Matrix4 {
    // Row-major storage, column vectors. Translation occupies m[3], m[7], m[11].
    std::array<double,16> m{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    bool operator==(const Matrix4&) const = default;
};
Matrix4 operator*(const Matrix4&, const Matrix4&);
std::optional<Matrix4> inverse(const Matrix4&);
Vec3 transform_point(const Matrix4&, Vec3);
Vec3 transform_vector(const Matrix4&, Vec3);
Vec3 transform_normal(const Matrix4&, Vec3);
double determinant3(const Matrix4&);
struct AnimatedTransform {
    Matrix4 start, end;
    double start_time = 0, end_time = 1;
    bool animated() const { return start != end; }
};
enum class ReferenceKind { material, texture, medium, object };
struct NamedReference {
    ReferenceKind kind;
    std::string parameter, name;
    Id id = invalid_id;
    SourceLocation location;
    // Texture references can be resolved in two separate namespaces.
    Id alternate_id = invalid_id;
};
struct ResourceReference {
    std::string parameter, literal;
    std::filesystem::path resolved;
    SourceLocation location;
    bool spectrum_name_or_file = false;
};
struct Entity {
    std::string type, name, value_type, color_space = "srgb";
    std::vector<Parameter> parameters; // Exactly as declared, with color-space annotations.
    std::vector<Parameter> inherited_parameters; // Attribute defaults; declaration wins.
    SourceLocation location;
    AnimatedTransform transform;
    std::vector<NamedReference> references;
    std::vector<ResourceReference> resources;
    std::size_t scope = 0; // Import visibility branch; not an Attribute scope.
};
struct Shape : Entity {
    Id material = 0, area_light = invalid_id;
    std::string material_name, inside_medium, outside_medium;
    Id inside_medium_id = invalid_id, outside_medium_id = invalid_id;
    bool reverse_orientation = false;
};
struct ObjectDefinition {
    std::string name;
    std::vector<Id> shapes; // Indices into Scene::shapes, no geometry copies.
    SourceLocation location;
    std::size_t scope = 0;
};
struct Instance {
    Id object = invalid_id;
    std::string object_name;
    AnimatedTransform transform;
    SourceLocation location;
    std::size_t scope = 0;
};
struct Scene {
    Entity camera, film, sampler, integrator, accelerator, pixel_filter;
    std::vector<Directive> options;
    std::vector<Shape> shapes;
    std::vector<Id> world_shapes;
    std::vector<ObjectDefinition> objects;
    std::vector<Instance> instances;
    std::vector<Entity> materials, textures, lights, area_lights, media;
    std::vector<Directive> directives; // Original syntax events including Include/Import.
    std::filesystem::path base_directory;
};
const Parameter* find_parameter(const std::vector<Parameter>&, std::string_view name);
const Parameter* find_parameter(const Entity&, std::string_view name);
std::optional<double> scalar(const Entity&, std::string_view name);
std::optional<std::string> string_value(const Entity&, std::string_view name);
struct ParseResult {
    Scene scene;
    std::vector<Diagnostic> diagnostics;
    explicit operator bool() const { return !has_errors(diagnostics); }
};
class SceneBuilder : public ParserTarget {
public:
    explicit SceneBuilder(const LoadOptions& = {});
    ~SceneBuilder() override;
    void on_directive(const Directive&) override;
    void on_file_begin(const FileEvent&) override;
    void on_file_end(const FileEvent&) override;
    ParseResult finish(std::vector<Diagnostic> syntax_diagnostics = {});
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
ParseResult load(const std::filesystem::path&, const LoadOptions& = {});
ParseResult load_string(std::string_view, std::string filename = "<memory>", const LoadOptions& = {});
}
