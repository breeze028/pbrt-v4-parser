#include <pbrt4/parser.h>
#include <iostream>
#include <stdexcept>
using namespace pbrt4;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Collector : ParserTarget {
    std::vector<Directive> events;
    void on_directive(const Directive& d) override { events.push_back(d); }
};
int main() {
    try {
        Collector c;
        auto r = parse_string("Film \"rgb\" \"integer xresolution\" 32\nWorldBegin Shape\"sphere\" \"float radius\" [1e-2]", c);
        for (const auto& d : r.diagnostics) std::cerr << format(d) << '\n';
        check(bool(r), "parse valid scene"); check(c.events.size() == 3,"directive count");
        check(std::get<double>(c.events[2].parameters[0].values[0]) == .01,"typed number");
        std::cout << "syntax smoke passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
