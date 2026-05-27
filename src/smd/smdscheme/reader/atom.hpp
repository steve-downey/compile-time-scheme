// src/smd/smdscheme/reader/atom.hpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_READER_ATOM_HPP
#define SRC_SMD_SMDSCHEME_READER_ATOM_HPP

#include <smd/smdscheme/parser/parser.hpp>
#include <smd/smdscheme/parser/parser_alternative.hpp>
#include <smd/smdscheme/parser/cursor.hpp>

#include <optional>
#include <string_view>
#include <variant>

namespace smd::smdscheme::reader {

struct atom_integer {
    int value;
};

struct atom_symbol {
    std::string_view name;
};

using atom = std::variant<atom_integer, atom_symbol>;

[[nodiscard]] constexpr auto integer_p() {
    return parser::parser{[](parser::cursor cur)
                              -> parser::parse_result<atom_integer> {
        auto sign_r = parser::optional(parser::char_p('-'))(cur);
        bool negative = sign_r.value().value.has_value();
        auto after_sign = sign_r.value().rest;

        auto digits = parser::some<20>(parser::satisfy(
            [](char c) { return c >= '0' && c <= '9'; }, "digit"))(after_sign);
        if (!digits.has_value()) {
            return parser::parse_result<atom_integer>{digits.error()};
        }
        auto &d = digits.value().value;
        auto rest = digits.value().rest;

        int sign = negative ? -1 : 1;
        int n = 0;
        for (int i = 0; i < d.size(); ++i)
            n = n * 10 + (d[i] - '0');
        return parser::parse_result<atom_integer>{
            parser::parse_state<atom_integer>{atom_integer{sign * n}, rest}};
    }};
}

[[nodiscard]] constexpr auto symbol_p() {
    return parser::parser{
        [](parser::cursor cur) -> parser::parse_result<atom_symbol> {
            auto start = cur;
            auto first =
                parser::satisfy(parser::is_initial_symbol_char, "symbol")(cur);
            if (!first.has_value())
                return parser::parse_result<atom_symbol>{first.error()};
            auto rest_cur = first.value().rest;
            auto tail = parser::many<64>(parser::satisfy(
                parser::is_symbol_char, "symbol char"))(rest_cur);
            auto end_cur = tail.value().rest;
            int len = end_cur.position().offset - start.position().offset;
            auto name = start.remaining().substr(0, len);
            return parser::parse_result<atom_symbol>{
                parser::parse_state<atom_symbol>{atom_symbol{name}, end_cur}};
        }};
}

} // namespace smd::smdscheme::reader

#endif
