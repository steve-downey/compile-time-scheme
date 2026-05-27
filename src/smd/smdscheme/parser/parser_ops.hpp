// src/smd/schemepoc/parser_ops.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_PARSER_PARSER_OPS_HPP
#define SRC_SMD_SMDSCHEME_PARSER_PARSER_OPS_HPP

#include <smd/smdscheme/parser/parser.hpp>
#include <smd/smdscheme/parser/parser_alternative.hpp>

namespace smd::smdscheme::parser {

struct parser_applicative_ops {
    template <class T>
    [[nodiscard]] constexpr auto pure(T value) const {
        return ::smd::smdscheme::parser::pure(value);
    }

    template <class P, class F>
    [[nodiscard]] constexpr auto map(P p, F f) const {
        return ::smd::smdscheme::parser::map(p, f);
    }

    template <class PF, class PA>
    [[nodiscard]] constexpr auto apply(PF pf, PA pa) const {
        return ::smd::smdscheme::parser::lift2(
            pf, pa, [](auto fn, auto val) { return fn(val); });
    }

    template <class F, class PA, class PB>
    [[nodiscard]] constexpr auto lift2(F f, PA pa, PB pb) const {
        return ::smd::smdscheme::parser::lift2(pa, pb, f);
    }

    template <class PLeft, class PRight>
    [[nodiscard]] constexpr auto sequence_left(PLeft left, PRight right) const {
        return ::smd::smdscheme::parser::sequence_left(left, right);
    }

    template <class PLeft, class PRight>
    [[nodiscard]] constexpr auto sequence_right(PLeft left,
                                                PRight right) const {
        return ::smd::smdscheme::parser::sequence_right(left, right);
    }
};

struct parser_alternative_ops {
    template <class PA, class PB>
    [[nodiscard]] constexpr auto alt(PA pa, PB pb) const {
        return ::smd::smdscheme::parser::alt(pa, pb);
    }

    template <int Capacity, class P>
    [[nodiscard]] constexpr auto many(P p) const {
        return ::smd::smdscheme::parser::many<Capacity>(p);
    }

    template <int Capacity, class P>
    [[nodiscard]] constexpr auto some(P p) const {
        return ::smd::smdscheme::parser::some<Capacity>(p);
    }

    template <class P>
    [[nodiscard]] constexpr auto optional(P p) const {
        return ::smd::smdscheme::parser::optional(p);
    }

    template <class P>
    [[nodiscard]] constexpr auto lexeme(P p) const {
        return ::smd::smdscheme::parser::lexeme(p);
    }
};

struct parser_ops : parser_applicative_ops, parser_alternative_ops {};

inline constexpr parser_ops parser_v{};

} // namespace smd::smdscheme::parser

#endif
