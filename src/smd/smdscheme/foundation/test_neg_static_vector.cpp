#include <smd/smdscheme/foundation/static_vector.hpp>

int main() {
    using namespace smd::schemepoc;
    constexpr auto overflow = [] {
        static_vector<int, 2> vec{};
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3); // Overflow
        return vec.size();
    }();
    return overflow;
}
