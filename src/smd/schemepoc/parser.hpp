// src/smd/schemepoc/parser.hpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_PARSER_HPP
#define SRC_SMD_SCHEMEPOC_PARSER_HPP

#include <smd/schemepoc/reader_cursor.hpp>
#include <smd/schemepoc/result.hpp>

namespace smd::schemepoc {

template <class T>
struct parse_state {
    T value;
    cursor rest;
};

template <class T>
using parse_result = result<parse_state<T>>;

template <class F>
class parser {
  public:
    constexpr explicit parser(F f) : f_{f} {}

    constexpr auto operator()(cursor cur) const { return f_(cur); }

  private:
    F f_;
};

template <class F>
parser(F) -> parser<F>;

template <class T>
[[nodiscard]] constexpr auto pure(T value) {
    return parser{[v = value](cursor cur) -> parse_result<T> {
        return parse_state<T>{v, cur};
    }};
}

[[nodiscard]] constexpr auto satisfy(auto pred, char const *expected) {
    return parser{[pred, expected](cursor cur) -> parse_result<char> {
        if (!cur.empty() && pred(cur.peek())) {
            return parse_state<char>{cur.peek(), cur.bump()};
        }
        return parse_error{cur.position(), expected};
    }};
}

[[nodiscard]] constexpr auto char_p(char expected) {
    return satisfy([expected](char c) { return c == expected; },
                   "expected char");
}

template <class PA, class F>
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

template <class PA, class PB, class F>
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

template <class PA, class PB>
[[nodiscard]] constexpr auto sequence_left(PA pa, PB pb) {
    return lift2(pa, pb, [](auto a, auto) { return a; });
}

template <class PA, class PB>
[[nodiscard]] constexpr auto sequence_right(PA pa, PB pb) {
    return lift2(pa, pb, [](auto, auto b) { return b; });
}

template <class PA, class PB>
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

} // namespace smd::schemepoc

#endif
