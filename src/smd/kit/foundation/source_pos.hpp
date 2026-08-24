// src/smd/kit/foundation/source_pos.hpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Extracted in step R8 from smd::cl::foundation::source_pos, itself the
// reviewed union of two independently-drifted copies:
// src/smd/smdscheme/foundation/source_pos.hpp (compile-time-scheme, at
// iteration/smdscheme-final) and src/smd/forth/foundation/source_pos.hpp
// (compile-time-forth). Those two copies were byte-identical apart from include
// guard, namespace, and comment — the zero-drift signal decision R8 acts on.
#ifndef SRC_SMD_KIT_FOUNDATION_SOURCE_POS_HPP
#define SRC_SMD_KIT_FOUNDATION_SOURCE_POS_HPP

namespace smd::kit::foundation {

/// A position within a source text, tracking byte offset, line, and column.
struct source_pos {
    int offset{};  ///< Byte offset from the start of the input.
    int line{1};   ///< 1-based line number.
    int column{1}; ///< 1-based column number.

    friend constexpr auto operator==(foundation::source_pos,
                                     foundation::source_pos) -> bool = default;
};

} // namespace smd::kit::foundation

#endif
