// src/smd/schemepoc/parser_ops.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_PARSER_OPS_HPP
#define SRC_SMD_SCHEMEPOC_PARSER_OPS_HPP

#include <smd/schemepoc/parser.hpp>
#include <smd/schemepoc/parser_alternative.hpp>

namespace smd::schemepoc {

struct parser_applicative_ops {
    template <class T>
    [[nodiscard]] constexpr auto pure(T value) const {
        return smd::schemepoc::pure(value);
    }

    template <class P, class F>
    [[nodiscard]] constexpr auto map(P p, F f) const {
        return smd::schemepoc::map(p, f);
    }

    template <class PF, class PA>
    [[nodiscard]] constexpr auto apply(PF pf, PA pa) const {
        return smd::schemepoc::lift2(
            pf, pa, [](auto fn, auto val) { return fn(val); });
    }

    template <class F, class PA, class PB>
    [[nodiscard]] constexpr auto lift2(F f, PA pa, PB pb) const {
        return smd::schemepoc::lift2(pa, pb, f);
    }

    template <class PLeft, class PRight>
    [[nodiscard]] constexpr auto sequence_left(PLeft left, PRight right) const {
        return smd::schemepoc::sequence_left(left, right);
    }

    template <class PLeft, class PRight>
    [[nodiscard]] constexpr auto sequence_right(PLeft left,
                                                PRight right) const {
        return smd::schemepoc::sequence_right(left, right);
    }
};

struct parser_alternative_ops {
    template <class PA, class PB>
    [[nodiscard]] constexpr auto alt(PA pa, PB pb) const {
        return smd::schemepoc::alt(pa, pb);
    }

    template <int Capacity, class P>
    [[nodiscard]] constexpr auto many(P p) const {
        return smd::schemepoc::many<Capacity>(p);
    }

    template <int Capacity, class P>
    [[nodiscard]] constexpr auto some(P p) const {
        return smd::schemepoc::some<Capacity>(p);
    }

    template <class P>
    [[nodiscard]] constexpr auto optional(P p) const {
        return smd::schemepoc::optional(p);
    }

    template <class P>
    [[nodiscard]] constexpr auto lexeme(P p) const {
        return smd::schemepoc::lexeme(p);
    }
};

struct parser_ops : parser_applicative_ops, parser_alternative_ops {};

inline constexpr parser_ops parser_v{};

} // namespace smd::schemepoc

#endif
