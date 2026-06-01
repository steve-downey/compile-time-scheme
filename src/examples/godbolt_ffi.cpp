// src/examples/godbolt_ffi.cpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <iostream>
#include <smd/smdscheme/smdscheme.hpp>
#include <span>

namespace scm = smd::smdscheme;

using Core = scm::elaborator::core_type<32, 16>;

// 2a057d0f-ec4f-4c3f-9c0f-7147a3675b6a
constexpr auto
ffi_print_and_add(std::span<scm::closure::value<Core> const> args)
    -> scm::foundation::result<scm::closure::value<Core>> {
    if (args.size() != 2)
        return scm::foundation::parse_error{{}, "ffi arity mismatch"};
    if (!std::holds_alternative<int>(args[0]) ||
        !std::holds_alternative<int>(args[1]))
        return scm::foundation::parse_error{{}, "ffi type error"};

    int a = std::get<int>(args[0]);
    int b = std::get<int>(args[1]);

    std::cout << "FFI Called with: " << a << " and " << b << '\n';

    return scm::closure::value<Core>{a + b};
}
// 2a057d0f-ec4f-4c3f-9c0f-7147a3675b6a end

// 2f7b4590-ca62-426d-a66f-2e9ace961764
constexpr auto program =
    scm::compiled_closure<"(print-and-add current-year 10)">;
// 2f7b4590-ca62-426d-a66f-2e9ace961764 end

// 27260b5f-089c-4210-bc68-5f5b25b2a025
int main() {
    auto env = scm::closure::default_env<Core, 16>();

    // Inject variables and native FFI functions into the environment
    env.define("current-year", scm::closure::value<Core>{2026});
    env.define("print-and-add",
               scm::closure::value<Core>{
                   scm::closure::foreign_function<Core>{ffi_print_and_add}});

    auto result = program(env);
    // 27260b5f-089c-4210-bc68-5f5b25b2a025 end

    if (result.has_value()) {
        std::cout << "Result: " << std::get<int>(result.value()) << '\n';
    } else {
        std::cout << "Error: " << result.error().message << '\n';
        return 1;
    }
    return 0;
}
