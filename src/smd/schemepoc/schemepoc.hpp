// src/smd/schemepoc/schemepoc.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_SCHEMEPOC_HPP
#define SRC_SMD_SCHEMEPOC_SCHEMEPOC_HPP

#include <smd/schemepoc/closure_backend.hpp>
#include <smd/schemepoc/value.hpp>

#include <algorithm>
#include <cstddef>
#include <string_view>

// 44cc988c-7353-43aa-a7d3-8840f92371a6
namespace smd::schemepoc {

// source_literal: NTTP carrier for string literals.
template <std::size_t N>
struct source_literal {
    char text[N]{};
    constexpr source_literal(char const (&input)[N]) {
        std::copy_n(input, N, text);
    }
    constexpr auto view() const -> std::string_view { return {text, N - 1}; }
};

// compiled_closure: evaluates Source at compile time.
// Use: constexpr auto prog = compiled_closure<"(+ 1 2)">;
//      auto r = prog(default_env<16>());
template <source_literal Source>
inline constexpr auto compiled_closure =
    compile_to_closure(Source.view()).value();

} // namespace smd::schemepoc
// 44cc988c-7353-43aa-a7d3-8840f92371a6 end

#endif // SRC_SMD_SCHEMEPOC_SCHEMEPOC_HPP
