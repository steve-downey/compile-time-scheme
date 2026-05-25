// src/examples/godbolt_lambda.cpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <iostream>
#include <smd/schemepoc/schemepoc.hpp>

constexpr auto program =
    smd::schemepoc::compiled_closure<"((lambda (x) (+ 1 (* x x))) argc)">;

int main(int argc, char **) {
    auto env = smd::schemepoc::default_env<16>();
    env.define("argc", smd::schemepoc::value{argc});

    auto result = program(env);

    if (result.has_value()) {
        std::cout << std::get<int>(result.value()) << '\n';
    } else {
        std::cout << result.error().message << '\n';
        return 1;
    }
    return 0;
}
