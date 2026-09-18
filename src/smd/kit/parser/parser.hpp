// src/smd/kit/parser/parser.hpp                                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_KIT_PARSER_PARSER_HPP
#define SRC_SMD_KIT_PARSER_PARSER_HPP

#include <smd/kit/foundation/parse_error.hpp>
#include <smd/kit/foundation/result.hpp>
#include <smd/kit/parser/cursor.hpp>
#include <smd/kit/parser/parse_context.hpp>

#include <type_traits>
#include <utility>

namespace smd::kit::parser {

/// The result of one parse step over a threaded context: a @ref parse_state
/// on success, a @c foundation::parse_error on failure.
///
/// @tparam T The parsed value type.
template <class T>
using parse_result = foundation::result<parse_state<T>>;

/// Concept satisfied by any callable @p P invocable with a @ref cursor and
/// a threaded context @p Ctx.
template <class P, class Ctx>
concept parser_like = requires(P p, cursor c, Ctx &ctx) { p(c, ctx); };

/// A type-erased callable wrapper for a single-pass, context-threaded
/// parser.
///
/// @c parser<F> wraps a callable @p F with signature
/// @c parse_result<T>(cursor, Ctx&) for some @ref parse_context @p Ctx. The
/// context is threaded rather than captured: combinators reify parsers into
/// storable values, and a captured @c Ctx& can outlive its context in a way
/// a per-call function cannot (D29, docs/cl-parser-scoping.md; DIV-0007 is
/// this project's own record of paying for a captured-reference dangling
/// bug once already).
///
/// A parser *is* its callable rather than holding one: @p F is a private
/// base and its @c operator() is re-exported, so running a parser costs one
/// constexpr call instead of two. That is not a micro-optimization. This
/// reader's recursion is ordinary C++ recursion, so every reified parser on
/// the path from @c read_node back to itself spends constant-evaluation
/// stack, and @c -fconstexpr-depth is a hard 512 by default: at step B7,
/// with the dispatch converted, a forwarding @c operator() put the
/// 64-deep nested-quote case in @c src/smd/cl/printer/prin1.test.cpp over
/// that limit. Re-exporting the call instead leaves it below where it
/// started.
///
/// The argument-order guard moves with the call rather than being lost:
/// @p F names its own context parameter, and every parser in this kit and
/// its one client spells it @c parse_context (or a refinement such as
/// @c smd::cl::reader::detail::reader_context, or a concrete context type),
/// so a @ref cursor passed where a context belongs is still a substitution
/// failure. What changes is where the constraint is written -- on the
/// callable that reads the context, which is the only place that can say
/// anything true about it -- not whether it is checked. A wrapper that
/// re-stated it could only do so by being a call, and a call is the cost
/// this removes.
///
/// @tparam F Callable type; deduced via the deduction guide.
template <class F>
class parser : private F {
  public:
    /// Constructs a parser wrapping @p f.
    constexpr explicit parser(F f);

    /// Runs the parser starting at @p cur, threading the context @p F
    /// itself names.
    using F::operator();
};

/// Deduction guide: @c parser(f) deduces @c parser<F>.
template <class F>
parser(F) -> parser<F>;

template <class F>
constexpr parser<F>::parser(F f) : F{std::move(f)} {}

/// Returns a parser that always succeeds, consuming no input and yielding
/// @p value, for any threaded context.
///
/// Implementor-facing rather than a CPO: @c monad.hpp's own doc comment
/// notes that @c pure cannot be dispatched from its argument, so a CPO
/// would have nothing to key on.
///
/// @tparam T Value type.
/// @param value The constant value to produce.
template <class T>
[[nodiscard]] constexpr auto pure(T value) {
    return parser{[value = std::move(value)](
                      cursor cur, parse_context auto &) -> parse_result<T> {
        return parse_state<T>{value, cur};
    }};
}

/// Returns a parser that applies @p f to the value @p p produces, passing
/// @p p's error through unchanged.
///
/// Deliberately not a registered @c foundation::functor<parser<F>> instance:
/// nothing outside this step needs a generic @c fmap over a parser value,
/// and a typeclass instance with no second caller is the over-eagerness
/// this project already learned to avoid (R8/DIV-0028). A real second
/// caller is an amendment; this step is not one.
///
/// @tparam P Parser type.
/// @tparam F Callable to apply to the parsed value.
template <class P, class F>
[[nodiscard]] constexpr auto map(P p, F f) {
    return parser{[p = std::move(p),
                   f = std::move(f)](cursor cur, parse_context auto &ctx) {
        auto const r = p(cur, ctx);
        using R = std::remove_cvref_t<decltype(f(r.value().value))>;
        if (!r.has_value()) {
            return parse_result<R>{r.error()};
        }
        return parse_result<R>{
            parse_state<R>{f(r.value().value), r.value().rest}};
    }};
}

/// Returns a parser that succeeds when the next character satisfies @p pred,
/// for any threaded context.
///
/// On success consumes one character. On failure reports @p expected at the
/// current position.
///
/// @tparam Pred Predicate on @c char.
/// @param pred     Predicate on @c char.
/// @param expected Human-readable description of the expected token (used in
///                 @ref foundation::parse_error::message).
template <class Pred>
[[nodiscard]] constexpr auto satisfy(Pred pred, char const *expected) {
    return parser{[pred = std::move(pred), expected](
                      cursor cur, parse_context auto &) -> parse_result<char> {
        if (!cur.empty() && pred(cur.peek())) {
            return parse_state<char>{cur.peek(), cur.bump()};
        }
        return foundation::parse_error{cur.position(), expected};
    }};
}

/// Returns a parser that matches exactly the character @p expected, for any
/// threaded context.
///
/// @param expected The character to match.
[[nodiscard]] constexpr auto char_p(char expected) {
    return satisfy([expected](char c) { return c == expected; },
                   "expected char");
}

} // namespace smd::kit::parser

#endif
