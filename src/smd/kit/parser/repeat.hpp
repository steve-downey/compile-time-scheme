// src/smd/kit/parser/repeat.hpp                                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_KIT_PARSER_REPEAT_HPP
#define SRC_SMD_KIT_PARSER_REPEAT_HPP

#include <smd/kit/foundation/parse_error.hpp>
#include <smd/kit/parser/cursor.hpp>
#include <smd/kit/parser/parse_context.hpp>
#include <smd/kit/parser/parser.hpp>

#include <optional>
#include <utility>
#include <variant>

namespace smd::kit::parser {

// 013cfe3b-6dfa-4f4b-b715-a59917e7715b
/// Runs @p p repeatedly, discarding every value, until @p p first fails or
/// first succeeds without consuming input, and succeeds with the cursor
/// where @p p stopped making progress.
///
/// Exhaustion is success, not failure -- the opposite of a collecting
/// repetition, whose contract is that running out of input before its own
/// stopping condition is met is a failure. This is deliberate: it is the
/// property @ref smd::cl::reader::detail::skip_intertoken_space depends on
/// so that an unterminated `#|` block comment still reports the caller's
/// end-of-input diagnostic rather than a diagnostic pointing at the comment
/// (decision D31, docs/cl-parser-scoping.md).
///
/// A zero-consumption success from @p p stops the repetition (as if @p p
/// had failed there) rather than looping forever: @c skip_many is itself
/// exactly such a parser -- it always succeeds, and consumes nothing once
/// its own inner parser starts failing -- so without this guard
/// @c skip_many(skip_many(p)) would spin the moment the inner repetition
/// reached its own fixpoint. Guarding here, once, is what lets a caller
/// nest repetitions without re-deriving this by hand.
///
/// @tparam P Parser type.
/// @param p The parser to repeat.
template <class P>
[[nodiscard]] constexpr auto skip_many(P p) {
    return parser{[p = std::move(p)](cursor cur, parse_context auto &ctx)
                      -> parse_result<std::monostate> {
        // Substrate generic algorithm: the repetition loop every
        // "run until failure, succeed on exhaustion" combinator over this
        // layer is built from.
        while (true) {
            auto const r = p(cur, ctx);
            if (!r.has_value() || r.value().rest == cur) {
                return parse_state<std::monostate>{{}, cur};
            }
            cur = r.value().rest;
        }
    }};
}
// 013cfe3b-6dfa-4f4b-b715-a59917e7715b end

// cb8c05d7-14a2-42fb-a36c-1f9da8bf705a
/// Runs @p step repeatedly, threading @p ctx, until it yields
/// @c std::nullopt -- which ends the repetition successfully at that
/// step's own rest cursor -- or until it fails, whose error this
/// repetition propagates unchanged. A @c std::optional value @p step
/// yields is otherwise discarded: this repetition supplies control flow
/// only, not accumulation, the same division of labour @ref skip_many
/// has with its own inner parser, which does not know what its caller
/// does with a successful match either.
///
/// This is the diagnosing collecting repetition the retired iteration's
/// `many<Capacity>` (`iteration/smdscheme-final`,
/// `src/smd/smdscheme/parser/alt.hpp`) is not: that combinator loops
/// until its inner parser fails and then always succeeds, so both
/// "collected Capacity elements and there was more" and "ran out of
/// input before the caller's own stopping condition was met" are silent
/// truncation, never failure -- a caller relying on either to be
/// diagnosed never finds out from that combinator, only from something
/// else afterward, at the wrong position, with the wrong message. Here,
/// @p step alone decides both what "stop" means (yielding
/// @c std::nullopt) and what a capacity overflow means (failing, with
/// whatever message and position it chooses, typically by checking its
/// own accumulator before accepting a value) -- this repetition does not
/// invent either policy, it only replaces the hand-written @c while(true)
/// loop that would otherwise carry it. It is named @c many_until, not
/// @c many_bounded or any other name suggesting it owns a capacity
/// itself, so that its caller-supplied stopping condition is not mistaken
/// for a truncating bound.
///
/// Guards a zero-consumption "keep going" value the same way @ref
/// skip_many guards a zero-consumption success -- by stopping there
/// rather than looping forever -- even though every known caller's own
/// @p step always advances on a "keep going" value (reading one datum, or
/// consuming a closing delimiter, always consumes at least one
/// character): a future @p step that does not hold that property should
/// not be able to hang a constexpr evaluation to find out.
///
/// @tparam P A parser over @c std::optional<T> for some @c T.
template <class P>
[[nodiscard]] constexpr auto many_until(P step) {
    return parser{[step = std::move(step)](cursor cur, parse_context auto &ctx)
                      -> parse_result<std::monostate> {
        // Substrate generic algorithm: the repetition loop this
        // combinator is built from, the collecting counterpart of
        // skip_many's discarding one above.
        while (true) {
            auto const r = step(cur, ctx);
            if (!r.has_value()) {
                return parse_result<std::monostate>{r.error()};
            }
            if (!r.value().value.has_value() || r.value().rest == cur) {
                return parse_state<std::monostate>{{}, r.value().rest};
            }
            cur = r.value().rest;
        }
    }};
}
// cb8c05d7-14a2-42fb-a36c-1f9da8bf705a end

} // namespace smd::kit::parser

#endif
