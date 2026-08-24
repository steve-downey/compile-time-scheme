// src/smd/kit/parser/parser.hpp                                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_KIT_PARSER_PARSER_HPP
#define SRC_SMD_KIT_PARSER_PARSER_HPP

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
/// @tparam F Callable type; deduced via the deduction guide.
template <class F>
class parser {
  public:
    /// Constructs a parser wrapping @p f.
    constexpr explicit parser(F f);

    /// Runs the parser starting at @p cur, threading @p ctx.
    template <parse_context Ctx>
    constexpr auto operator()(cursor cur, Ctx &ctx) const;

  private:
    F f_;
};

/// Deduction guide: @c parser(f) deduces @c parser<F>.
template <class F>
parser(F) -> parser<F>;

template <class F>
constexpr parser<F>::parser(F f) : f_{std::move(f)} {}

template <class F>
template <parse_context Ctx>
constexpr auto parser<F>::operator()(cursor cur, Ctx &ctx) const {
    return f_(cur, ctx);
}

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
/// Deliberately not a registered @c functor_typeclass<parser<F>> instance:
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

} // namespace smd::kit::parser

#endif
