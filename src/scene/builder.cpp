// SPDX-License-Identifier: Apache-2.0
#include <pbrt4/pbrt4.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <set>
#include <utility>

namespace pbrt4 {
namespace {
std::string text(const Directive& d,std::size_t i=0) { return std::get<std::string>(d.arguments.at(i)); }
double num(const Directive& d,std::size_t i) { return std::get<double>(d.arguments.at(i)); }
struct State {
    AnimatedTransform transform;
    unsigned active = 3;
    Id material = 0, area_light = invalid_id;
    std::string material_name, inside, outside, color_space = "srgb";
    bool reverse = false;
    std::map<std::string,std::vector<Parameter>> attributes;
};
struct StackEntry { State state; char kind; SourceLocation location; };
struct ImportFrame {
    State state;
    std::vector<StackEntry> stack;
    std::map<std::string,AnimatedTransform> coords;
    Id object;
    std::size_t scope;
};
}
struct SceneBuilder::Impl {
    ParseResult result;
    LoadOptions options;
    State state;
    bool world = false, finished = false;
    std::vector<StackEntry> stack;
    std::map<std::string,AnimatedTransform> coords;
    Id object = invalid_id;
    std::size_t scope = 0;
    std::vector<std::size_t> parents{0};
    std::vector<ImportFrame> imports;
    explicit Impl(LoadOptions o) : options(std::move(o)) {
        auto& s=result.scene;
        s.camera.type="perspective"; s.film.type="rgb"; s.sampler.type="zsobol";
        s.integrator.type="volpath"; s.accelerator.type="bvh"; s.pixel_filter.type="gaussian";
        Entity m; m.type="diffuse"; s.materials.push_back(std::move(m));
    }
    void error(SourceLocation l,std::string code,std::string msg) { result.diagnostics.push_back({Severity::error,std::move(code),std::move(msg),std::move(l)}); }
    void apply(const Matrix4& m,bool replace=false) {
        if (state.active&1) state.transform.start=replace ? m : state.transform.start*m;
        if (state.active&2) state.transform.end=replace ? m : state.transform.end*m;
    }
    Entity entity(const Directive& d,std::string category) {
        Entity e; e.type=text(d); e.location=d.location; e.parameters=d.parameters;
        e.transform=state.transform; e.color_space=state.color_space; e.scope=scope;
        if (!category.empty()) e.inherited_parameters=state.attributes[category];
        for (auto& p:e.parameters) p.color_space=state.color_space;
        return e;
    }
    void pop(char kind,const SourceLocation& l) {
        if (stack.empty() || stack.back().kind!=kind) { error(l,"scope.mismatch","Unmatched or incorrectly nested scope end"); return; }
        state=std::move(stack.back().state); stack.pop_back();
    }
    bool require(bool condition,const Directive& d,const char* message) {
        if (!condition) error(d.location,"scene.phase",d.keyword+": "+message);
        return condition;
    }
    bool in_imported_object() const { return !imports.empty() && imports.back().object!=invalid_id && object==imports.back().object; }
    void command(const Directive& d) {
        result.scene.directives.push_back(d);
        const auto& k=d.keyword; auto& s=result.scene;
        static const std::set<std::string> options_only={"Camera","Film","Sampler","Integrator","Accelerator","PixelFilter","TransformTimes","Option"};
        static const std::set<std::string> world_only={"Shape","Material","MakeNamedMaterial","NamedMaterial","Texture","LightSource","AreaLightSource","Attribute","AttributeBegin","AttributeEnd","TransformBegin","TransformEnd","ObjectBegin","ObjectEnd","ObjectInstance","ReverseOrientation","Import"};
        if (options_only.contains(k) && !require(!world,d,"only valid before WorldBegin")) return;
        if (world_only.contains(k) && !require(world,d,"requires WorldBegin")) return;
        if (k=="Include" || k=="Import") return;
        if (k=="WorldBegin") {
            if (!require(!world,d,"duplicate WorldBegin")) return;
            world=true; state.transform.start={}; state.transform.end={}; state.active=3;
            coords["world"]=state.transform; return;
        }
        if (k=="AttributeBegin" || k=="TransformBegin") { stack.push_back({state,'a',d.location}); return; }
        if (k=="AttributeEnd" || k=="TransformEnd") { pop('a',d.location); return; }
        if (k=="Identity") { apply({},true); return; }
        if (k=="Translate" || k=="Scale") {
            Matrix4 m; if (k=="Translate") { m.m[3]=num(d,0); m.m[7]=num(d,1); m.m[11]=num(d,2); }
            else { m.m[0]=num(d,0); m.m[5]=num(d,1); m.m[10]=num(d,2); }
            apply(m); return;
        }
        if (k=="Rotate") {
            Vec3 a{num(d,1),num(d,2),num(d,3)};
            if (dot(a,a)==0) { error(d.location,"transform.axis","Rotation axis is zero"); return; }
            a=normalize(a); double angle=num(d,0)*std::numbers::pi/180, c=std::cos(angle),t=1-c,v=std::sin(angle);
            Matrix4 m; m.m={t*a.x*a.x+c,t*a.x*a.y-v*a.z,t*a.x*a.z+v*a.y,0,
                t*a.x*a.y+v*a.z,t*a.y*a.y+c,t*a.y*a.z-v*a.x,0,
                t*a.x*a.z-v*a.y,t*a.y*a.z+v*a.x,t*a.z*a.z+c,0, 0,0,0,1};
            apply(m); return;
        }
        if (k=="Transform" || k=="ConcatTransform") {
            Matrix4 m; for (int r=0;r<4;++r) for (int c=0;c<4;++c) m.m[r*4+c]=num(d,c*4+r);
            apply(m,k=="Transform"); return;
        }
        if (k=="LookAt") {
            Vec3 p{num(d,0),num(d,1),num(d,2)}, to{num(d,3),num(d,4),num(d,5)}, up{num(d,6),num(d,7),num(d,8)};
            Vec3 z=normalize(to-p), x=normalize(cross(normalize(up),z)), y=cross(z,x);
            if (dot(z,z)==0 || dot(x,x)==0) { error(d.location,"transform.lookat","Degenerate LookAt basis"); return; }
            Matrix4 m; m.m={x.x,y.x,z.x,p.x,x.y,y.y,z.y,p.y,x.z,y.z,z.z,p.z,0,0,0,1};
            apply(*inverse(m)); return;
        }
        if (k=="ActiveTransform") { auto a=text(d); state.active=a=="All" ? 3 : a=="StartTime" ? 1 : 2; return; }
        if (k=="TransformTimes") {
            if (num(d,0)>=num(d,1)) { error(d.location,"transform.times","Start time must precede end time"); return; }
            state.transform.start_time=num(d,0); state.transform.end_time=num(d,1); return;
        }
        if (k=="CoordinateSystem") { coords[text(d)]=state.transform; return; }
        if (k=="CoordSysTransform") {
            auto i=coords.find(text(d));
            if (i==coords.end()) error(d.location,"reference.coordinate","Unknown coordinate system: "+text(d));
            else state.transform=i->second;
            return;
        }
        if (k=="ColorSpace") {
            static const std::set<std::string> spaces={"srgb","rec2020","dci-p3","aces2065-1"};
            if (!spaces.contains(text(d))) error(d.location,"scene.colorspace","Unknown color space: "+text(d));
            else state.color_space=text(d);
            return;
        }
        if (k=="ReverseOrientation") { state.reverse=!state.reverse; return; }
        if (k=="MediumInterface") { state.inside=text(d); state.outside=text(d,1); return; }
        if (k=="Option") { s.options.push_back(d); return; }
        if (k=="Attribute") {
            static const std::set<std::string> targets={"shape","light","material","medium","texture"};
            if (!targets.contains(text(d))) { error(d.location,"attribute.target","Unknown Attribute target: "+text(d)); return; }
            for (auto p:d.parameters) { p.color_space=state.color_space; state.attributes[text(d)].push_back(std::move(p)); } return;
        }
        if (k=="ObjectBegin") {
            if (object!=invalid_id) { error(d.location,"object.nesting","ObjectBegin inside object definition"); return; }
            stack.push_back({state,'o',d.location}); object=s.objects.size(); s.objects.push_back({text(d),{},d.location,scope}); return;
        }
        if (k=="ObjectEnd") {
            if (object==invalid_id || in_imported_object() || stack.empty() || stack.back().kind!='o') { error(d.location,"object.scope","Unmatched ObjectEnd or attempted closing inherited object"); return; }
            pop('o',d.location); object=invalid_id; return;
        }
        if (k=="ObjectInstance") {
            if (object!=invalid_id) { error(d.location,"object.nesting","ObjectInstance inside definition is not allowed by v4"); return; }
            s.instances.push_back({invalid_id,text(d),state.transform,d.location,scope}); return;
        }
        if (k=="Camera") {
            auto e=entity(d,""); auto a=inverse(e.transform.start), b=inverse(e.transform.end);
            if (!a || !b) { error(d.location,"transform.singular","Camera transform is singular"); return; }
            e.transform.start=*a; e.transform.end=*b; coords["camera"]=e.transform;
            e.references.push_back({ReferenceKind::medium,"outside",state.outside,invalid_id,d.location});
            s.camera=std::move(e); return;
        }
        if (k=="Film") { s.film=entity(d,""); return; }
        if (k=="Sampler") { s.sampler=entity(d,""); return; }
        if (k=="Integrator") { s.integrator=entity(d,""); return; }
        if (k=="Accelerator") { s.accelerator=entity(d,""); return; }
        if (k=="PixelFilter") { s.pixel_filter=entity(d,""); return; }
        if (k=="NamedMaterial") { state.material_name=text(d); state.material=invalid_id; return; }
        if (k=="Material" || k=="MakeNamedMaterial") {
            auto e=entity(d,"material");
            if (k=="MakeNamedMaterial") {
                e.name=text(d); e.type=string_value(e,"type").value_or("");
                if (e.type.empty()) error(d.location,"material.type","MakeNamedMaterial requires string type");
            } else { state.material=s.materials.size(); state.material_name.clear(); }
            s.materials.push_back(std::move(e)); return;
        }
        if (k=="Texture") {
            auto e=entity(d,"texture"); e.name=text(d); e.value_type=text(d,1); e.type=text(d,2);
            if (e.value_type!="float" && e.value_type!="spectrum") error(d.location,"texture.type","Texture value type must be float or spectrum");
            s.textures.push_back(std::move(e)); return;
        }
        if (k=="MakeNamedMedium") {
            auto e=entity(d,"medium"); e.name=text(d); e.type=string_value(e,"type").value_or("");
            if (e.type.empty()) error(d.location,"medium.type","MakeNamedMedium requires string type");
            s.media.push_back(std::move(e)); return;
        }
        if (k=="LightSource") {
            auto e=entity(d,"light"); e.references.push_back({ReferenceKind::medium,"outside",state.outside,invalid_id,d.location});
            if (object!=invalid_id) result.diagnostics.push_back({Severity::warning,"light.object","Non-area lights are world lights even inside object definitions",d.location});
            s.lights.push_back(std::move(e)); return;
        }
        if (k=="AreaLightSource") {
            if (text(d).empty() || text(d)=="none") state.area_light=invalid_id;
            else { state.area_light=s.area_lights.size(); s.area_lights.push_back(entity(d,"light")); } return;
        }
        if (k=="Shape") {
            Shape e; static_cast<Entity&>(e)=entity(d,"shape"); e.material=state.material;
            e.material_name=state.material_name; e.area_light=state.area_light; e.reverse_orientation=state.reverse;
            e.inside_medium=state.inside; e.outside_medium=state.outside;
            if (!inverse(e.transform.start) || !inverse(e.transform.end)) error(d.location,"transform.singular","Shape transform is singular");
            auto id=s.shapes.size(); s.shapes.push_back(std::move(e));
            if (object==invalid_id) s.world_shapes.push_back(id); else s.objects[object].shapes.push_back(id);
            return;
        }
        error(d.location,"scene.directive","No semantics implemented for "+k);
    }
    bool visible(std::size_t declared,std::size_t from) const {
        while (true) { if (declared==from) return true; if (!from) return false; from=parents[from]; }
    }
    template<class T> Id lookup(const std::vector<T>& entities,const std::string& name,std::size_t from,const SourceLocation& loc,std::string kind,std::string value_type={}) {
        if (name.empty()) return invalid_id;
        for (Id i=0;i<entities.size();++i) {
            if constexpr (std::is_same_v<T,Entity>) { if (!value_type.empty() && entities[i].value_type!=value_type) continue; }
            if (entities[i].name==name && visible(entities[i].scope,from)) return i;
        }
        error(loc,"reference."+kind,"Undefined or Import-inaccessible "+kind+": "+name); return invalid_id;
    }
    template<class T> void duplicates(const std::vector<T>& entities,const char* category) {
        std::set<std::pair<std::string,std::string>> names;
        for (const auto& e:entities) if (!e.name.empty()) {
            std::string subtype; if constexpr(std::is_same_v<T,Entity>) subtype=e.value_type;
            if (!names.insert({e.name,subtype}).second) error(e.location,"reference.duplicate",std::string("Duplicate ")+category+": "+e.name);
        }
    }
    void resolve_entity(Entity& e) {
        // Only effective parameters create semantic references; shadowed declarations stay intact.
        std::map<std::string,const Parameter*> effective;
        for (const auto& p:e.inherited_parameters) effective[p.name]=&p;
        for (const auto& p:e.parameters) effective[p.name]=&p;
        for (const auto& [name,p]:effective) {
            if (p->type=="texture") {
                auto texture_name=std::get<std::string>(p->values[0]);
                Id a=invalid_id,b=invalid_id;
                for (Id i=0;i<result.scene.textures.size();++i) {
                    auto& t=result.scene.textures[i];
                    if (t.name==texture_name && visible(t.scope,e.scope)) { if (t.value_type=="float") a=i; else b=i; }
                }
                if (a==invalid_id && b==invalid_id) error(p->location,"reference.texture","Undefined or Import-inaccessible texture: "+texture_name);
                e.references.push_back({ReferenceKind::texture,name,texture_name,a==invalid_id ? b : a,p->location,a==invalid_id ? invalid_id : b});
            }
            if (p->type=="string" || p->type=="spectrum") for (const auto& v:p->values) if (const auto* str=std::get_if<std::string>(&v)) {
                if (p->type=="spectrum" || name=="filename" || name.ends_with("filename") || name=="mapname") {
                    auto path=std::filesystem::path(*str);
                    e.resources.push_back({name,*str,(path.is_absolute() ? path : result.scene.base_directory/path).lexically_normal(),p->location,p->type=="spectrum"});
                }
            }
        }
        for (auto& ref:e.references) if (ref.kind==ReferenceKind::medium) ref.id=lookup(result.scene.media,ref.name,e.scope,ref.location,"medium");
        if (e.type=="mix") {
            auto p=find_parameter(e,"materials");
            if (p && p->type=="string") for (const auto& v:p->values) {
                auto name=std::get<std::string>(v); auto id=lookup(result.scene.materials,name,e.scope,p->location,"material");
                e.references.push_back({ReferenceKind::material,"materials",name,id,p->location});
            }
        }
    }
    void finish() {
        if (finished) return;
        finished=true;
        if (!world) error({},"scene.world","Missing WorldBegin");
        for (const auto& x:stack) error(x.location,"scope.unclosed","Unclosed Attribute/Object scope");
        if (!imports.empty()) error({},"scope.import","Import interrupted by syntax or file error");
        auto& s=result.scene;
        duplicates(s.materials,"material"); duplicates(s.textures,"texture"); duplicates(s.media,"medium"); duplicates(s.objects,"object");
        resolve_entity(s.camera); resolve_entity(s.film); resolve_entity(s.sampler); resolve_entity(s.integrator); resolve_entity(s.accelerator); resolve_entity(s.pixel_filter);
        for (auto* list:{&s.materials,&s.textures,&s.media,&s.lights,&s.area_lights}) for (auto& e:*list) resolve_entity(e);
        for (auto& shape:s.shapes) {
            resolve_entity(shape);
            if (!shape.material_name.empty()) shape.material=lookup(s.materials,shape.material_name,shape.scope,shape.location,"material");
            shape.inside_medium_id=lookup(s.media,shape.inside_medium,shape.scope,shape.location,"medium");
            shape.outside_medium_id=lookup(s.media,shape.outside_medium,shape.scope,shape.location,"medium");
        }
        for (auto& i:s.instances) i.object=lookup(s.objects,i.object_name,i.scope,i.location,"object");
    }
};
SceneBuilder::SceneBuilder(const LoadOptions& o) : impl_(std::make_unique<Impl>(o)) {}
SceneBuilder::~SceneBuilder()=default;
void SceneBuilder::on_directive(const Directive& d) { impl_->command(d); }
void SceneBuilder::on_file_begin(const FileEvent& f) {
    auto& b=*impl_;
    if (f.kind==FileKind::root) {
        b.result.scene.base_directory=b.options.base_directory.empty() ? f.path.parent_path() : std::filesystem::absolute(b.options.base_directory).lexically_normal();
    } else if (f.kind==FileKind::import) {
        if (!b.world) b.error(f.location,"scene.import","Import requires WorldBegin");
        b.imports.push_back({b.state,std::move(b.stack),std::move(b.coords),b.object,b.scope});
        b.stack.clear(); b.coords.clear(); b.scope=b.parents.size(); b.parents.push_back(b.imports.back().scope);
    }
}
void SceneBuilder::on_file_end(const FileEvent& f) {
    if (f.kind!=FileKind::import) return;
    auto& b=*impl_;
    for (const auto& e:b.stack) b.error(e.location,"scope.import","Unclosed scope inside Import");
    auto frame=std::move(b.imports.back()); b.imports.pop_back();
    b.state=std::move(frame.state); b.stack=std::move(frame.stack); b.coords=std::move(frame.coords); b.object=frame.object; b.scope=frame.scope;
}
ParseResult SceneBuilder::finish(std::vector<Diagnostic> d) {
    impl_->finish(); auto r=std::move(impl_->result);
    r.diagnostics.insert(r.diagnostics.begin(),std::make_move_iterator(d.begin()),std::make_move_iterator(d.end())); return r;
}
ParseResult load(const std::filesystem::path& p,const LoadOptions& o) {
    SceneBuilder b(o); auto syntax=parse_file(p,b,o); return b.finish(std::move(syntax.diagnostics));
}
ParseResult load_string(std::string_view s,std::string f,const LoadOptions& o) {
    SceneBuilder b(o); auto syntax=parse_string(s,b,std::move(f),o); return b.finish(std::move(syntax.diagnostics));
}
}
