// src/examples/godbolt_lisp_ffi.cpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <iostream>
#include <smd/smdlisp/smdlisp.hpp>
#include <span>
#include <variant>

namespace lisp = smd::smdlisp;

// 89fd04b3-d44b-4bcb-86f9-048297a36b7a
constexpr lisp::source_literal source{"(print-and-add current-year 10)"};

using program = lisp::lisp_program<source>;
using Core = program::core_type;
using Val = lisp::closure::value<Core>;

// A `foreign_function` is an ordinary C++ function reachable by pointer.
// There is no new core form and no marshalling layer: it takes the same
// `value` variant the evaluator already passes around, and returns the same
// `result` the evaluator already propagates.
constexpr auto ffi_print_and_add(std::span<Val const> args)
    -> smd::smdscheme::foundation::result<Val> {
    if (args.size() != 2)
        return smd::smdscheme::foundation::parse_error{{},
                                                       "ffi arity mismatch"};
    if (!std::holds_alternative<int>(args[0]) ||
        !std::holds_alternative<int>(args[1]))
        return smd::smdscheme::foundation::parse_error{{}, "ffi type error"};

    int a = std::get<int>(args[0]);
    int b = std::get<int>(args[1]);

    std::cout << "FFI Called with: " << a << " and " << b << '\n';

    return Val{a + b};
}
// 89fd04b3-d44b-4bcb-86f9-048297a36b7a end

// c19f8ea7-df86-41a3-9586-ca2daf90d093
int main() {
    program::env_arena_type envs;
    auto environment = lisp::closure::default_env<Core, 16>();

    // Two namespaces, two injection points (decision D4). The reader case
    // folds, so the names are spelled uppercase here.
    environment.define_value(lisp::closure::symbol{"CURRENT-YEAR"}, Val{2026});
    environment.define_function(
        lisp::closure::symbol{"PRINT-AND-ADD"},
        Val{lisp::closure::foreign_function<Core>{ffi_print_and_add}});

    auto result = lisp::compiled_lisp<source>(environment, envs);
    // c19f8ea7-df86-41a3-9586-ca2daf90d093 end

    if (result.has_value()) {
        std::cout << "Result: " << std::get<int>(result.value()) << '\n';
    } else {
        std::cout << "Error: " << result.error().message << '\n';
        return 1;
    }
    return 0;
}
