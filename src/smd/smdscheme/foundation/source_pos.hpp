// src/smd/smdscheme/foundation/source_pos.hpp                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_FOUNDATION_SOURCE_POS_HPP
#define SRC_SMD_SMDSCHEME_FOUNDATION_SOURCE_POS_HPP

namespace smd::smdscheme::foundation {

/// A position within a source text, tracking byte offset, line, and column.
struct source_pos {
    int offset{};  ///< Byte offset from the start of the input.
    int line{1};   ///< 1-based line number.
    int column{1}; ///< 1-based column number.

    friend constexpr auto operator==(foundation::source_pos,
                                     foundation::source_pos) -> bool = default;
};

} // namespace smd::smdscheme::foundation

#endif
