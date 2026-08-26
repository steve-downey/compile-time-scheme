// src/smd/cl/reader/detail/skip.hpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_READER_DETAIL_SKIP_HPP
#define SRC_SMD_CL_READER_DETAIL_SKIP_HPP

#include <smd/cl/reader/cursor.hpp>
#include <smd/cl/reader/readtable.hpp>
#include <smd/kit/foundation/parse_error.hpp>
#include <smd/kit/parser/parse_context.hpp>
#include <smd/kit/parser/parser.hpp>
#include <smd/kit/parser/parser_instances.hpp>
#include <smd/kit/parser/repeat.hpp>

#include <variant>

namespace smd::cl::reader::detail {

namespace skip_detail {

using smd::kit::parser::char_p;
using smd::kit::parser::no_context;
using smd::kit::parser::parser;
using smd::kit::parser::skip_many;

// Matches the two-character delimiters via bind, the same Monad instance
// B2 registered against parser<F> -- not a hand-rolled sequencing helper.
inline constexpr auto block_comment_open =
    smd::kit::foundation::bind(char_p('#'), [](char) { return char_p('|'); });
inline constexpr auto block_comment_close =
    smd::kit::foundation::bind(char_p('|'), [](char) { return char_p('#'); });

/// Recursive worker behind @ref skip_block_comment: @p depth is the count of
/// still-open `#|`s, threaded as a recursive parameter rather than a
/// mutated loop variable. Returning @p cur unconditionally at end of input
/// -- regardless of @p depth -- is deliberate and load-bearing, not an
/// oversight: it is the D31 property that an unterminated `#|` consumes to
/// end of input and lets the caller's end-of-input handling report, rather
/// than this function reporting a (better, and therefore forbidden)
/// diagnostic pointing at the comment itself.
[[nodiscard]] constexpr auto skip_block_comment_depth(cursor cur, int depth)
    -> cursor {
    if (cur.empty() || depth == 0) {
        return cur;
    }
    // advance_while is the substrate's own character-stepping primitive
    // (src/smd/kit/parser/cursor.hpp); this only skips ordinary body
    // characters between the '#'/'|' occurrences that might start or close
    // a nested comment.
    cur = advance_while(cur, [](char c) { return c != '#' && c != '|'; });
    if (cur.empty()) {
        return cur;
    }
    no_context ctx{};
    if (auto const open = block_comment_open(cur, ctx); open.has_value()) {
        return skip_block_comment_depth(open.value().rest, depth + 1);
    }
    if (auto const close = block_comment_close(cur, ctx); close.has_value()) {
        return skip_block_comment_depth(close.value().rest, depth - 1);
    }
    return skip_block_comment_depth(cur.bump(), depth);
}

} // namespace skip_detail

/// Advances past a `#| ... |#` block comment body (the opening `#|`
/// already consumed), honouring nesting. An unterminated comment consumes
/// to end of input, where the caller's end-of-input handling reports
/// (decision D31: this is a deliberately weaker diagnostic than the natural
/// recursive-combinator answer would give, preserved on purpose -- see
/// @ref skip_detail::skip_block_comment_depth).
[[nodiscard]] constexpr auto skip_block_comment(cursor cur) -> cursor {
    return skip_detail::skip_block_comment_depth(cur, 1);
}

namespace skip_detail {

/// One step of intertoken space: whitespace, a `;` line comment, or a
/// `#|...|#` block comment, whichever applies at the current position.
/// Fails (rather than succeeding with no progress) when none applies, so
/// that @ref skip_many stops there instead of looping -- every branch that
/// succeeds consumes at least one character.
inline constexpr auto intertoken_step =
    parser{[](cursor cur, readtable const &table)
               -> smd::kit::parser::parse_result<std::monostate> {
        if (cur.empty()) {
            return smd::kit::foundation::parse_error{cur.position(),
                                                     "no intertoken space"};
        }
        if (table.is_whitespace(cur.peek())) {
            return smd::kit::parser::parse_state<std::monostate>{
                {}, advance_while(cur, [&table](char c) {
                    return table.is_whitespace(c);
                })};
        }
        if (table.macro_of(cur.peek()) == macro_kind::semicolon) {
            return smd::kit::parser::parse_state<std::monostate>{
                {},
                advance_while(cur.bump(), [](char c) { return c != '\n'; })};
        }
        if (table.macro_of(cur.peek()) == macro_kind::sharpsign) {
            cursor const after = cur.bump();
            if (!after.empty() &&
                table.sharp_of(after.peek()) == sharpsign_kind::block_comment) {
                return smd::kit::parser::parse_state<std::monostate>{
                    {}, skip_block_comment(after.bump())};
            }
        }
        return smd::kit::foundation::parse_error{cur.position(),
                                                 "no intertoken space"};
    }};

} // namespace skip_detail

// 2ce9b1bf-8125-40fa-8496-fb96580aab21
/// Advances past all leading intertoken space: interleaved runs of
/// whitespace, `;` line comments, and `#|...|#` block comments, to a
/// fixpoint, every decision read from @p table.
///
/// @p table is threaded as the combinator layer's context here: the public
/// `read<>` entry point calls this with a bare @ref readtable and no reader
/// context in hand, and `readtable const` models @c parse_context directly
/// rather than being wrapped -- the first evidence in this reader that two
/// different context types coexist, each named in the signature of the
/// parser that needs it (see docs/compiler_architecture.org, B3).
[[nodiscard]] constexpr auto skip_intertoken_space(cursor cur,
                                                   readtable const &table)
    -> cursor {
    return skip_detail::skip_many(skip_detail::intertoken_step)(cur, table)
        .value()
        .rest;
}
// 2ce9b1bf-8125-40fa-8496-fb96580aab21 end

} // namespace smd::cl::reader::detail

#endif
