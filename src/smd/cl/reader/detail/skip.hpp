// src/smd/cl/reader/detail/skip.hpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_READER_DETAIL_SKIP_HPP
#define SRC_SMD_CL_READER_DETAIL_SKIP_HPP

#include <smd/cl/reader/cursor.hpp>
#include <smd/cl/reader/readtable.hpp>

namespace smd::cl::reader::detail {

/// Advances past a `#| ... |#` block comment body (the opening `#|`
/// already consumed), honouring nesting. An unterminated comment consumes
/// to end of input, where the caller's end-of-input handling reports.
[[nodiscard]] constexpr auto skip_block_comment(cursor cur) -> cursor {
    // Substrate generic algorithm: two-character lookahead with a nesting
    // depth makes this the skipping layer's one stateful loop.
    int depth = 1;
    while (!cur.empty() && depth > 0) {
        char const c = cur.peek();
        cursor next = cur.bump();
        if (c == '#' && !next.empty() && next.peek() == '|') {
            ++depth;
            next = next.bump();
        } else if (c == '|' && !next.empty() && next.peek() == '#') {
            --depth;
            next = next.bump();
        }
        cur = next;
    }
    return cur;
}

/// Advances past all leading intertoken space: interleaved runs of
/// whitespace, `;` line comments, and `#|...|#` block comments, to a
/// fixpoint, every decision read from @p table.
[[nodiscard]] constexpr auto skip_intertoken_space(cursor cur,
                                                   readtable const &table)
    -> cursor {
    // Substrate generic algorithm: fixpoint iteration of the three
    // skippable syntaxes, each step built on advance_while.
    while (true) {
        cur = advance_while(
            cur, [&table](char c) { return table.is_whitespace(c); });
        if (cur.empty()) {
            return cur;
        }
        if (table.macro_of(cur.peek()) == macro_kind::semicolon) {
            cur = advance_while(cur, [](char c) { return c != '\n'; });
            continue;
        }
        if (table.macro_of(cur.peek()) == macro_kind::sharpsign) {
            cursor const after = cur.bump();
            if (!after.empty() &&
                table.sharp_of(after.peek()) == sharpsign_kind::block_comment) {
                cur = skip_block_comment(after.bump());
                continue;
            }
        }
        return cur;
    }
}

} // namespace smd::cl::reader::detail

#endif
