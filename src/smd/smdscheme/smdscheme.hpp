// src/smd/smdscheme/smdscheme.hpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SMDSCHEME_HPP
#define SRC_SMD_SMDSCHEME_SMDSCHEME_HPP

// src/smd/smdscheme/smdscheme.hpp                              -*-C++-*-// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception#ifndef SRC_SMD_SMDSCHEME_SMDSCHEME_HPP#define SRC_SMD_SMDSCHEME_SMDSCHEME_HPP
// 44cc988c-7353-43aa-a7d3-8840f92371a6
#include <smd/smdscheme/closure/closure_backend.hpp>

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace smd::smdscheme {

template <std::size_t N>
struct source_literal {
    char text[N]{};
    constexpr source_literal(char const (&input)[N]) {
        std::copy_n(input, N, text);
    }
    [[nodiscard]] constexpr auto view() const -> std::string_view {
        return {text, N - 1};
    }
};

template <source_literal Source>
inline constexpr auto compiled_closure =
    closure::compile_to_closure(Source.view()).value();

} // namespace smd::smdscheme
// 44cc988c-7353-43aa-a7d3-8840f92371a6 end

#endif // SRC_SMD_SCHEMEPOC_SCHEMEPOC_HPP
