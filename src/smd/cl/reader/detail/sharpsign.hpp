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
#include <smd/kit/parser/parser.hpp>
#include <smd/kit/parser/parser_instances.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>

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

/// Reads a sharpsign dispatch form (positioned at the `#`): the optional
/// infix numeric argument, then the sub-handler @ref readtable::sharp_of
/// selects.
template <class Ctx>
[[nodiscard]] constexpr auto read_sharpsign(cursor cur, Ctx &ctx)
    -> foundation::result<parse_state<int>> {
    auto const where = cur.position();
    cursor after = cur.bump();
    int argument = -1; // -1: absent
    cursor const digits_end =
        advance_while(after, [](char c) { return c >= '0' && c <= '9'; });
    if (digits_end.position().offset > after.position().offset) {
        auto const digits = after.remaining().substr(
            0, static_cast<std::size_t>(digits_end.position().offset -
                                        after.position().offset));
        argument = std::ranges::fold_left(digits, 0, [](int acc, char c) {
            // Cap far above the largest valid radix; enough to reject.
            return std::min(acc * 10 + (c - '0'), 999);
        });
        after = digits_end;
    }
    if (after.empty()) {
        return foundation::parse_error{where,
                                       "unexpected end of input after '#'"};
    }
    auto const reject_argument =
        [&](auto continuation) -> foundation::result<parse_state<int>> {
        if (argument >= 0) {
            return foundation::parse_error{
                where, "unexpected numeric argument after '#'"};
        }
        return continuation();
    };
    switch (ctx.table.sharp_of(after.peek())) {
    case sharpsign_kind::function_quote:
        return reject_argument([&] {
            return read_wrapped(after.bump(), ctx, datum_branch::function,
                                where);
        });
    case sharpsign_kind::vector_open:
        if (argument >= 0) {
            return foundation::parse_error{
                where, "sized #n(...) vectors not yet supported"};
        }
        return read_delimited(after.bump(), ctx, datum_branch::vector, where);
    case sharpsign_kind::character_literal:
        return reject_argument(
            [&] { return read_character(after.bump(), ctx, where); });
    case sharpsign_kind::radix_binary:
        return reject_argument(
            [&] { return read_radix_number(after.bump(), ctx, 2, where); });
    case sharpsign_kind::radix_octal:
        return reject_argument(
            [&] { return read_radix_number(after.bump(), ctx, 8, where); });
    case sharpsign_kind::radix_hex:
        return reject_argument(
            [&] { return read_radix_number(after.bump(), ctx, 16, where); });
    case sharpsign_kind::radix_n:
        if (argument < 2 || argument > 36) {
            return foundation::parse_error{where,
                                           "radix must be between 2 and 36"};
        }
        return read_radix_number(after.bump(), ctx, argument, where);
    case sharpsign_kind::block_comment: // consumed as intertoken space
    case sharpsign_kind::none:
        break;
    }
    return foundation::parse_error{where, "unsupported '#' syntax"};
}

} // namespace smd::cl::reader::detail

#endif
