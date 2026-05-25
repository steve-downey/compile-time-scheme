// src/examples/godbolt_lambda.cpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <iostream>
#include <smd/schemepoc/schemepoc.hpp>

using Core = smd::schemepoc::core_type<32, 16>;

constexpr auto program =
    smd::schemepoc::compiled_closure<"((lambda (x) (+ 1 (* x x))) argc)">;

int main(int argc, char **) {
    auto env = smd::schemepoc::default_env<Core, 16>();
    env.define("argc", smd::schemepoc::value<Core>{argc});

    auto result = program(env);

    if (result.has_value()) {
        std::cout << std::get<int>(result.value()) << '\n';
    } else {
        std::cout << "Error: " << result.error().message << '\n';
        return 1;
    }
    return 0;
}
