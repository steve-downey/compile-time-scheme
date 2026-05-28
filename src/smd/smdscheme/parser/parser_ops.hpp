// src/smd/smdscheme/parser/parser_ops.hpp                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_PARSER_PARSER_OPS_HPP
#define SRC_SMD_SMDSCHEME_PARSER_PARSER_OPS_HPP

#include <smd/smdscheme/parser/alt.hpp>
#include <smd/smdscheme/parser/parser.hpp>

#include <type_traits>

namespace smd::smdscheme::parser {

// Layer 1: Impl — primitives that delegate to the free functions in parser.hpp
//          and parser_alternative.hpp.  An alternate Impl could provide
//          different parsing strategies.

struct parser_applicative_impl {
    template <class T>
    [[nodiscard]] constexpr auto pure(this auto &&, T value) {
        return ::smd::smdscheme::parser::pure(value);
    }

    template <class P, class F>
    [[nodiscard]] constexpr auto map(this auto &&, P p, F f) {
        return ::smd::smdscheme::parser::map(p, f);
    }

    template <class PA, class PB, class F>
    [[nodiscard]] constexpr auto lift2(this auto &&, PA pa, PB pb, F f) {
        return ::smd::smdscheme::parser::lift2(pa, pb, f);
    }
};

struct parser_alternative_impl {
    template <class PA, class PB>
    [[nodiscard]] constexpr auto alt(this auto &&, PA pa, PB pb) {
        return ::smd::smdscheme::parser::alt(pa, pb);
    }
};

// Layer 2: CRTP bases — derive convenience operations from the primitives.

template <class Impl>
struct ParserApplicative : protected Impl {
    using Impl::lift2;
    using Impl::map;
    using Impl::pure;

    template <class PF, class PA>
    [[nodiscard]] constexpr auto apply(this auto &&self, PF pf, PA pa) {
        return self.lift2(pf, pa, [](auto fn, auto val) { return fn(val); });
    }

    template <class PLeft, class PRight>
    [[nodiscard]] constexpr auto sequence_left(this auto &&self, PLeft left,
                                               PRight right) {
        return self.lift2(left, right, [](auto a, auto) { return a; });
    }

    template <class PLeft, class PRight>
    [[nodiscard]] constexpr auto sequence_right(this auto &&self, PLeft left,
                                                PRight right) {
        return self.lift2(left, right, [](auto, auto b) { return b; });
    }
};

template <class Impl>
struct ParserAlternative : protected Impl {
    using Impl::alt;

    template <int Capacity, class P>
    [[nodiscard]] constexpr auto many(this auto &&, P p) {
        return ::smd::smdscheme::parser::many<Capacity>(p);
    }

    template <int Capacity, class P>
    [[nodiscard]] constexpr auto some(this auto &&, P p) {
        return ::smd::smdscheme::parser::some<Capacity>(p);
    }

    template <class P>
    [[nodiscard]] constexpr auto optional(this auto &&, P p) {
        return ::smd::smdscheme::parser::optional(p);
    }

    template <class P>
    [[nodiscard]] constexpr auto lexeme(this auto &&, P p) {
        return ::smd::smdscheme::parser::lexeme(p);
    }
};

// Layer 3: Map — combines both bases, selects primitives via using.

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

// Extension point: specialize for parser-like types.
template <class T>
inline constexpr auto parser_typeclass = std::false_type{};

template <class F>
inline constexpr auto parser_typeclass<parser<F>> = parser_ops{};

inline constexpr parser_ops parser_v{};

} // namespace smd::smdscheme::parser

#endif
