// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace pbrt4 {
// PBRT string/path literals and SourceLocation filenames use UTF-8 on every OS.
std::filesystem::path path_from_utf8(std::string_view);
std::string path_to_utf8(const std::filesystem::path&);
struct SourceLocation {
    std::string filename;
    std::size_t line = 1, column = 1;
};
enum class Severity { warning, error };
struct Diagnostic {
    Severity severity = Severity::error;
    std::string code, message;
    SourceLocation location;
};
bool has_errors(const std::vector<Diagnostic>& diagnostics);
std::string format(const Diagnostic& diagnostic);

using Value = std::variant<std::int64_t, double, bool, std::string>;
struct Parameter {
    std::string type, name;
    std::vector<Value> values;
    SourceLocation location;
    std::string color_space; // Filled by SceneBuilder, empty for syntax-only use.
    bool array = false;
};
struct Directive {
    std::string keyword;
    std::vector<Value> arguments;
    std::vector<Parameter> parameters;
    SourceLocation location;
};
enum class TokenKind { word, string, left_bracket, right_bracket, end };
struct Token { TokenKind kind; std::string text; SourceLocation location; };
// Owns input. Tokens own their text and survive subsequent next() calls.
class Tokenizer {
public:
    Tokenizer(std::string input, std::string filename, std::vector<Diagnostic>& diagnostics);
    Token next();
private:
    std::string input_;
    SourceLocation location_;
    std::vector<Diagnostic>& diagnostics_;
    std::size_t position_ = 0;
    char take();
};
struct LoadOptions {
    std::filesystem::path base_directory; // Empty: root file's parent (memory: cwd).
    std::size_t max_include_depth = 64;
    std::size_t max_file_bytes = 256 * 1024 * 1024;
    // Optional virtual filesystem. Receives normalized absolute resource paths.
    std::function<std::optional<std::string>(const std::filesystem::path&)> read_file;
};
enum class FileKind { root, include, import };
struct FileEvent {
    FileKind kind;
    std::filesystem::path path;
    SourceLocation location; // Include/Import location, or root location.
};
class ParserTarget {
public:
    virtual ~ParserTarget() = default;
    virtual void on_directive(const Directive&) = 0;
    virtual void on_file_begin(const FileEvent&) {}
    virtual void on_file_end(const FileEvent&) {}
};
struct SyntaxResult {
    std::vector<Diagnostic> diagnostics;
    explicit operator bool() const { return !has_errors(diagnostics); }
};
SyntaxResult parse_file(const std::filesystem::path&, ParserTarget&, const LoadOptions& = {});
SyntaxResult parse_string(std::string_view, ParserTarget&, std::string filename = "<memory>", const LoadOptions& = {});
// All current v4 keywords; deprecated v3 transform scopes are accepted with warnings.
const std::vector<std::string>& supported_directives();
}
