// src/smd/smdscheme/parser/parser.hpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_PARSER_PARSER_HPP
#define SRC_SMD_SMDSCHEME_PARSER_PARSER_HPP

#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/parser/cursor.hpp>

namespace smd::smdscheme::parser {

/// Concept satisfied by any callable @p P that can be invoked with a @ref
/// cursor.
template <class P>
concept parser_like = requires(P p, cursor c) { p(c); };

// 59712a9e-e890-4931-9196-61ceaa880b6d
/// Carries a successfully parsed value together with the unconsumed @ref
/// cursor.
///
/// @tparam T The type of the parsed value.
template <class T>
struct parse_state {
    T value;     ///< The successfully parsed value.
    cursor rest; ///< The cursor positioned after the parsed input.
};

/// Alias for the result type returned by all parsers.
/// Holds either a @ref parse_state on success or a @ref foundation::parse_error
/// on failure.
///
/// @tparam T The success value type.
template <class T>
using parse_result = foundation::result<parse_state<T>>;
// 59712a9e-e890-4931-9196-61ceaa880b6d end

// 5f36b161-a5b2-45a9-82e4-4d4679a8a60e
/// A type-erased callable wrapper for a single-pass parser.
///
/// @c parser<F> wraps a callable @p F with signature
/// @c parse_result<T>(cursor). It satisfies @ref parser_like so it
/// composes with all the combinator functions in this header.
///
/// @tparam F Callable type; deduced via the deduction guide.
template <class F>
class parser {
  public:
    /// Constructs a parser wrapping @p f.
    constexpr explicit parser(F f) : f_{f} {}

    /// Runs the parser starting at @p cur.
    constexpr auto operator()(cursor cur) const { return f_(cur); }

  private:
    F f_;
};

/// Deduction guide: @c parser(f) deduces @c parser<F>.
template <class F>
parser(F) -> parser<F>;
// 5f36b161-a5b2-45a9-82e4-4d4679a8a60e end

// d6bea45d-0050-4c54-98e7-65e500510b3e
/// Returns a parser that always succeeds, consuming no input and yielding
/// @p value.
///
/// @tparam T Value type.
/// @param  value The constant value to produce.
template <class T>
[[nodiscard]] constexpr auto pure(T value) {
    return parser{[v = value](cursor cur) -> parse_result<T> {
        return parse_state<T>{v, cur};
    }};
}
// d6bea45d-0050-4c54-98e7-65e500510b3e end

// f954087d-8a4a-4e71-8f8d-dd63bc9a857a
/// Returns a parser that succeeds when the next character satisfies @p pred.
///
/// On success consumes one character. On failure reports @p expected at the
/// current position.
///
/// @param pred     Predicate on @c char.
/// @param expected Human-readable description of the expected token (used in
///                 @ref foundation::parse_error::message).
[[nodiscard]] constexpr auto satisfy(auto pred, char const *expected) {
    return parser{[pred, expected](cursor cur) -> parse_result<char> {
        if (!cur.empty() && pred(cur.peek())) {
            return parse_state<char>{cur.peek(), cur.bump()};
        }
        return foundation::parse_error{cur.position(), expected};
    }};
}

/// Returns a parser that matches exactly the character @p expected.
///
/// @param expected The character to match.
[[nodiscard]] constexpr auto char_p(char expected) {
    return satisfy([expected](char c) { return c == expected; },
                   "expected char");
}
// f954087d-8a4a-4e71-8f8d-dd63bc9a857a end

// 51e6f31e-12ad-4180-b7ca-9aa3e564fdbc
/// Returns a parser that applies @p f to the result of @p pa.
///
/// Fails with @p pa's error if @p pa fails; the error position is preserved
/// so the caller can decide whether to try alternatives.
///
/// @tparam PA Parser type.
/// @tparam F  Callable to apply to the parse value.
template <parser_like PA, class F>
[[nodiscard]] constexpr auto map(PA pa, F f) {
    return parser{[pa, f](cursor cur) {
        auto r = pa(cur);
        if (!r.has_value()) {
            using R = decltype(f(r.value().value));
            return parse_result<R>{r.error()};
        }
        using R = decltype(f(r.value().value));
        return parse_result<R>{
            parse_state<R>{f(r.value().value), r.value().rest}};
    }};
}
// 51e6f31e-12ad-4180-b7ca-9aa3e564fdbc end

// f446b18c-6afb-4bc1-9df7-ba1f5ea89046
/// Returns a parser that runs @p pa then @p pb in sequence, combining
/// their values with @p f.
///
/// Fails if either @p pa or @p pb fails, forwarding the earliest error.
///
/// @tparam PA Parser for the first value.
/// @tparam PB Parser for the second value.
/// @tparam F  Binary combiner callable.
template <parser_like PA, parser_like PB, class F>
[[nodiscard]] constexpr auto lift2(PA pa, PB pb, F f) {
    return parser{[pa, pb, f](cursor cur) {
        auto ra = pa(cur);
        if (!ra.has_value()) {
            using V = decltype(f(ra.value().value, pb(cur).value().value));
            return parse_result<V>{ra.error()};
        }
        auto rb = pb(ra.value().rest);
        if (!rb.has_value()) {
            using V = decltype(f(ra.value().value, rb.value().value));
            return parse_result<V>{rb.error()};
        }
        using V = decltype(f(ra.value().value, rb.value().value));
        return parse_result<V>{parse_state<V>{
            f(ra.value().value, rb.value().value), rb.value().rest}};
    }};
}

/// Returns a parser that runs @p pa then @p pb, discarding @p pb's value.
template <parser_like PA, parser_like PB>
[[nodiscard]] constexpr auto sequence_left(PA pa, PB pb) {
    return lift2(pa, pb, [](auto a, auto) { return a; });
}

/// Returns a parser that runs @p pa then @p pb, discarding @p pa's value.
template <parser_like PA, parser_like PB>
[[nodiscard]] constexpr auto sequence_right(PA pa, PB pb) {
    return lift2(pa, pb, [](auto, auto b) { return b; });
}
// f446b18c-6afb-4bc1-9df7-ba1f5ea89046 end

// 2a77ed5e-0863-4a2a-8ea0-1b03b56ffeb4
/// Ordered-choice combinator: tries @p pa; if it fails *at the same position*
/// it started, tries @p pb.
///
/// If @p pa consumes input before failing the error is propagated without
/// trying @p pb, which prevents accidental backtracking into already-consumed
/// tokens.
///
/// @tparam PA First alternative parser.
/// @tparam PB Second alternative parser.
template <parser_like PA, parser_like PB>
[[nodiscard]] constexpr auto operator|(PA pa, PB pb) {
    return parser{[pa, pb](cursor cur) {
        auto start = cur.position().offset;
        auto ra = pa(cur);
        if (ra.has_value())
            return ra;
        if (ra.error().where.offset != start)
            return ra;
        return pb(cur);
    }};
}
// 2a77ed5e-0863-4a2a-8ea0-1b03b56ffeb4 end

} // namespace smd::smdscheme::parser

#endif
