// src/examples/godbolt_arithmetic.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <iostream>
#include <smd/smdscheme/smdscheme.hpp>

namespace scm = smd::smdscheme;

using Core = scm::elaborator::core_type<32, 16>;

constexpr auto program = scm::compiled_closure<"(+ 1 (* 2 3))">;

int main() {
    auto env = scm::closure::default_env<Core, 16>();
    auto result = program(env);

    if (result.has_value()) {
        std::cout << std::get<int>(result.value()) << '\n';
    } else {
        std::cout << "Error: " << result.error().message << '\n';
        return 1;
    }
    return 0;
}
