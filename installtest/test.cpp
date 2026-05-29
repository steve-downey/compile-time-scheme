// installtest/test.cpp                                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/smdscheme.hpp>

#include <cassert>
#include <variant>

int main() {
    using namespace smd::smdscheme;
    auto env = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto r = compiled_closure<"(+ 1 (* 2 3))">(env);
    assert(r.has_value());
    assert(std::get<int>(r.value()) == 7);
    return 0;
}
