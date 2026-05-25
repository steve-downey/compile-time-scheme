// src/examples/godbolt_arithmetic.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/schemepoc.hpp>

#include <iostream>

constexpr auto program = smd::schemepoc::compiled_closure<"(+ 1 (* 2 3))">;

int main() {
    auto result = program(smd::schemepoc::default_env<16>()).value();
    std::cout << std::get<int>(result) << '\n';
}
