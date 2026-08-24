// src/smd/cl/reader/detail/forms.hpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_READER_DETAIL_FORMS_HPP
#define SRC_SMD_CL_READER_DETAIL_FORMS_HPP

#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/source_pos.hpp>
#include <smd/cl/reader/cursor.hpp>
#include <smd/cl/reader/datum.hpp>
#include <smd/cl/reader/detail/read_context.hpp>
#include <smd/cl/reader/detail/read_node_fwd.hpp>
#include <smd/cl/reader/detail/skip.hpp>
#include <smd/cl/reader/readtable.hpp>

namespace smd::cl::reader::detail {

/// Reads the datum after a quote-family marker and wraps it in a
/// one-child branch of @p kind.
template <class Ctx>
[[nodiscard]] constexpr auto read_wrapped(cursor after_marker, Ctx &ctx,
                                          datum_branch kind,
                                          foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    return and_then(
        read_node(after_marker, ctx),
        [&](parse_state<int> const &inner)
            -> foundation::result<parse_state<int>> {
            typename Ctx::child_list children;
            children.push_back(inner.value);
            return and_then(
                add_branch_checked(ctx, kind, children, where),
                [&](int id) -> foundation::result<parse_state<int>> {
                    return parse_state<int>{id, inner.rest};
                });
        });
}

/// Reads `)`-delimited elements (the opening delimiter already consumed)
/// into a branch of @p kind: lists and vectors.
template <class Ctx>
[[nodiscard]] constexpr auto read_delimited(cursor cur, Ctx &ctx,
                                            datum_branch kind,
                                            foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    typename Ctx::child_list children;
    // Substrate generic algorithm: the reader's element unfold. The
    // elements do not exist as a structure until this loop reads them,
    // so this produces what traverse later consumes.
    while (true) {
        cur = skip_intertoken_space(cur, ctx.table);
        if (cur.empty()) {
            return foundation::parse_error{cur.position(), "expected ')'"};
        }
        if (ctx.table.macro_of(cur.peek()) == macro_kind::right_paren) {
            return and_then(
                add_branch_checked(ctx, kind, children, where),
                [&](int id) -> foundation::result<parse_state<int>> {
                    return parse_state<int>{id, cur.bump()};
                });
        }
        // Read one element and record it: two steps, the second taking the
        // first's output, which is a bind. Running out of room is one more
        // way reading an element fails, so the check belongs inside the
        // chain rather than in a second ladder after it.
        auto const stepped = and_then(
            read_node(cur, ctx),
            [&](parse_state<int> const &element) -> foundation::result<cursor> {
                if (children.size() >= children.capacity()) {
                    return foundation::parse_error{cur.position(),
                                                   "too many elements"};
                }
                children.push_back(element.value);
                return element.rest;
            });
        // The one test that survives, and the reason it is not the ladder
        // D15 rules out: this asks whether to take another turn of an
        // unfold whose length nothing knows until the `)` arrives. Under
        // strict evaluation a traversal can only stop by asking the effect
        // whether it has already failed — the same argument
        // fold_left_short's short_circuit_effect rests on — and bind, which
        // skips work but not iteration, cannot answer it. Removing this
        // means making the loop itself monadic, over a parser that carries
        // its own state; that is the combinator layer's job, not this one's.
        if (!stepped.has_value()) {
            return stepped.error();
        }
        cur = stepped.value();
    }
}

} // namespace smd::cl::reader::detail

#endif
