// src/smd/kit/parser/choice.hpp                                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_KIT_PARSER_CHOICE_HPP
#define SRC_SMD_KIT_PARSER_CHOICE_HPP

#include <smd/kit/parser/cursor.hpp>
#include <smd/kit/parser/parse_context.hpp>
#include <smd/kit/parser/parser.hpp>

#include <optional>
#include <type_traits>
#include <utility>

namespace smd::kit::parser {

// 0fb6ef1c-7476-4f92-8259-0cc24c0459f3
/// Ordered choice: runs @p pa; if it fails without consuming any input,
/// runs @p pb from the same starting cursor; otherwise @p pa's result --
/// success or failure -- stands.
///
/// "Without consuming" is judged by comparing @p pa's failure position to
/// the cursor @c operator| itself started from, not by any property @p pa
/// reports about itself: a parser that partially matches and then fails
/// deeper in has already committed, and that failure propagates rather
/// than being discarded in favour of @p pb. This is the retired
/// iteration's own rule (`iteration/smdscheme-final`,
/// `src/smd/smdscheme/parser/parser.hpp`), carried forward unchanged
/// because B7's readtable dispatch is built on it continuing to hold.
///
/// On a double failure -- @p pa fails without consuming and @p pb also
/// fails -- @p pb's error, whatever it is, is what @c operator| returns.
/// This is deliberately not a synthesized "expected one of ..." message:
/// hand-rolled parsers in this codebase produce targeted diagnostics
/// (`"expected ')'"`), and a chain of alternatives that manufactures its
/// own combined message would be a *better* parser than the one it
/// replaces, which decision D31 forbids. A caller that wants a specific
/// message to survive a double failure puts that alternative last.
///
/// @tparam PA First alternative.
/// @tparam PB Second alternative; its type must produce the same
///         @c parse_result as @p PA.
template <class PA, class PB>
[[nodiscard]] constexpr auto operator|(parser<PA> pa, parser<PB> pb) {
    return parser{[pa = std::move(pa),
                   pb = std::move(pb)](cursor cur, parse_context auto &ctx) {
        auto const start = cur.position();
        auto const ra = pa(cur, ctx);
        if (ra.has_value()) {
            return ra;
        }
        if (ra.error().where != start) {
            return ra;
        }
        return pb(cur, ctx);
    }};
}

/// Named alias for @ref operator|, for a call-site spelling that reads as
/// a combinator rather than an operator.
///
/// @tparam PA First alternative.
/// @tparam PB Second alternative.
template <class PA, class PB>
[[nodiscard]] constexpr auto alt(parser<PA> pa, parser<PB> pb) {
    return pa | pb;
}
// 0fb6ef1c-7476-4f92-8259-0cc24c0459f3 end

/// Returns a parser that succeeds with @c std::optional engaged at @p p's
/// value when @p p succeeds, succeeds with @c std::nullopt at the
/// original cursor when @p p fails without consuming, and fails -- rather
/// than always succeeding -- when @p p fails after consuming input.
///
/// Applies @ref operator|'s own no-backtracking-after-consumption rule to
/// the specific case of falling back to "nothing here", rather than
/// hand-rolling an independent always-succeeds loop: unlike the retired
/// iteration's `optional` (`iteration/smdscheme-final`,
/// `src/smd/smdscheme/parser/parser.hpp`), which always succeeds even
/// when @p p fails after consuming input, this @c optional propagates
/// that failure, because a caller past the point of committed input has
/// already lost the right to fall back to "nothing here".
///
/// @tparam P Parser to attempt.
template <class P>
[[nodiscard]] constexpr auto optional(P p) {
    return parser{[p = std::move(p)](cursor cur, parse_context auto &ctx) {
        auto const start = cur.position();
        auto const r = p(cur, ctx);
        using T = std::remove_cvref_t<decltype(r.value().value)>;
        if (r.has_value()) {
            return parse_result<std::optional<T>>{
                parse_state<std::optional<T>>{r.value().value, r.value().rest}};
        }
        if (r.error().where != start) {
            return parse_result<std::optional<T>>{r.error()};
        }
        return parse_result<std::optional<T>>{
            parse_state<std::optional<T>>{std::optional<T>{}, cur}};
    }};
}

} // namespace smd::kit::parser

#endif
