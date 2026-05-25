// src/examples/godbolt_ffi.cpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <iostream>
#include <smd/schemepoc/schemepoc.hpp>
#include <span>

using Core = smd::schemepoc::core_type<32, 16>;

constexpr auto program =
    smd::schemepoc::compiled_closure<"(print-and-add current-year 10)">;

constexpr auto
ffi_print_and_add(std::span<smd::schemepoc::value<Core> const> args)
    -> smd::schemepoc::result<smd::schemepoc::value<Core>> {
    if (args.size() != 2)
        return smd::schemepoc::parse_error{{}, "ffi arity mismatch"};
    if (!std::holds_alternative<int>(args[0]) ||
        !std::holds_alternative<int>(args[1]))
        return smd::schemepoc::parse_error{{}, "ffi type error"};

    int a = std::get<int>(args[0]);
    int b = std::get<int>(args[1]);

    std::cout << "FFI Called with: " << a << " and " << b << '\n';

    return smd::schemepoc::value<Core>{a + b};
}

int main() {
    auto env = smd::schemepoc::default_env<Core, 16>();

    // Inject variables and native FFI functions into the environment
    env.define("current-year", smd::schemepoc::value<Core>{2026});
    env.define("print-and-add",
               smd::schemepoc::value<Core>{
                   smd::schemepoc::foreign_function<Core>{ffi_print_and_add}});

    auto result = program(env);

    if (result.has_value()) {
        std::cout << "Result: " << std::get<int>(result.value()) << '\n';
    } else {
        std::cout << "Error: " << result.error().message << '\n';
        return 1;
    }
    return 0;
}
