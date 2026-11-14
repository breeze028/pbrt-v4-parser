#include <pbrt4/pbrt4.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <future>
#include <fstream>
#include <chrono>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
using namespace pbrt4;
void check(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
void near(double a,double b) { check(std::abs(a-b)<1e-9,"numeric expectation"); }
void good(const ParseResult& r) {
    if (!r) { for (auto& d:r.diagnostics) std::cerr<<format(d)<<'\n'; throw std::runtime_error("expected valid scene"); }
}
bool code(const std::vector<Diagnostic>& d,std::string_view c) { return std::any_of(d.begin(),d.end(),[&](auto& x){return x.code==c;}); }
struct Collector : ParserTarget {
    std::vector<Directive> events;
    std::vector<FileKind> begin,end;
    void on_directive(const Directive& d) override { events.push_back(d); }
    void on_file_begin(const FileEvent& f) override { begin.push_back(f.kind); }
    void on_file_end(const FileEvent& f) override { end.push_back(f.kind); }
};
LoadOptions virtual_files(std::map<std::string,std::string> files) {
    LoadOptions o; o.base_directory=PBRT4_TESTDATA;
    o.read_file=[files=std::move(files)](const std::filesystem::path& p)->std::optional<std::string> {
        auto relative=p.lexically_relative(std::filesystem::path(PBRT4_TESTDATA)).generic_string();
        auto i=files.find(relative); if (i==files.end()) return {}; return i->second;
    }; return o;
}
const std::map<std::string,std::function<void()>> tests={
{"tokenizer",[]{
    std::vector<Diagnostic> d;
    Tokenizer t("\xef\xbb\xbf# comment\r\nShape\"sp\\\"here\"\t[1 -2e+3]", "file.pbrt",d);
    auto a=t.next(); check(a.text=="Shape","token spelling"); check(a.location.line==2 && a.location.column==1,"source location");
    auto b=t.next(); check(b.text=="sp\"here","escaped quote"); check(b.location.column==6,"adjacent string location");
    check(t.next().kind==TokenKind::left_bracket,"left bracket"); check(t.next().text=="1","number"); check(t.next().text=="-2e+3","exponent");
    check(t.next().kind==TokenKind::right_bracket,"right bracket"); check(t.next().kind==TokenKind::end,"EOF"); check(d.empty(),"valid lexing");
}},
{"parameters",[]{
    Collector c; auto r=parse_string(R"(Shape "custom" "integer ids" [+3 -4] "float f" .5 "bool enabled" [true false] "point2 uv" [0 1] "point3 P" [1 2 3] "vector2 a" [2 3] "vector3 b" [4 5 6] "normal3 N" [0 1 0] "rgb c" [0 .5 1] "spectrum s" [400 .2 700 .8] "spectrum named" "metal-Cu-eta" "blackbody L" 6500 "texture reflectance" "albedo" "string filename" "test.ply")",c);
    check(bool(r),"all v4 parameter types"); auto& p=c.events.at(0).parameters;
    check(p.size()==14,"parameter count"); check(std::get<std::int64_t>(p[0].values[0])==3,"integer type");
    check(std::get<bool>(p[2].values[0]),"boolean type"); check(p[9].type=="spectrum" && p[9].values.size()==4,"sampled spectrum preserved");
    check(std::get<std::string>(p[10].values[0])=="metal-Cu-eta","named spectrum preserved");
}},
{"all_directives",[]{
    const std::string source=R"(Option "bool disablepixeljitter" true
Identity Translate 1 2 3 Scale 2 2 2 Rotate 30 0 1 0 LookAt 0 0 -5 0 0 0 0 1 0
Transform [1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1]
ConcatTransform [1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1]
ActiveTransform All TransformTimes 0 1 CoordinateSystem "x" CoordSysTransform "x" ColorSpace "srgb"
PixelFilter "gaussian" Film "rgb" Sampler "independent" Integrator "path" Accelerator "bvh" Camera "perspective"
MakeNamedMedium "fog" "string type" "homogeneous" MediumInterface "fog"
WorldBegin AttributeBegin Attribute "shape" "float alpha" 1
Texture "x" "float" "constant" Material "diffuse" MakeNamedMaterial "m" "string type" "diffuse" NamedMaterial "m"
LightSource "infinite" AreaLightSource "diffuse" ReverseOrientation Shape "sphere" AttributeEnd
ObjectBegin "o" Shape "sphere" ObjectEnd ObjectInstance "o"
TransformBegin TransformEnd Include "empty.pbrt" Import "empty.pbrt")";
    auto o=virtual_files({{"empty.pbrt",""}}); Collector c; auto r=parse_string(source,c,"root.pbrt",o);
    check(bool(r),"full v4 syntax surface"); std::set<std::string> seen; for(auto& d:c.events) seen.insert(d.keyword);
    for(auto& k:supported_directives()) check(seen.contains(k),"keyword missing from corpus");
    check(c.begin.size()==3 && c.end.size()==3,"file boundary events"); check(code(r.diagnostics,"syntax.deprecated"),"deprecated warning");
}},
{"syntax_errors",[]{
    const std::vector<std::string> bad={"WorldEnd","UnknownDirective",R"(Shape "sphere" "point P" [0 0 0])",R"(Shape "sphere" "float x" "one")",
      R"(Shape "sphere" "bool x" "true")",R"(Shape "sphere" "integer x" 1.5)",R"(Shape "sphere" "integer x" 9223372036854775808)",
      R"(Shape "sphere" "rgb x" [1 2])",R"(Shape "sphere" "point2 x" [])",R"(Shape "sphere" "spectrum x" ["a" 1])",
      R"(Shape "sphere" "float x" [1 2)",R"(Shape "sphere" "float x" nan)",R"(Shape "sphere" "float x" 1e999)",
      R"(Shape "sphere" "float x" 1e)",R"(Shape "sphere" "texture x" ["a" "b"])","Translate 1 2", "Transform [1 2 3]",
      "ActiveTransform Bad",R"(Option "bool enabled" [true])",R"(Shape "sphere" "float too many names" 1)"};
    for(auto& s:bad) { Collector c; auto r=parse_string(s,c,"bad.pbrt"); check(!r,"invalid syntax must fail"); check(!r.diagnostics.empty(),"error diagnostic"); check(!r.diagnostics[0].location.filename.empty(),"error source"); }
}},
{"lexical_errors",[]{
    for(auto s:{std::string("Shape \"unterminated"),std::string("Shape \"new\nline\""),std::string("Shape \"bad\\q\""),std::string("WorldBegin\0Shape",16)}) {
        Collector c; auto r=parse_string(s,c); check(!r,"invalid lexing must fail");
    }
    Collector c; auto r=parse_string(std::string("Shape \"")+char(0xff)+"\"",c); check(!r && code(r.diagnostics,"lex.utf8"),"Malformed UTF-8 diagnostic");
}},
{"unicode_paths",[]{
    auto folder=std::filesystem::temp_directory_path()/std::filesystem::path("pbrt4-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(folder);
    auto root=folder/std::filesystem::path(u8"\u573a\u666f.pbrt"),nested=folder/std::filesystem::path(u8"\u6a21\u578b.pbrt");
    auto nested_name=path_to_utf8(nested.filename());
    { std::ofstream out(root); out<<"WorldBegin Include \""<<nested_name<<"\""; }
    { std::ofstream out(nested); out<<"Shape \"sphere\""; }
    auto r=load(root); std::filesystem::remove(root); std::filesystem::remove(nested); std::filesystem::remove(folder);
    good(r); check(r.scene.shapes[0].location.filename==path_to_utf8(nested),"UTF-8 filename roundtrip");
}},
{"transform",[]{
    auto r=load_string(R"(WorldBegin Translate 1 2 3 Scale 2 3 4 Shape "sphere")"); good(r);
    auto p=transform_point(r.scene.shapes[0].transform.start,{1,1,1}); near(p.x,3); near(p.y,5); near(p.z,7);
    r=load_string(R"(WorldBegin Transform [1 0 0 0 0 1 0 0 0 0 1 0 4 5 6 1] ConcatTransform [2 0 0 0 0 2 0 0 0 0 2 0 0 0 0 1] Shape "sphere")"); good(r);
    p=transform_point(r.scene.shapes[0].transform.start,{1,1,1}); near(p.x,6); near(p.y,7); near(p.z,8);
    r=load_string(R"(WorldBegin Rotate 90 0 0 1 Shape "sphere")"); good(r); p=transform_point(r.scene.shapes[0].transform.start,{1,0,0}); near(p.x,0); near(p.y,1);
    r=load_string(R"(WorldBegin Rotate 90 0 0 1e-200 Shape "sphere")"); good(r); p=transform_point(r.scene.shapes[0].transform.start,{1,0,0}); near(p.x,0); near(p.y,1);
}},
{"camera",[]{
    auto r=load_string(R"(LookAt 0 1 -5 0 1 0 0 1 0 Camera "perspective" WorldBegin Shape "sphere")"); good(r);
    auto eye=transform_point(r.scene.camera.transform.start,{}); near(eye.x,0); near(eye.y,1); near(eye.z,-5); near(r.scene.shapes[0].transform.start.m[11],0);
    r=load_string(R"(Translate 2 3 4 Camera "perspective" WorldBegin CoordSysTransform "camera" Shape "sphere")"); good(r);
    near(r.scene.shapes[0].transform.start.m[3],-2); near(r.scene.shapes[0].transform.start.m[7],-3);
}},
{"attributes",[]{
    auto r=load_string(R"(ColorSpace "rec2020" WorldBegin Attribute "shape" "float alpha" .2 Attribute "material" "rgb reflectance" [.1 .2 .3]
AttributeBegin Translate 1 0 0 ReverseOrientation Material "diffuse" AreaLightSource "diffuse" Shape "sphere" "float alpha" .8 AttributeEnd Shape "sphere")"); good(r);
    auto& a=r.scene.shapes[0]; auto& b=r.scene.shapes[1]; check(a.reverse_orientation && !b.reverse_orientation,"orientation scope");
    near(a.transform.start.m[3],1); near(b.transform.start.m[3],0); near(*scalar(a,"alpha"),.8); near(*scalar(b,"alpha"),.2);
    check(a.area_light!=invalid_id && b.area_light==invalid_id,"area light association"); check(a.material!=b.material,"material scope");
    check(a.color_space=="rec2020","WorldBegin preserves color space");
    check(find_parameter(r.scene.materials[a.material],"reflectance")->color_space=="rec2020","inherited color space");
}},
{"animation",[]{
    auto r=load_string(R"(TransformTimes .2 .8 WorldBegin ActiveTransform StartTime Translate 1 0 0 ActiveTransform EndTime Translate 2 0 0 Shape "sphere" CoordinateSystem "motion" ActiveTransform All Identity CoordSysTransform "motion" Shape "sphere")"); good(r);
    for(auto& s:r.scene.shapes) { check(s.transform.animated(),"animation retained"); near(s.transform.start.m[3],1); near(s.transform.end.m[3],2); near(s.transform.start_time,.2); near(s.transform.end_time,.8); }
}},
{"instances",[]{
    auto r=load_string(R"(WorldBegin Translate 2 0 0 ObjectBegin "ball" Translate 3 0 0 Shape "sphere" ObjectEnd Translate 5 0 0 ObjectInstance "ball" ObjectInstance "ball")"); good(r);
    check(r.scene.shapes.size()==1 && r.scene.world_shapes.empty(),"one shared shape"); check(r.scene.objects[0].shapes==std::vector<Id>{0},"object shape IDs");
    check(r.scene.instances.size()==2,"instance count"); near(r.scene.shapes[0].transform.start.m[3],5); near(r.scene.instances[0].transform.start.m[3],7);
    check(r.scene.instances[0].object==r.scene.instances[1].object,"shared object ID");
}},
{"references",[]{
    auto r=load_string(R"(WorldBegin NamedMaterial "red" Shape "sphere" MakeNamedMaterial "red" "string type" "diffuse" "texture reflectance" "t" Texture "t" "spectrum" "constant" "rgb value" [1 0 0])"); good(r);
    check(r.scene.shapes[0].material==1,"forward material reference"); check(r.scene.materials[1].references[0].id==0,"texture ID resolved");
    r=load_string(R"(WorldBegin AttributeBegin MakeNamedMaterial "m" "string type" "diffuse" AttributeEnd NamedMaterial "m" Shape "sphere")"); good(r);
    r=load_string(R"(WorldBegin Texture "t" "float" "constant" Texture "t" "spectrum" "constant" Material "diffuse" "texture reflectance" "t" Shape "sphere")"); good(r);
    check(r.scene.materials[1].references[0].alternate_id!=invalid_id,"texture namespaces retained");
}},
{"include",[]{
    auto o=virtual_files({{"sub/one.pbrt",R"(Translate 1 0 0 Include "two.pbrt")"},{"two.pbrt",R"(Translate 2 0 0 Shape "plymesh" "string filename" "mesh.ply")"}});
    auto r=load_string(R"(WorldBegin Include "sub/one.pbrt" Shape "sphere")","root.pbrt",o); good(r);
    near(r.scene.shapes[0].transform.start.m[3],3); near(r.scene.shapes[1].transform.start.m[3],3);
    check(r.scene.shapes[0].resources.at(0).resolved==std::filesystem::path(PBRT4_TESTDATA)/"mesh.ply","root-relative resource");
    check(r.scene.shapes[0].location.filename.ends_with("two.pbrt"),"nested source location");
}},
{"import",[]{
    auto o=virtual_files({{"i.pbrt",R"(Translate 10 0 0 ReverseOrientation Material "diffuse" Shape "sphere" MakeNamedMaterial "local" "string type" "diffuse" NamedMaterial "local" Shape "sphere")"}});
    auto r=load_string(R"(WorldBegin Translate 2 0 0 Import "i.pbrt" Shape "sphere")","root.pbrt",o); good(r);
    near(r.scene.shapes[0].transform.start.m[3],12); near(r.scene.shapes[2].transform.start.m[3],2);
    check(r.scene.shapes[0].reverse_orientation && !r.scene.shapes[2].reverse_orientation,"Import isolated state"); check(r.scene.shapes[2].material==0,"Import material doesn't leak");
    r=load_string(R"(WorldBegin Import "i.pbrt" NamedMaterial "local" Shape "sphere")","root.pbrt",o); check(!r && code(r.diagnostics,"reference.material"),"Import names not exported");
    o=virtual_files({{"i.pbrt",R"(Shape "sphere")"}});
    r=load_string(R"(WorldBegin ObjectBegin "o" Import "i.pbrt" ObjectEnd ObjectInstance "o")","root.pbrt",o); good(r);
    check(r.scene.objects[0].shapes.size()==1 && r.scene.world_shapes.empty(),"Import contributes to object");
}},
{"import_scope_errors",[]{
    for(auto source:{R"(AttributeBegin Shape "sphere")",R"(ObjectEnd)",R"(AttributeEnd)",R"(CoordSysTransform "outer")"}) {
        auto o=virtual_files({{"i.pbrt",source}});
        auto r=load_string(R"(WorldBegin CoordinateSystem "outer" ObjectBegin "o" Import "i.pbrt" ObjectEnd)","root.pbrt",o); check(!r,"Import cannot escape scope or inherit coordinate map");
    }
}},
{"medium_spectrum",[]{
    auto r=load_string(R"(MakeNamedMedium "fog" "string type" "homogeneous" "spectrum sigma_a" [400 .2 700 .5] MediumInterface "fog" Camera "perspective" WorldBegin Shape "sphere" LightSource "infinite" "blackbody L" 6500)"); good(r);
    check(r.scene.shapes[0].inside_medium_id==0 && r.scene.shapes[0].outside_medium_id==0,"medium IDs");
    check(r.scene.camera.references[0].id==0,"camera medium"); check(r.scene.lights[0].parameters[0].type=="blackbody","no RGB conversion");
}},
{"semantic_errors",[]{
    const std::vector<std::string> bad={R"(Shape "sphere")",R"(WorldBegin Film "rgb")",R"(WorldBegin WorldBegin)",R"(WorldBegin AttributeEnd)",
        R"(WorldBegin AttributeBegin)",R"(WorldBegin ObjectBegin "x" AttributeEnd)",R"(WorldBegin ObjectEnd)",R"(WorldBegin ObjectBegin "x" ObjectInstance "x" ObjectEnd)",
        R"(WorldBegin ObjectBegin "x" ObjectBegin "y" ObjectEnd)",R"(WorldBegin ObjectInstance "missing")",R"(WorldBegin NamedMaterial "missing" Shape "sphere")",
        R"(WorldBegin Material "diffuse" "texture reflectance" "missing" Shape "sphere")",R"(WorldBegin MakeNamedMaterial "m")",
        R"(WorldBegin MakeNamedMaterial "m" "string type" "diffuse" MakeNamedMaterial "m" "string type" "diffuse")",
        R"(WorldBegin MediumInterface "missing" Shape "sphere")",R"(WorldBegin CoordSysTransform "missing")",R"(WorldBegin Scale 0 1 1 Shape "sphere")",
        R"(WorldBegin Rotate 90 0 0 0)",R"(LookAt 0 0 0 0 0 1 0 0 1 Camera "perspective" WorldBegin)",R"(ColorSpace "unknown" WorldBegin)",
        R"(WorldBegin Attribute "wrong")",R"(WorldBegin Texture "a" "rgb" "constant")",R"(TransformTimes 1 0 WorldBegin)",
        R"(WorldBegin ObjectInstance "")",R"(WorldBegin NamedMaterial "" Shape "sphere")",R"(WorldBegin ObjectBegin "" ObjectEnd)"};
    for(auto& s:bad) { auto r=load_string(s); check(!r,"invalid scene must fail"); }
}},
{"file_errors",[]{
    auto o=virtual_files({{"a.pbrt",R"(Include "a.pbrt")"}});
    auto r=load_string(R"(WorldBegin Include "a.pbrt")","root.pbrt",o); check(!r && code(r.diagnostics,"file.cycle"),"cycle");
    r=load_string(R"(WorldBegin Include "missing")","root.pbrt",o); check(!r && code(r.diagnostics,"file.open"),"missing file");
    r=load_string(R"(WorldBegin Include "a.gz")","root.pbrt",o); check(!r && code(r.diagnostics,"file.compression"),"compression error");
    o.max_include_depth=1; r=load_string(R"(WorldBegin Include "a.pbrt")","root.pbrt",o); check(!r && code(r.diagnostics,"file.depth"),"depth limit");
    o.max_file_bytes=2; r=load_string("WorldBegin","root.pbrt",o); check(!r && code(r.diagnostics,"file.size"),"size limit");
}},
{"independent_loads",[]{
    std::vector<std::future<ParseResult>> pending;
    for(int i=0;i<12;++i) pending.push_back(std::async(std::launch::async,[i]{return load_string("WorldBegin Translate "+std::to_string(i)+" 0 0 Shape \"sphere\"");}));
    for(int i=0;i<12;++i) { auto r=pending[i].get(); good(r); near(r.scene.shapes[0].transform.start.m[3],i); }
}},
{"malformed_corpus",[]{
    std::mt19937 rng(42); std::string seed=R"(WorldBegin AttributeBegin Shape "sphere" "float radius" [1] AttributeEnd)";
    for(int i=0;i<1500;++i) { auto s=seed; auto start=rng()%s.size(); s.erase(start,rng()%(s.size()-start+1)); s.insert(rng()%(s.size()+1),1,static_cast<char>(rng()%128)); auto r=load_string(s); (void)r; }
}}
};
int main(int argc,char** argv) {
    int count=0;
    for(const auto& [name,run]:tests) if(argc==1 || name==argv[1]) {
        try { run(); std::cout<<"PASS "<<name<<'\n'; ++count; }
        catch(const std::exception& e) { std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n'; return 1; }
    }
    return count ? 0 : 2;
}
