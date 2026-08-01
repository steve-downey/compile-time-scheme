// src/smd/cl/foundation/source_pos.hpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Reviewed union of the two prior copies of this component:
// src/smd/smdscheme/foundation/source_pos.hpp (compile-time-scheme) and
// src/smd/forth/foundation/source_pos.hpp (compile-time-forth).
#ifndef SRC_SMD_CL_FOUNDATION_SOURCE_POS_HPP
#define SRC_SMD_CL_FOUNDATION_SOURCE_POS_HPP

namespace smd::cl::foundation {

/// A position within a source text, tracking byte offset, line, and column.
struct source_pos {
    int offset{};  ///< Byte offset from the start of the input.
    int line{1};   ///< 1-based line number.
    int column{1}; ///< 1-based column number.

    friend constexpr auto operator==(foundation::source_pos,
                                     foundation::source_pos) -> bool = default;
};

} // namespace smd::cl::foundation

#endif
