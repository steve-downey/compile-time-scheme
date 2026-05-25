// src/examples/godbolt_arithmetic.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <iostream>
#include <smd/schemepoc/schemepoc.hpp>

using Core = smd::schemepoc::core_type<32, 16>;

constexpr auto program = smd::schemepoc::compiled_closure<"(+ 1 (* 2 3))">;

int main() {
    auto env = smd::schemepoc::default_env<Core, 16>();
    auto result = program(env);

    if (result.has_value()) {
        std::cout << std::get<int>(result.value()) << '\n';
    } else {
        std::cout << "Error: " << result.error().message << '\n';
        return 1;
    }
    return 0;
}
