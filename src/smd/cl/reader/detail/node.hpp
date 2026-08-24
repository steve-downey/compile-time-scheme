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
#include <smd/cl/reader/detail/read_node_fwd.hpp>
#include <smd/cl/reader/detail/sharpsign.hpp>
#include <smd/cl/reader/detail/skip.hpp>
#include <smd/cl/reader/detail/text.hpp>
#include <smd/cl/reader/detail/token_datum.hpp>
#include <smd/cl/reader/readtable.hpp>

namespace smd::cl::reader::detail {

// b803edcd-959d-4364-94f8-13fb256e7b9b
/// Reads one datum node: skips intertoken space, then dispatches on the
/// current character through the readtable (decision D19: this lookup —
/// not character tests — is the reader's spine).
template <class Ctx>
[[nodiscard]] constexpr auto read_node(cursor cur, Ctx &ctx)
    -> foundation::result<parse_state<int>> {
    cur = skip_intertoken_space(cur, ctx.table);
    if (cur.empty()) {
        return foundation::parse_error{cur.position(),
                                       "unexpected end of input"};
    }
    auto const where = cur.position();
    switch (ctx.table.macro_of(cur.peek())) {
    case macro_kind::none:
        return read_token_datum(cur, ctx);
    case macro_kind::left_paren:
        return read_delimited(cur.bump(), ctx, datum_branch::list, where);
    case macro_kind::right_paren:
        return foundation::parse_error{where, "unexpected ')'"};
    case macro_kind::single_quote:
        return read_wrapped(cur.bump(), ctx, datum_branch::quote, where);
    case macro_kind::backquote:
        return read_wrapped(cur.bump(), ctx, datum_branch::backquote, where);
    case macro_kind::comma: {
        cursor const after = cur.bump();
        if (!after.empty() && after.peek() == '@') {
            return read_wrapped(after.bump(), ctx, datum_branch::unquote_splice,
                                where);
        }
        return read_wrapped(after, ctx, datum_branch::unquote, where);
    }
    case macro_kind::double_quote:
        return read_string(cur.bump(), ctx, where);
    case macro_kind::semicolon: // consumed as intertoken space
        break;
    case macro_kind::sharpsign:
        return read_sharpsign(cur, ctx);
    }
    return foundation::parse_error{where, "expected datum"};
}
// b803edcd-959d-4364-94f8-13fb256e7b9b end

} // namespace smd::cl::reader::detail

#endif
