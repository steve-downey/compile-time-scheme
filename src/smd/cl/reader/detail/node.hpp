// src/smd/cl/reader/detail/node.hpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_READER_DETAIL_NODE_HPP
#define SRC_SMD_CL_READER_DETAIL_NODE_HPP

#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/source_pos.hpp>
#include <smd/cl/reader/cursor.hpp>
#include <smd/cl/reader/datum.hpp>
#include <smd/cl/reader/detail/forms.hpp>
#include <smd/cl/reader/detail/read_context.hpp>
#include <smd/cl/reader/detail/read_node_fwd.hpp>
#include <smd/cl/reader/detail/sharpsign.hpp>
#include <smd/cl/reader/detail/skip.hpp>
#include <smd/cl/reader/detail/text.hpp>
#include <smd/cl/reader/detail/token_datum.hpp>
#include <smd/cl/reader/readtable.hpp>
#include <smd/kit/parser/choice.hpp>
#include <smd/kit/parser/parser.hpp>
#include <smd/kit/parser/parser_instances.hpp>

namespace smd::cl::reader::detail {

// b803edcd-959d-4364-94f8-13fb256e7b9b
/// Reads one datum node: skips intertoken space, then dispatches on the
/// current character through the readtable (decision D19: this lookup —
/// not character tests — is the reader's spine).
///
/// The reader's other dispatch, and the same shape as @ref read_sharpsign:
/// a selection by table lookup, committed to, with each arm naming a
/// parser. Every arm has the same result type because a datum is an arena
/// index, so what the readtable selects among is already a set of
/// same-typed parsers -- which is what a later @c set-macro-character
/// needs and is the whole of D19's ask.
///
/// It is not @c operator|, and the distinction is the point. A chain of
/// alternatives searches, and on a double failure reports the last
/// alternative's error; a table selects one arm and lets that arm's
/// diagnostic stand. Those differ exactly when an arm fails, which is
/// every diagnostic below, so D31 settles it.
template <reader_context Ctx>
[[nodiscard]] constexpr auto read_node(cursor cur, Ctx &ctx)
    -> foundation::result<parse_state<int>> {
    cur = skip_intertoken_space(cur, ctx.table);
    if (cur.empty()) {
        return foundation::parse_error{cur.position(),
                                       "unexpected end of input"};
    }
    auto const where = cur.position();
    // The quote family and the two bracketing forms record their branch at
    // @p where -- the marker's own position, one character behind the
    // cursor each parser is then handed. That difference is observable in
    // exactly one place (a "datum tree full" from the branch append) and
    // is pinned by read.test.cpp's wrapped_branch_error_sits_at_the_marker.
    auto const wrapped_p = [where](datum_branch kind) {
        return smd::kit::parser::parser{
            [where, kind](cursor c, reader_context auto &rc) {
                return read_wrapped(c, rc, kind, where);
            }};
    };
    auto const delimited_p = [where](datum_branch kind) {
        return smd::kit::parser::parser{
            [where, kind](cursor c, reader_context auto &rc) {
                return read_delimited(c, rc, kind, where);
            }};
    };
    auto const string_p =
        smd::kit::parser::parser{[where](cursor c, reader_context auto &rc) {
            return read_string(c, rc, where);
        }};
    auto const token_datum_p =
        smd::kit::parser::parser{[](cursor c, reader_context auto &rc) {
            return read_token_datum(c, rc);
        }};
    auto const sharpsign_p =
        smd::kit::parser::parser{[](cursor c, reader_context auto &rc) {
            return read_sharpsign(c, rc);
        }};
    switch (ctx.table.macro_of(cur.peek())) {
    case macro_kind::none:
        return token_datum_p(cur, ctx);
    case macro_kind::left_paren:
        return delimited_p(datum_branch::list)(cur.bump(), ctx);
    case macro_kind::right_paren:
        return foundation::parse_error{where, "unexpected ')'"};
    case macro_kind::single_quote:
        return wrapped_p(datum_branch::quote)(cur.bump(), ctx);
    case macro_kind::backquote:
        return wrapped_p(datum_branch::backquote)(cur.bump(), ctx);
    case macro_kind::comma: {
        // `,@` against `,`, and the one place in this function where
        // operator| is the right tool: this is a lookahead between two
        // spellings of one macro character, not a readtable selection, so
        // there is no table entry to consult and nothing to commit to
        // until the next character is read. The splice alternative fails
        // at the cursor operator| started from whenever the `@` is absent,
        // which is what lets the plain unquote stand; once the `@` is
        // consumed the alternative has committed, and everything that can
        // fail after it reports at where or beyond, never back at the
        // starting cursor, so nothing real can fall through by accident.
        auto const splice_p =
            bind(smd::kit::parser::char_p('@'), [wrapped_p](char) {
                return wrapped_p(datum_branch::unquote_splice);
            });
        return (splice_p | wrapped_p(datum_branch::unquote))(cur.bump(), ctx);
    }
    case macro_kind::double_quote:
        return string_p(cur.bump(), ctx);
    case macro_kind::semicolon: // consumed as intertoken space
        break;
    case macro_kind::sharpsign:
        return sharpsign_p(cur, ctx);
    }
    return foundation::parse_error{where, "expected datum"};
}
// b803edcd-959d-4364-94f8-13fb256e7b9b end

} // namespace smd::cl::reader::detail

#endif
