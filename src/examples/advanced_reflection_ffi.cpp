// src/examples/advanced_reflection_ffi.cpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <cstdio>
#include <print>
#include <span>
#include <string_view>

#include <smd/smdscheme/reflection/reified_environment.hpp>
#include <smd/smdscheme/smdscheme.hpp>

namespace scm = smd::smdscheme;

using Core = scm::elaborator::core_type<512, 16>;

// 2. Foreign Function Interfaces (FFIs) mapped manually.
constexpr auto ffi_eq(std::span<scm::closure::value<Core> const> args)
    -> scm::foundation::result<scm::closure::value<Core>> {
    if (args.size() != 2)
        return scm::foundation::parse_error{{}, "arity eq?"};
    if (!std::holds_alternative<int>(args[0]) ||
        !std::holds_alternative<int>(args[1]))
        return scm::foundation::parse_error{{}, "type err eq?"};
    return scm::closure::value<Core>{std::get<int>(args[0]) ==
                                     std::get<int>(args[1])};
}

constexpr auto
ffi_print_and_return(std::span<scm::closure::value<Core> const> args)
    -> scm::foundation::result<scm::closure::value<Core>> {
    if (args.size() != 2)
        return scm::foundation::parse_error{{}, "arity print-and-return"};
    if (!std::holds_alternative<int>(args[0]))
        return scm::foundation::parse_error{{}, "type err print-and-return"};

    if (!std::is_constant_evaluated()) {
        std::println("Scheme executing evaluation step for n = {}",
                     std::get<int>(args[0]));
    }
    return args[1];
}

// 3. Compile-time generation of the Runtime State Aggregates using Reflection.
// This creates a physical C++ `struct` natively from the descriptor list.
struct RuntimeStateTag {};

// e3f4a5b6-c7d8-4e9f-0a1b-2c3d4e5f6071
consteval {
    scm::reflection::compile_environment<RuntimeStateTag>(
        {{^^int, "eval_result"}, {^^bool, "successful_run"}});
}
// e3f4a5b6-c7d8-4e9f-0a1b-2c3d4e5f6071 end

// 1. Non-trivial Scheme program compiled to heavily optimized CPS form strictly
// at compile-time. This is the Y-combinator executing Factorial recursively.
constexpr auto scheme_source = R"(
  ((lambda (fact)
     (fact fact 6))
   (lambda (self n)
     (print-and-return n
       (if (eq? n 0)
           1
           (* n (self self (+ n -1)))))))
)";

constexpr auto program =
    scm::closure::compile_to_closure<512, 16>(scheme_source).value();

int main() {
    std::println("==== Compile-Time Scheme -> Runtime Execution ====");

    auto env = scm::closure::default_env<Core, 16>();
    env.define("eq?", scm::closure::value<Core>{
                          scm::closure::foreign_function<Core>{ffi_eq}});
    env.define("print-and-return",
               scm::closure::value<Core>{
                   scm::closure::foreign_function<Core>{ffi_print_and_return}});

    std::println("Running pre-compiled Scheme application...");
    auto result = program(env);

    // f4a5b6c7-d8e9-4f0a-1b2c-3d4e5f607182
    // Populate a reified C++ aggregate structure directly from reflection
    // properties
    scm::reflection::reified_environment<RuntimeStateTag> state{};

    if (result.has_value()) {
        state.successful_run = true;
        state.eval_result = std::get<int>(result.value());
        std::println("Final Scheme Result Output: {}", state.eval_result);
    } else {
        state.successful_run = false;
        std::println(stderr, "Error evaluating Scheme: {}",
                     result.error().message);
        return 1;
    }
    // f4a5b6c7-d8e9-4f0a-1b2c-3d4e5f607182 end

    return 0;
}
