// src/smd/smdscheme/parser/parser_ops.hpp                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_PARSER_PARSER_OPS_HPP
#define SRC_SMD_SMDSCHEME_PARSER_PARSER_OPS_HPP

#include <smd/smdscheme/parser/alt.hpp>
#include <smd/smdscheme/parser/parser.hpp>

#include <type_traits>

namespace smd::smdscheme::parser {

/// Layer 1: Impl — primitives that delegate to the free functions in parser.hpp
///          and parser_alternative.hpp.  An alternate Impl could provide
///          different parsing strategies.

/// Applicative primitives for parsers: @c pure, @c map, @c lift2.
///
/// This is Layer 1 of the typeclass-object pattern for parsers.  All members
/// delegate to the free-function combinators in @c parser.hpp so a different
/// @c Impl could substitute alternate strategies.
struct parser_applicative_impl {
    /// Succeeds unconditionally, yielding @p value and consuming no input.
    template <class T>
    [[nodiscard]] constexpr auto pure(this auto &&, T value) {
        return ::smd::smdscheme::parser::pure(value);
    }

    /// Applies @p f to the value produced by @p p.
    template <class P, class F>
    [[nodiscard]] constexpr auto map(this auto &&, P p, F f) {
        return ::smd::smdscheme::parser::map(p, f);
    }

    /// Runs @p pa then @p pb and combines their values with @p f.
    template <class PA, class PB, class F>
    [[nodiscard]] constexpr auto lift2(this auto &&, PA pa, PB pb, F f) {
        return ::smd::smdscheme::parser::lift2(pa, pb, f);
    }
};

/// Alternative primitive for parsers: @c alt.
///
/// Delegates to the ordered-choice @c alt combinator.
struct parser_alternative_impl {
    /// Tries @p pa; if it fails without consuming input, tries @p pb.
    template <class PA, class PB>
    [[nodiscard]] constexpr auto alt(this auto &&, PA pa, PB pb) {
        return ::smd::smdscheme::parser::alt(pa, pb);
    }
};

/// CRTP base that derives applicative parser operations from @c pure/@c map/@c
/// lift2.
///
/// @tparam Impl Concrete implementation; typically @ref
/// parser_applicative_impl.
template <class Impl>
struct ParserApplicative : protected Impl {
    using Impl::lift2;
    using Impl::map;
    using Impl::pure;

    /// Applies a parser-wrapped function @p pf to the value produced by @p pa.
    template <class PF, class PA>
    [[nodiscard]] constexpr auto apply(this auto &&self, PF pf, PA pa) {
        return self.lift2(pf, pa, [](auto fn, auto val) { return fn(val); });
    }

    /// Runs @p left then @p right; returns @p left's value, discarding @p
    /// right's.
    template <class PLeft, class PRight>
    [[nodiscard]] constexpr auto sequence_left(this auto &&self, PLeft left,
                                               PRight right) {
        return self.lift2(left, right, [](auto a, auto) { return a; });
    }

    /// Runs @p left then @p right; returns @p right's value, discarding @p
    /// left's.
    template <class PLeft, class PRight>
    [[nodiscard]] constexpr auto sequence_right(this auto &&self, PLeft left,
                                                PRight right) {
        return self.lift2(left, right, [](auto, auto b) { return b; });
    }
};

/// CRTP base that derives alternative parser operations from @c alt.
///
/// @tparam Impl Concrete implementation; typically @ref
/// parser_alternative_impl.
template <class Impl>
struct ParserAlternative : protected Impl {
    using Impl::alt;

    /// Applies @p p zero or more times, collecting up to @c Capacity results.
    template <int Capacity, class P>
    [[nodiscard]] constexpr auto many(this auto &&, P p) {
        return ::smd::smdscheme::parser::many<Capacity>(p);
    }

    /// Applies @p p one or more times; fails if @p p does not match at least
    /// once.
    template <int Capacity, class P>
    [[nodiscard]] constexpr auto some(this auto &&, P p) {
        return ::smd::smdscheme::parser::some<Capacity>(p);
    }

    /// Tries @p p, succeeding with @c std::nullopt if it fails without
    /// consuming.
    template <class P>
    [[nodiscard]] constexpr auto optional(this auto &&, P p) {
        return ::smd::smdscheme::parser::optional(p);
    }

    /// Wraps @p p so it skips surrounding inter-token whitespace.
    template <class P>
    [[nodiscard]] constexpr auto lexeme(this auto &&, P p) {
        return ::smd::smdscheme::parser::lexeme(p);
    }
};

/// Combined parser operations object inheriting both applicative and
/// alternative operations from their respective CRTP bases.
///
/// This is the "Map" in the typeclass-object pattern: it selects primitives
/// via @c using and exposes the full derived surface to callers.
struct parser_ops : ParserApplicative<parser_applicative_impl>,
                    ParserAlternative<parser_alternative_impl> {
    using ParserApplicative<parser_applicative_impl>::pure;
    using ParserApplicative<parser_applicative_impl>::map;
    using ParserApplicative<parser_applicative_impl>::lift2;
    using ParserApplicative<parser_applicative_impl>::apply;
    using ParserApplicative<parser_applicative_impl>::sequence_left;
    using ParserApplicative<parser_applicative_impl>::sequence_right;
    using ParserAlternative<parser_alternative_impl>::alt;
    using ParserAlternative<parser_alternative_impl>::many;
    using ParserAlternative<parser_alternative_impl>::some;
    using ParserAlternative<parser_alternative_impl>::optional;
    using ParserAlternative<parser_alternative_impl>::lexeme;
};

/// Typeclass lookup variable for parser-like types; specialize for custom
/// types.
///
/// Defaults to @c std::false_type{}; specialized for @c parser<F> below.
template <class T>
inline constexpr auto parser_typeclass = std::false_type{};

/// Registers every @c parser<F> as having @ref parser_ops behavior.
template <class F>
inline constexpr auto parser_typeclass<parser<F>> = parser_ops{};

/// A global, default-constructed @c parser_ops instance for direct use.
inline constexpr parser_ops parser_v{};

} // namespace smd::smdscheme::parser

#endif
