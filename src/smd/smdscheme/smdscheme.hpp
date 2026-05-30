// src/smd/smdscheme/smdscheme.hpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SMDSCHEME_HPP
#define SRC_SMD_SMDSCHEME_SMDSCHEME_HPP

// 44cc988c-7353-43aa-a7d3-8840f92371a6
#include <smd/smdscheme/closure/closure_program.hpp>

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace smd::smdscheme {

/// A compile-time string literal that can be used as a non-type template
/// parameter.
///
/// Stores a null-terminated copy of the source string in a @c char array so
/// that the string content participates in template argument deduction and
/// becomes part of the type identity.
///
/// @tparam N Length of the string literal, including the null terminator.
template <std::size_t N>
struct source_literal {
    char text[N]{}; ///< Null-terminated copy of the source string.

    /// Constructs from a string literal; copies all @p N characters.
    constexpr source_literal(char const (&input)[N]) {
        std::copy_n(input, N, text);
    }

    /// Returns a @c string_view over the stored text, excluding the null
    /// terminator.
    [[nodiscard]] constexpr auto view() const -> std::string_view {
        return {text, N - 1};
    }
};

/// A @c constinit variable template that compiles a Scheme source literal to a
/// @ref closure::closure_program at program-startup time.
///
/// Usage:
/// @code
///   auto &prog = compiled_closure<"(+ 1 2)">;
///   auto result = prog(closure::default_env<Core, 16>());
/// @endcode
///
/// The Scheme source must parse and elaborate without error; a violation is a
/// compile-time error because @c .value() on a failed @c result will throw in
/// constexpr.
///
/// @tparam Source The Scheme source literal, deduced from the NTTP.
template <source_literal Source>
inline constexpr auto compiled_closure =
    closure::compile_to_closure(Source.view()).value();

} // namespace smd::smdscheme
// 44cc988c-7353-43aa-a7d3-8840f92371a6 end

#endif // SRC_SMD_SCHEMEPOC_SCHEMEPOC_HPP
