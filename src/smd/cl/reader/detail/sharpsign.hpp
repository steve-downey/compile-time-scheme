// src/smd/cl/reader/detail/sharpsign.hpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_READER_DETAIL_SHARPSIGN_HPP
#define SRC_SMD_CL_READER_DETAIL_SHARPSIGN_HPP

#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/source_pos.hpp>
#include <smd/cl/reader/cursor.hpp>
#include <smd/cl/reader/datum.hpp>
#include <smd/cl/reader/detail/forms.hpp>
#include <smd/cl/reader/detail/read_context.hpp>
#include <smd/cl/reader/detail/text.hpp>
#include <smd/cl/reader/number.hpp>
#include <smd/cl/reader/readtable.hpp>
#include <smd/cl/reader/token.hpp>
#include <smd/kit/parser/choice.hpp>
#include <smd/kit/parser/parser.hpp>
#include <smd/kit/parser/parser_instances.hpp>
#include <smd/kit/parser/repeat.hpp>

#include <algorithm>
#include <optional>
#include <utility>
#include <variant>

namespace smd::cl::reader::detail {

using smd::kit::parser::parser;

/// Reads a rational token in @p radix (the radix prefix already
/// consumed): a fixnum when it fits, otherwise a tower spelling carrying
/// the radix (decision D19). Float syntax is decimal-only, so anything
/// but a rational here is an error.
///
/// The first real consumer of @ref parser's Monad instance: the radix
/// parsed from the prefix decides how the following token is classified,
/// which @c lift2 could never express, since it fixes both parsers before
/// either runs. @ref token_p is @c bind's first argument; its continuation
/// returns a second parser that classifies the scanned token and appends
/// it to the tree, resuming from whatever cursor @c bind threads it --
/// @c token_p's own rest, automatically, not a captured copy of it.
template <reader_context Ctx>
[[nodiscard]] constexpr auto read_radix_number(cursor cur, Ctx &ctx, int radix,
                                               foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    auto const classify_and_add = [radix, where](token_text const &scanned) {
        return parser{[radix, where, scanned](cursor rest,
                                              reader_context auto &ctx)
                          -> foundation::result<parse_state<int>> {
            auto const finish = [&](datum_atom atom) {
                return bind(
                    add_leaf_checked(ctx, std::move(atom), where),
                    [&](int id) -> foundation::result<parse_state<int>> {
                        return parse_state<int>{id, rest};
                    });
            };
            if (scanned.has_escape) {
                return foundation::parse_error{
                    where, "expected a rational after radix prefix"};
            }
            auto const classified = classify_number(scanned.view(), radix);
            switch (classified.cls) {
            case number_class::fixnum:
                return finish(datum_atom{datum_fixnum{classified.value}});
            case number_class::bignum:
                return finish(datum_atom{
                    datum_tower{tower_kind::bignum, radix, scanned}});
            case number_class::ratio:
                return finish(
                    datum_atom{datum_tower{tower_kind::ratio, radix, scanned}});
            case number_class::floating:
            case number_class::none:
                break;
            }
            return foundation::parse_error{
                where, "expected a rational after radix prefix"};
        }};
    };
    return bind(token_p, classify_and_add)(cur, ctx);
}

// b3055934-2efb-4df4-a4bb-e961e29e7d0f
/// Reads a sharpsign dispatch form (positioned at the `#`): the optional
/// infix numeric argument, then the sub-handler @ref readtable::sharp_of
/// selects.
///
/// The argument decides which parsers the character after it may name --
/// four of the seven arms refuse one outright, one requires it to be a
/// radix, one refuses it with a message of its own -- so the second parser
/// here is chosen by the first one's value, which is a @c bind and not a
/// @c lift2 (D28, docs/cl-parser-scoping.md).
///
/// The dispatch itself stays a @c switch. A readtable selects one branch
/// by lookup and commits to it; @ref smd::kit::parser::operator| searches,
/// and a search would change which diagnostic surfaces when the selected
/// branch fails, which D31 does not permit. What changed is that every arm
/// now names a parser instead of open-coding a call -- and because a datum
/// is an arena index, all seven of them have the same result type, so the
/// table already selects among same-typed parsers. That is the shape D19
/// asked for when it said a later @c set-macro-character should be a table
/// lookup rather than a redesign.
template <reader_context Ctx>
[[nodiscard]] constexpr auto read_sharpsign(cursor cur, Ctx &ctx)
    -> foundation::result<parse_state<int>> {
    auto const where = cur.position();

    // The optional infix numeric argument: a digit run folded into an int,
    // -1 when absent. Absence is a value rather than a failure, so
    // `optional` is what turns satisfy's own cursor-positioned failure --
    // which no caller here ever sees -- into the std::nullopt `many_until`
    // reads as "stop", and the fold rides along in `map`. The accumulator
    // is a local the way @ref read_string's is, for the same reason: this
    // parser is built, run, and discarded inside one call, so nothing it
    // captures can outlive it.
    int argument = -1;
    auto const digit_p = smd::kit::parser::satisfy(
        [](char c) { return c >= '0' && c <= '9'; }, "expected digit");
    auto const accumulate = [&argument](std::optional<char> digit) {
        if (digit.has_value()) {
            // Cap far above the largest valid radix; enough to reject.
            argument = std::min(
                (argument < 0 ? 0 : argument) * 10 + (*digit - '0'), 999);
        }
        return digit;
    };
    auto const argument_p = smd::kit::parser::map(
        smd::kit::parser::many_until(smd::kit::parser::map(
            smd::kit::parser::optional(digit_p), accumulate)),
        [&argument](std::monostate) { return argument; });

    auto const dispatch = [where](int infix) {
        return parser{[where, infix](cursor after, reader_context auto &rc)
                          -> foundation::result<parse_state<int>> {
            if (after.empty()) {
                return foundation::parse_error{
                    where, "unexpected end of input after '#'"};
            }
            // Each sub-handler as a parser value. Every one of them
            // reports at @c where -- the `#`'s own position, not the
            // cursor they are handed -- which is why they are built here,
            // closed over it, rather than named at namespace scope.
            auto const wrapped_p = [where](datum_branch kind) {
                return parser{
                    [where, kind](cursor c, reader_context auto &inner) {
                        return read_wrapped(c, inner, kind, where);
                    }};
            };
            auto const delimited_p = [where](datum_branch kind) {
                return parser{
                    [where, kind](cursor c, reader_context auto &inner) {
                        return read_delimited(c, inner, kind, where);
                    }};
            };
            auto const character_p =
                parser{[where](cursor c, reader_context auto &inner) {
                    return read_character(c, inner, where);
                }};
            auto const radix_p = [where](int radix) {
                return parser{
                    [where, radix](cursor c, reader_context auto &inner) {
                        return read_radix_number(c, inner, radix, where);
                    }};
            };
            // A precondition on four of the seven arms, producing one of
            // the five pinned diagnostics. It guards a parser rather than
            // a continuation now, but it is otherwise the check it was.
            auto const reject_argument =
                [&](auto p) -> foundation::result<parse_state<int>> {
                if (infix >= 0) {
                    return foundation::parse_error{
                        where, "unexpected numeric argument after '#'"};
                }
                return p(after.bump(), rc);
            };
            switch (rc.table.sharp_of(after.peek())) {
            case sharpsign_kind::function_quote:
                return reject_argument(wrapped_p(datum_branch::function));
            case sharpsign_kind::vector_open:
                if (infix >= 0) {
                    return foundation::parse_error{
                        where, "sized #n(...) vectors not yet supported"};
                }
                return delimited_p(datum_branch::vector)(after.bump(), rc);
            case sharpsign_kind::character_literal:
                return reject_argument(character_p);
            case sharpsign_kind::radix_binary:
                return reject_argument(radix_p(2));
            case sharpsign_kind::radix_octal:
                return reject_argument(radix_p(8));
            case sharpsign_kind::radix_hex:
                return reject_argument(radix_p(16));
            case sharpsign_kind::radix_n:
                if (infix < 2 || infix > 36) {
                    return foundation::parse_error{
                        where, "radix must be between 2 and 36"};
                }
                return radix_p(infix)(after.bump(), rc);
            case sharpsign_kind::block_comment: // consumed as intertoken space
            case sharpsign_kind::none:
                break;
            }
            return foundation::parse_error{where, "unsupported '#' syntax"};
        }};
    };
    return bind(argument_p, dispatch)(cur.bump(), ctx);
}
// b3055934-2efb-4df4-a4bb-e961e29e7d0f end

} // namespace smd::cl::reader::detail

#endif
