#include <smd/smdscheme/smdscheme.hpp>

int main() {
    using namespace smd::smdscheme;
    char const *text = "not a literal";
    [[maybe_unused]] auto closure = compiled_closure<text>;
    return 0;
}
