// src/smd/kit/parser/repeat.hpp                                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_KIT_PARSER_REPEAT_HPP
#define SRC_SMD_KIT_PARSER_REPEAT_HPP

#include <smd/kit/parser/cursor.hpp>
#include <smd/kit/parser/parse_context.hpp>
#include <smd/kit/parser/parser.hpp>

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

} // namespace smd::kit::parser

#endif
