#include <smd/schemepoc/parser.hpp>

int main() {
    using namespace smd::schemepoc;
    // 'int' is not a parser_like.
    [[maybe_unused]] auto bad_parser = map(42, [](auto v) { return v; });
    return 0;
}
