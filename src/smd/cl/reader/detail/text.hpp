// src/smd/cl/reader/detail/text.hpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_READER_DETAIL_TEXT_HPP
#define SRC_SMD_CL_READER_DETAIL_TEXT_HPP

#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/source_pos.hpp>
#include <smd/cl/reader/cursor.hpp>
#include <smd/cl/reader/datum.hpp>
#include <smd/cl/reader/detail/read_context.hpp>
#include <smd/cl/reader/readtable.hpp>
#include <smd/cl/reader/token.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace smd::cl::reader::detail {

/// Reads a string literal (the opening `"` already consumed): characters
/// verbatim, `\c` taking @c c literally, until the closing `"`.
template <class Ctx>
[[nodiscard]] constexpr auto read_string(cursor cur, Ctx &ctx,
                                         foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    datum_string contents{};
    // Substrate generic algorithm: the string accumulator; the escape
    // state makes each step depend on the previous character.
    while (!cur.empty()) {
        char c = cur.peek();
        if (ctx.table.macro_of(c) == macro_kind::double_quote) {
            return and_then(
                add_leaf_checked(ctx, datum_atom{contents}, where),
                [&](int id) -> foundation::result<parse_state<int>> {
                    return parse_state<int>{id, cur.bump()};
                });
        }
        if (ctx.table.syntax_of(c) == syntax_type::single_escape) {
            cur = cur.bump();
            if (cur.empty()) {
                break;
            }
            c = cur.peek();
        }
        if (contents.length >= max_string_chars) {
            return foundation::parse_error{where, "string too long"};
        }
        contents.storage[static_cast<std::size_t>(contents.length)] = c;
        ++contents.length;
        cur = cur.bump();
    }
    return foundation::parse_error{where, "unterminated string"};
}

/// One standard character name and its character.
struct named_char {
    std::string_view name;
    char value;
};

/// The character names this reader recognizes: the two ANSI-required
/// names, the ANSI semi-standard names, and NUL/NULL.
inline constexpr std::array<named_char, 10> named_chars{{
    {"SPACE", ' '},
    {"NEWLINE", '\n'},
    {"TAB", '\t'},
    {"PAGE", '\f'},
    {"RETURN", '\r'},
    {"LINEFEED", '\n'},
    {"BACKSPACE", '\b'},
    {"RUBOUT", '\x7f'},
    {"NUL", '\0'},
    {"NULL", '\0'},
}};

/// Case-insensitively looks up @p raw among @ref named_chars.
[[nodiscard]] constexpr auto lookup_char_name(std::string_view raw)
    -> std::optional<char> {
    auto const matches = [raw](named_char const &candidate) {
        return std::ranges::equal(raw, candidate.name, [](char a, char b) {
            return to_upper_char(a) == b;
        });
    };
    auto const found = std::ranges::find_if(named_chars, matches);
    if (found == named_chars.end()) {
        return std::nullopt;
    }
    return found->value;
}

/// Reads a character literal (the `#\` already consumed): the next
/// character itself, case-sensitively, unless it begins a run of
/// constituents — then a character name, case-insensitively (ANSI 2.4.8.1).
template <class Ctx>
[[nodiscard]] constexpr auto read_character(cursor cur, Ctx &ctx,
                                            foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    if (cur.empty()) {
        return foundation::parse_error{where, "expected character after #\\"};
    }
    char const first = cur.peek();
    cursor const after_first = cur.bump();
    bool const named =
        ctx.table.syntax_of(first) == syntax_type::constituent &&
        !after_first.empty() &&
        ctx.table.syntax_of(after_first.peek()) == syntax_type::constituent;
    auto const finish = [&](char value, cursor rest) {
        return and_then(
            add_leaf_checked(ctx, datum_atom{datum_character{value}}, where),
            [&](int id) -> foundation::result<parse_state<int>> {
                return parse_state<int>{id, rest};
            });
    };
    if (!named) {
        return finish(first, after_first);
    }
    cursor const name_end = advance_while(after_first, [&ctx](char c) {
        return ctx.table.syntax_of(c) == syntax_type::constituent;
    });
    auto const raw = cur.remaining().substr(
        0, static_cast<std::size_t>(name_end.position().offset -
                                    cur.position().offset));
    if (auto const value = lookup_char_name(raw)) {
        return finish(*value, name_end);
    }
    return foundation::parse_error{where, "unknown character name"};
}

} // namespace smd::cl::reader::detail

#endif
