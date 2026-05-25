// src/smd/schemepoc/parser.hpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_PARSER_HPP
#define SRC_SMD_SCHEMEPOC_PARSER_HPP

#include <smd/schemepoc/reader_cursor.hpp>
#include <smd/schemepoc/result.hpp>

namespace smd::schemepoc {

template <class T>
struct parse_state {
    T      value;
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

[[nodiscard]] constexpr auto satisfy(auto pred, char const* expected) {
    return parser{[pred, expected](cursor cur) -> parse_result<char> {
        if (!cur.empty() && pred(cur.peek())) {
            return parse_state<char>{cur.peek(), cur.bump()};
        }
        return parse_error{cur.position(), expected};
    }};
}

[[nodiscard]] constexpr auto char_p(char expected) {
    return satisfy([expected](char c) { return c == expected; }, "expected char");
}

} // namespace smd::schemepoc

#endif
