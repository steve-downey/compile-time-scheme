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
#include <smd/kit/parser/parser.hpp>
#include <smd/kit/parser/parser_instances.hpp>
#include <smd/kit/parser/repeat.hpp>

#include <optional>
#include <variant>

namespace smd::cl::reader::detail {

// 086ca303-e32b-40bb-88fd-09a2cffaad61
/// Reads the datum after a quote-family marker and wraps it in a
/// one-child branch of @p kind.
///
/// The example @c docs/cl-parser-scoping.md § 1 leads with, and the
/// shortest statement of what the last five steps were for. Before the
/// layer existed this was @c and_then nested inside @c and_then -- the
/// inner datum's result feeding a branch append, the branch append's
/// result feeding a @ref parse_state -- which is @c bind written out by
/// hand, twice, over two different monads that happened to share a
/// spelling. Now the outer one is the parser Monad's own @c bind: @ref
/// read_node lifted into a parser value, and a continuation that wraps
/// whatever it produced.
///
/// Two positions have to stay apart and neither is checked by the types.
/// The branch is recorded at @p where -- the *marker's* position, passed
/// in by @ref read_sharpsign or by @ref read_node's readtable dispatch,
/// not the inner datum's, which for `'x` is one character further on. The
/// cursor the whole thing resumes from is the inner datum's rest, and that
/// one is no longer written down at all: @c bind threads it into the
/// continuation's parser, which is exactly the class of transcription
/// mistake the conversion removes rather than merely avoids.
///
/// @ref read_node is still an ordinary function template reached through
/// @c detail/read_node_fwd.hpp, wrapped here in a @c parser at each call
/// rather than made a parser value that closes over itself. The mutual
/// recursion is B7's to look at; this step only stops writing its bind out
/// longhand.
template <reader_context Ctx>
[[nodiscard]] constexpr auto read_wrapped(cursor after_marker, Ctx &ctx,
                                          datum_branch kind,
                                          foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    auto const node_p = smd::kit::parser::parser{
        [](cursor c, reader_context auto &rc) { return read_node(c, rc); }};
    auto const wrap_in_branch = [kind, where](int inner) {
        return smd::kit::parser::parser{
            [kind, where, inner](cursor rest, reader_context auto &rc)
                -> foundation::result<parse_state<int>> {
                typename Ctx::child_list children;
                children.push_back(inner);
                return bind(
                    add_branch_checked(rc, kind, children, where),
                    [rest](int id) -> foundation::result<parse_state<int>> {
                        return parse_state<int>{id, rest};
                    });
            }};
    };
    return bind(node_p, wrap_in_branch)(after_marker, ctx);
}
// 086ca303-e32b-40bb-88fd-09a2cffaad61 end

// 1fe3a27d-557b-46a2-92ae-3ccd155173d2
/// Reads `)`-delimited elements (the opening delimiter already consumed)
/// into a branch of @p kind: lists and vectors.
///
/// One iteration of the element unfold, reused verbatim from before this
/// step (93e4bc7 already turned it into the @c and_then chain below): read
/// one datum, and record it unless @c children is already full, in which
/// case that is one more way reading an element fails. What this step
/// changes is what drives that chain: a hand-written unbounded repeat
/// construct used to call it, carrying a "substrate generic algorithm"
/// comment that was never true of a loop living in the reader rather than
/// the substrate (@ref smd::kit::parser -- @c src/smd/kit/parser/repeat.hpp
/// -- is the substrate; this file is not). @ref smd::kit::parser::many_until
/// now drives it: this function's own step closure decides, each turn,
/// whether to stop (the closing delimiter, signalled by
/// @c std::nullopt), keep going (one element, folded into @c children by
/// the unchanged @c and_then chain), or fail (running out of input, or
/// the chain above failing) -- and @c many_until only supplies the "keep
/// asking until told to stop or handed a failure" control flow around
/// that, the same relationship it has with @ref skip_many's own inner
/// parser.
///
/// The three diagnostics are unchanged, at the same positions: `"expected
/// ')'"` when input runs out before the closing delimiter (checked here,
/// ahead of @c read_node, so it is never read_node's own "unexpected end
/// of input" -- a different message at the same position, and so a
/// behaviour change D31 forbids); `"too many elements"`, at the position
/// of the element that would not fit, from the unchanged capacity check;
/// and `"datum tree full"` from @ref add_branch_checked, unchanged.
template <class Ctx>
[[nodiscard]] constexpr auto read_delimited(cursor cur, Ctx &ctx,
                                            datum_branch kind,
                                            foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    typename Ctx::child_list children;
    auto const step = [&children](cursor c, reader_context auto &rc)
        -> foundation::result<parse_state<std::optional<int>>> {
        c = skip_intertoken_space(c, rc.table);
        if (c.empty()) {
            return foundation::parse_error{c.position(), "expected ')'"};
        }
        if (rc.table.macro_of(c.peek()) == macro_kind::right_paren) {
            return parse_state<std::optional<int>>{std::nullopt, c.bump()};
        }
        // Read one element and record it: two steps, the second taking the
        // first's output, which is a bind. Running out of room is one more
        // way reading an element fails, so the check belongs inside the
        // chain rather than in a second ladder after it.
        return and_then(
            read_node(c, rc),
            [&](parse_state<int> const &element)
                -> foundation::result<parse_state<std::optional<int>>> {
                if (children.size() >= children.capacity()) {
                    return foundation::parse_error{c.position(),
                                                   "too many elements"};
                }
                children.push_back(element.value);
                return parse_state<std::optional<int>>{element.value,
                                                       element.rest};
            });
    };
    return and_then(
        smd::kit::parser::many_until(step)(cur, ctx),
        [&](parse_state<std::monostate> const &finished)
            -> foundation::result<parse_state<int>> {
            return and_then(
                add_branch_checked(ctx, kind, children, where),
                [&](int id) -> foundation::result<parse_state<int>> {
                    return parse_state<int>{id, finished.rest};
                });
        });
}
// 1fe3a27d-557b-46a2-92ae-3ccd155173d2 end

} // namespace smd::cl::reader::detail

#endif
