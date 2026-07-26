// src/examples/godbolt_lisp.cpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <iostream>
#include <smd/smdlisp/smdlisp.hpp>
#include <variant>

namespace lisp = smd::smdlisp;

// 05b6b13a-69a8-4324-a056-2d754a038802
// `x` is bound in the value namespace and in the function namespace at the
// same time. That is the difference between this front end and the Scheme
// one: `(+ x (x 3))` reads the first `x` as a variable and the second as a
// function, and both are live. Result: 10 + (3 * 2) = 16.
constexpr lisp::source_literal source{R"(
(progn
  (defvar x 10)
  (defun x (n) (* n 2))
  (+ x (x 3)))
)"};

// Reading, macro-free elaboration and CPS compilation all happen here, in
// the initializer of a constexpr variable. What survives to run time is a
// tree of already-compiled continuations.
using program = lisp::lisp_program<source>;

int main() {
    program::env_arena_type envs;
    auto environment = lisp::closure::default_env<program::core_type, 16>();

    auto result = lisp::compiled_lisp<source>(environment, envs);
    // 05b6b13a-69a8-4324-a056-2d754a038802 end

    if (result.has_value()) {
        std::cout << std::get<int>(result.value()) << '\n';
    } else {
        std::cout << "Error: " << result.error().message << '\n';
        return 1;
    }
    return 0;
}
