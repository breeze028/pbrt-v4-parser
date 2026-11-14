// SPDX-License-Identifier: Apache-2.0
#include <pbrt4/parser.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <locale>
#include <sstream>
#include <unordered_map>

namespace pbrt4 {
bool has_errors(const std::vector<Diagnostic>& d) {
    return std::any_of(d.begin(), d.end(), [](const auto& x) { return x.severity == Severity::error; });
}
std::string format(const Diagnostic& d) {
    return d.location.filename + ":" + std::to_string(d.location.line) + ":" +
        std::to_string(d.location.column) + ": " + (d.severity == Severity::error ? "error" : "warning") +
        " [" + d.code + "] " + d.message;
}
Tokenizer::Tokenizer(std::string s, std::string f, std::vector<Diagnostic>& d)
    : input_(std::move(s)), location_{std::move(f), 1, 1}, diagnostics_(d) {
    if (input_.starts_with("\xef\xbb\xbf")) position_ = 3;
}
char Tokenizer::take() {
    char c = input_[position_++];
    if (c == '\n') { ++location_.line; location_.column = 1; }
    else ++location_.column;
    return c;
}
static bool space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
Token Tokenizer::next() {
    while (position_ < input_.size()) {
        if (space(input_[position_])) { take(); continue; }
        if (input_[position_] == '#') { while (position_ < input_.size() && take() != '\n') {} continue; }
        break;
    }
    auto loc = location_;
    if (position_ == input_.size()) return {TokenKind::end, {}, loc};
    char c = take();
    if (c == '[') return {TokenKind::left_bracket, "[", loc};
    if (c == ']') return {TokenKind::right_bracket, "]", loc};
    if (c == '"') {
        std::string s;
        while (position_ < input_.size()) {
            c = take();
            if (c == '"') return {TokenKind::string, std::move(s), loc};
            if (c == '\n' || c == '\r' || c == '\0') break;
            if (c == '\\') {
                if (position_ == input_.size()) break;
                c = take();
                switch (c) {
                case 'n': c = '\n'; break; case 'r': c = '\r'; break; case 't': c = '\t'; break;
                case 'b': c = '\b'; break; case 'f': c = '\f'; break;
                case '\\': case '"': case '\'': break;
                default: diagnostics_.push_back({Severity::error,"lex.escape","Invalid string escape",loc}); break;
                }
            }
            s += c;
        }
        diagnostics_.push_back({Severity::error,"lex.string","Unterminated string",loc});
        return {TokenKind::end, {}, loc};
    }
    std::string s(1,c);
    while (position_ < input_.size()) {
        c = input_[position_];
        if (space(c) || c == '[' || c == ']' || c == '"' || c == '#') break;
        s += take();
    }
    if (s.find('\0') != std::string::npos) diagnostics_.push_back({Severity::error,"lex.nul","NUL byte in input",loc});
    return {TokenKind::word, std::move(s), loc};
}
namespace {
// Numbers of string and numeric arguments; -1 denotes a typed parameter list.
struct Signature { int strings = 0, numbers = 0; bool params = false; };
const std::unordered_map<std::string,Signature>& signatures() {
    static const std::unordered_map<std::string,Signature> s = {
        {"Identity",{}},{"Translate",{0,3}},{"Scale",{0,3}},{"Rotate",{0,4}},{"LookAt",{0,9}},
        {"Transform",{0,16}},{"ConcatTransform",{0,16}},{"TransformTimes",{0,2}},
        {"CoordinateSystem",{1}},{"CoordSysTransform",{1}},{"ActiveTransform",{}},{"ColorSpace",{1}},
        {"Option",{}},{"WorldBegin",{}},{"AttributeBegin",{}},{"AttributeEnd",{}},
        {"TransformBegin",{}},{"TransformEnd",{}},{"ReverseOrientation",{}},
        {"ObjectBegin",{1}},{"ObjectEnd",{}},{"ObjectInstance",{1}},
        {"Include",{1}},{"Import",{1}},{"NamedMaterial",{1}},{"MediumInterface",{1}},
        {"Camera",{1,0,true}},{"Film",{1,0,true}},{"PixelFilter",{1,0,true}},
        {"Sampler",{1,0,true}},{"Integrator",{1,0,true}},{"Accelerator",{1,0,true}},
        {"Material",{1,0,true}},{"MakeNamedMaterial",{1,0,true}},{"MakeNamedMedium",{1,0,true}},
        {"Texture",{3,0,true}},{"Shape",{1,0,true}},{"LightSource",{1,0,true}},
        {"AreaLightSource",{1,0,true}},{"Attribute",{1,0,true}}
    }; return s;
}
struct Abort {};
struct Reader {
    Tokenizer lexer;
    std::optional<Token> buffered;
    Reader(std::string s, std::string f, std::vector<Diagnostic>& d) : lexer(std::move(s),std::move(f),d) {}
    Token peek() { if (!buffered) buffered = lexer.next(); return *buffered; }
    Token get() { auto t = peek(); buffered.reset(); return t; }
};
struct Parser {
    ParserTarget& target;
    LoadOptions options;
    SyntaxResult result;
    std::filesystem::path base;
    std::vector<std::filesystem::path> files;
    [[noreturn]] void fail(const SourceLocation& loc, std::string code, std::string message) {
        result.diagnostics.push_back({Severity::error,std::move(code),std::move(message),loc}); throw Abort{};
    }
    std::string quoted(Reader& r) {
        auto t = r.get(); if (t.kind != TokenKind::string) fail(t.location,"syntax.string","Expected quoted string"); return t.text;
    }
    double number(const Token& t) {
        if (t.kind != TokenKind::word || t.text.empty()) fail(t.location,"syntax.number","Expected finite decimal number");
        // Enforce decimal grammar independently of the host locale and iostream extensions.
        std::size_t i = 0;
        if (t.text[i] == '+' || t.text[i] == '-') ++i;
        std::size_t digits = 0;
        while (i < t.text.size() && t.text[i] >= '0' && t.text[i] <= '9') { ++i; ++digits; }
        if (i < t.text.size() && t.text[i] == '.') { ++i; while (i < t.text.size() && t.text[i] >= '0' && t.text[i] <= '9') { ++i; ++digits; } }
        if (!digits) fail(t.location,"syntax.number","Invalid decimal number: " + t.text);
        if (i < t.text.size() && (t.text[i] == 'e' || t.text[i] == 'E')) {
            ++i; if (i < t.text.size() && (t.text[i] == '+' || t.text[i] == '-')) ++i;
            auto start = i;
            while (i < t.text.size() && t.text[i] >= '0' && t.text[i] <= '9') ++i;
            if (i == start) fail(t.location,"syntax.number","Missing exponent digits");
        }
        if (i != t.text.size()) fail(t.location,"syntax.number","Invalid decimal number: " + t.text);
        std::istringstream in(t.text); in.imbue(std::locale::classic()); double v = 0; in >> v;
        if (!in || !std::isfinite(v)) fail(t.location,"syntax.number","Number out of range: " + t.text);
        return v;
    }
    Parameter parameter(Reader& r) {
        auto decl = r.get(); Parameter p; p.location = decl.location;
        std::istringstream declaration(decl.text); std::string extra;
        if (!(declaration >> p.type >> p.name) || (declaration >> extra)) fail(decl.location,"parameter.declaration","Expected \"type name\"");
        static const std::vector<std::string> types = {"integer","float","point2","vector2","point3","vector3","normal3","rgb","spectrum","blackbody","bool","string","texture"};
        if (std::find(types.begin(),types.end(),p.type) == types.end()) fail(p.location,"parameter.type","Unknown v4 parameter type: " + p.type);
        p.array = r.peek().kind == TokenKind::left_bracket;
        if (p.array) r.get();
        std::optional<bool> spectrum_string;
        do {
            if (p.array && r.peek().kind == TokenKind::right_bracket) break;
            auto t = r.get();
            if (p.type == "string" || p.type == "texture" || (p.type == "spectrum" && t.kind == TokenKind::string)) {
                if (t.kind != TokenKind::string) fail(t.location,"parameter.value","Expected quoted string");
                if (p.type == "spectrum") { if (spectrum_string == false) fail(t.location,"parameter.value","Mixed spectrum representations"); spectrum_string = true; }
                p.values.emplace_back(t.text);
            } else if (p.type == "bool") {
                if (t.kind != TokenKind::word || (t.text != "true" && t.text != "false")) fail(t.location,"parameter.value","Expected unquoted true or false");
                p.values.emplace_back(t.text == "true");
            } else if (p.type == "integer") {
                std::string_view s = t.text; if (s.starts_with('+')) s.remove_prefix(1);
                std::int64_t v = 0; auto [end,ec] = std::from_chars(s.data(),s.data()+s.size(),v);
                if (t.kind != TokenKind::word || ec != std::errc{} || end != s.data()+s.size()) fail(t.location,"parameter.value","Expected signed 64-bit integer");
                p.values.emplace_back(v);
            } else {
                if (p.type == "spectrum") { if (spectrum_string == true) fail(t.location,"parameter.value","Mixed spectrum representations"); spectrum_string = false; }
                p.values.emplace_back(number(t));
            }
        } while (p.array);
        if (p.array && r.get().kind != TokenKind::right_bracket) fail(p.location,"parameter.array","Unterminated array");
        std::size_t width = (p.type == "point2" || p.type == "vector2") ? 2 :
            (p.type == "point3" || p.type == "vector3" || p.type == "normal3" || p.type == "rgb") ? 3 :
            (p.type == "spectrum" && spectrum_string == false) ? 2 : 1;
        if (p.values.empty() || p.values.size() % width) fail(p.location,"parameter.arity","Invalid component count for " + p.type);
        if (p.type == "texture" && p.values.size() != 1) fail(p.location,"parameter.arity","Texture reference requires one name");
        return p;
    }
    std::filesystem::path resolve(const std::filesystem::path& p) { return (p.is_absolute() ? p : base / p).lexically_normal(); }
    std::string read(const FileEvent& event) {
        if (event.path.extension() == ".gz") fail(event.location,"file.compression","gzip is not enabled; decompress the scene before loading");
        std::optional<std::string> data;
        if (options.read_file) data = options.read_file(event.path);
        else {
            std::ifstream in(event.path, std::ios::binary | std::ios::ate);
            if (in) {
                auto length = in.tellg();
                if (length < 0 || static_cast<std::uintmax_t>(length) > options.max_file_bytes) fail(event.location,"file.size","File exceeds configured size limit");
                data.emplace(static_cast<std::size_t>(length),'\0'); in.seekg(0);
                if (!in.read(data->data(),static_cast<std::streamsize>(data->size()))) fail(event.location,"file.read","Failed reading " + event.path.string());
            }
        }
        if (!data) fail(event.location,"file.open","Cannot open " + event.path.string());
        if (data->size() > options.max_file_bytes) fail(event.location,"file.size","File exceeds configured size limit");
        return std::move(*data);
    }
    void file(const FileEvent& event, std::optional<std::string> contents = {}) {
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(event.path,ec);
        if (ec) canonical = event.path.lexically_normal();
        if (files.size() >= options.max_include_depth) fail(event.location,"file.depth","Include/Import depth limit exceeded");
        if (std::find(files.begin(),files.end(),canonical) != files.end()) fail(event.location,"file.cycle","Include/Import cycle: " + event.path.string());
        if (!contents) contents = read(event);
        if (contents->size() > options.max_file_bytes) fail(event.location,"file.size","Input exceeds configured size limit");
        files.push_back(canonical); target.on_file_begin(event);
        Reader r(std::move(*contents),event.path.string(),result.diagnostics);
        while (r.peek().kind != TokenKind::end) {
            auto t = r.get(); auto it = signatures().find(t.text);
            if (t.kind != TokenKind::word || it == signatures().end()) fail(t.location,"syntax.directive","Unknown PBRT-v4 directive: " + t.text);
            Directive d; d.keyword = t.text; d.location = t.location; auto sig = it->second;
            if (d.keyword == "TransformBegin" || d.keyword == "TransformEnd")
                result.diagnostics.push_back({Severity::warning,"syntax.deprecated","Deprecated transform scope has Attribute scope semantics",t.location});
            for (int i = 0; i < sig.strings; ++i) d.arguments.emplace_back(quoted(r));
            bool matrix = d.keyword == "Transform" || d.keyword == "ConcatTransform";
            if (matrix && r.get().kind != TokenKind::left_bracket) fail(t.location,"syntax.matrix","Expected '[' before 16 matrix values");
            for (int i = 0; i < sig.numbers; ++i) d.arguments.emplace_back(number(r.get()));
            if (matrix && r.get().kind != TokenKind::right_bracket) fail(t.location,"syntax.matrix","Expected ']' after 16 matrix values");
            if (d.keyword == "ActiveTransform") {
                auto v = r.get();
                if (v.kind != TokenKind::word || (v.text != "All" && v.text != "StartTime" && v.text != "EndTime")) fail(v.location,"syntax.active","Expected All, StartTime or EndTime");
                d.arguments.emplace_back(v.text);
            } else if (d.keyword == "Option") {
                auto p = parameter(r);
                if (p.array || p.values.size() != 1 || (p.type != "bool" && p.type != "string" && p.type != "float" && p.type != "integer")) fail(p.location,"syntax.option","Option requires one scalar value");
                d.parameters.push_back(std::move(p));
            } else if (d.keyword == "MediumInterface") {
                if (r.peek().kind == TokenKind::string) d.arguments.emplace_back(quoted(r));
                else d.arguments.push_back(d.arguments.front());
            }
            if (sig.params) while (r.peek().kind == TokenKind::string) d.parameters.push_back(parameter(r));
            target.on_directive(d);
            if (d.keyword == "Include" || d.keyword == "Import") {
                auto path = resolve(std::get<std::string>(d.arguments[0]));
                file({d.keyword == "Include" ? FileKind::include : FileKind::import,path,d.location});
            }
        }
        target.on_file_end(event); files.pop_back();
    }
};
SyntaxResult run(const std::filesystem::path& path, ParserTarget& t, const LoadOptions& options, std::optional<std::string> input) {
    Parser p{t,options,{},{},{}};
    try {
        auto full = std::filesystem::absolute(path).lexically_normal();
        p.base = options.base_directory.empty() ? full.parent_path() : std::filesystem::absolute(options.base_directory).lexically_normal();
        p.file({FileKind::root,full,{full.string(),1,1}},std::move(input));
    } catch (const Abort&) {} catch (const std::exception& e) {
        p.result.diagnostics.push_back({Severity::error,"parse.exception",e.what(),{path.string(),1,1}});
    }
    return std::move(p.result);
}
}
const std::vector<std::string>& supported_directives() {
    static const auto names = [] { std::vector<std::string> n; for (const auto& [k,v] : signatures()) { (void)v; n.push_back(k); } std::sort(n.begin(),n.end()); return n; }();
    return names;
}
SyntaxResult parse_file(const std::filesystem::path& p, ParserTarget& t, const LoadOptions& o) { return run(p,t,o,{}); }
SyntaxResult parse_string(std::string_view s, ParserTarget& t, std::string f, const LoadOptions& o) { return run(f,t,o,std::string(s)); }
}
