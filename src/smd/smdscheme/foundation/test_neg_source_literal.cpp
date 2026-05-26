#include <smd/smdscheme/foundation/schemepoc.hpp>

int main() {
    using namespace smd::schemepoc;
    char const *text = "not a literal";
    [[maybe_unused]] auto closure = compiled_closure<text>;
    return 0;
}
