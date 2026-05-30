// src/smd/smdscheme/reader/atom.hpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_READER_ATOM_HPP
#define SRC_SMD_SMDSCHEME_READER_ATOM_HPP

#include <smd/smdscheme/parser/alt.hpp>
#include <smd/smdscheme/parser/cursor.hpp>
#include <smd/smdscheme/parser/parser.hpp>

#include <optional>
#include <string_view>
#include <variant>

namespace smd::smdscheme::reader {

/// A Scheme integer literal, e.g. @c 42 or @c -7.
struct atom_integer {
    int value; ///< The parsed integer value.
};

/// A Scheme symbol (identifier), e.g. @c foo or @c +.
struct atom_symbol {
    std::string_view name; ///< The symbol text as a view into the source.
};

/// A Scheme atom: either an integer or a symbol.
using atom = std::variant<atom_integer, atom_symbol>;

/// Returns a parser for a Scheme integer literal.
///
/// Handles an optional leading @c - sign followed by one or more decimal
/// digits (up to 20 digits). The value is computed without allocating
/// intermediate strings.
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

/// Returns a parser for a Scheme symbol.
///
/// A symbol starts with a letter or operator character (per
/// @ref parser::is_initial_symbol_char) and continues with zero or more
/// symbol characters (per @ref parser::is_symbol_char). Up to 64 tail
/// characters are accepted. The returned @c atom_symbol::name is a view
/// into the original source.
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
