// src/smd/schemepoc/reader_atom.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_READER_ATOM_HPP
#define SRC_SMD_SCHEMEPOC_READER_ATOM_HPP

#include <smd/schemepoc/parser.hpp>
#include <smd/schemepoc/parser_alternative.hpp>
#include <smd/schemepoc/reader_cursor.hpp>

#include <optional>
#include <string_view>
#include <variant>

namespace smd::schemepoc {

struct atom_integer {
    int value;
};

struct atom_symbol {
    std::string_view name;
};

using atom = std::variant<atom_integer, atom_symbol>;

[[nodiscard]] constexpr auto integer_p() {
    return parser{[](cursor cur) -> parse_result<atom_integer> {
        auto sign_r = optional(char_p('-'))(cur);
        bool negative = sign_r.value().value.has_value();
        auto after_sign = sign_r.value().rest;

        auto digits = some<20>(satisfy(
            [](char c) { return c >= '0' && c <= '9'; }, "digit"))(after_sign);
        if (!digits.has_value()) {
            return parse_result<atom_integer>{digits.error()};
        }
        auto &d = digits.value().value;
        auto rest = digits.value().rest;

        int sign = negative ? -1 : 1;
        int n = 0;
        for (int i = 0; i < d.size(); ++i)
            n = n * 10 + (d[i] - '0');
        return parse_result<atom_integer>{
            parse_state<atom_integer>{atom_integer{sign * n}, rest}};
    }};
}

[[nodiscard]] constexpr auto symbol_p() {
    return parser{[](cursor cur) -> parse_result<atom_symbol> {
        auto start = cur;
        auto first = satisfy(is_initial_symbol_char, "symbol")(cur);
        if (!first.has_value())
            return parse_result<atom_symbol>{first.error()};
        auto rest_cur = first.value().rest;
        auto tail = many<64>(satisfy(is_symbol_char, "symbol char"))(rest_cur);
        auto end_cur = tail.value().rest;
        int len = end_cur.position().offset - start.position().offset;
        auto name = start.remaining().substr(0, len);
        return parse_result<atom_symbol>{
            parse_state<atom_symbol>{atom_symbol{name}, end_cur}};
    }};
}

} // namespace smd::schemepoc

#endif
