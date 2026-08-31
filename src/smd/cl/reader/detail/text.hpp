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
#include <smd/kit/parser/choice.hpp>
#include <smd/kit/parser/parser.hpp>
#include <smd/kit/parser/parser_instances.hpp>
#include <smd/kit/parser/repeat.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <variant>

namespace smd::cl::reader::detail {

// 4a777d67-1246-4233-bf78-73aa4c23257a
/// Reads a string literal (the opening `"` already consumed): characters
/// verbatim, `\c` taking @c c literally, until the closing `"`.
///
/// The body is @ref smd::kit::parser::many_until of a step that owns the
/// accumulator and the capacity check, the same division of labour B4's
/// @c read_delimited established: @c many_until supplies only "keep asking
/// until told to stop or handed a failure." What used to be a flag
/// (escaping) threaded by hand through each loop iteration is now the
/// step's own choice between two small parsers -- a single escape followed
/// by any character taken literally, or any character that is not the
/// closing quote -- composed with @ref smd::kit::parser::operator|, this
/// combinator's first production consumer (B4 landed it with law tests
/// only).
template <reader_context Ctx>
[[nodiscard]] constexpr auto read_string(cursor cur, Ctx &ctx,
                                         foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    auto const escaped_char = smd::kit::parser::parser{
        [](cursor c,
           reader_context auto &rc) -> smd::kit::parser::parse_result<char> {
            if (c.empty() ||
                rc.table.syntax_of(c.peek()) != syntax_type::single_escape) {
                return foundation::parse_error{c.position(), "escape"};
            }
            cursor const after = c.bump();
            if (after.empty()) {
                return foundation::parse_error{after.position(),
                                               "character after escape"};
            }
            return parse_state<char>{after.peek(), after.bump()};
        }};
    auto const ordinary_char = smd::kit::parser::parser{
        [](cursor c,
           reader_context auto &rc) -> smd::kit::parser::parse_result<char> {
            if (c.empty() ||
                rc.table.macro_of(c.peek()) == macro_kind::double_quote) {
                return foundation::parse_error{c.position(),
                                               "non-quote character"};
            }
            return parse_state<char>{c.peek(), c.bump()};
        }};
    auto const string_char = escaped_char | ordinary_char;

    datum_string contents{};
    auto const step = [&contents, &string_char, where](cursor c,
                                                       reader_context auto &rc)
        -> foundation::result<parse_state<std::optional<char>>> {
        if (c.empty()) {
            return foundation::parse_error{where, "unterminated string"};
        }
        if (rc.table.macro_of(c.peek()) == macro_kind::double_quote) {
            return parse_state<std::optional<char>>{std::nullopt, c};
        }
        // c is non-empty and not the closing quote, so ordinary_char alone
        // always matches it; the only way this can fail is a single escape
        // with nothing after it, which falls out the same way the
        // hand-written loop did: "unterminated string", not a diagnostic
        // of its own.
        auto const matched = string_char(c, rc);
        if (!matched.has_value()) {
            return foundation::parse_error{where, "unterminated string"};
        }
        if (contents.length >= max_string_chars) {
            return foundation::parse_error{where, "string too long"};
        }
        contents.storage[static_cast<std::size_t>(contents.length)] =
            matched.value().value;
        ++contents.length;
        return parse_state<std::optional<char>>{matched.value().value,
                                                matched.value().rest};
    };
    return and_then(
        smd::kit::parser::many_until(step)(cur, ctx),
        [&](parse_state<std::monostate> const &finished)
            -> foundation::result<parse_state<int>> {
            return and_then(
                add_leaf_checked(ctx, datum_atom{contents}, where),
                [&](int id) -> foundation::result<parse_state<int>> {
                    return parse_state<int>{id, finished.rest.bump()};
                });
        });
}
// 4a777d67-1246-4233-bf78-73aa4c23257a end

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

// 766d64ea-9ba2-46fb-a63c-b2ce52c6fe6f
/// Reads a character literal (the `#\` already consumed): the next
/// character itself, case-sensitively, unless it begins a run of
/// constituents — then a character name, case-insensitively (ANSI 2.4.8.1).
///
/// Whether to read the first character alone or a character name is
/// genuinely context-dependent -- it depends on the character *after* the
/// first -- so this is @c bind, not a choice between two parsers (D28,
/// docs/cl-parser-scoping.md): the continuation only decides which parser
/// runs next once it has seen the first character, which is exactly what
/// @c lift2's fixed-in-advance composition cannot express.
template <reader_context Ctx>
[[nodiscard]] constexpr auto read_character(cursor cur, Ctx &ctx,
                                            foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    auto const first_char = smd::kit::parser::parser{
        [where](cursor c,
                reader_context auto &) -> smd::kit::parser::parse_result<char> {
            if (c.empty()) {
                return foundation::parse_error{where,
                                               "expected character after #\\"};
            }
            return parse_state<char>{c.peek(), c.bump()};
        }};
    return bind(first_char, [cur, where](char first) {
        return smd::kit::parser::parser{
            [cur, first, where](cursor after_first, reader_context auto &rc)
                -> foundation::result<parse_state<int>> {
                auto const finish = [&](char value, cursor rest) {
                    return and_then(
                        add_leaf_checked(rc, datum_atom{datum_character{value}},
                                         where),
                        [&](int id) -> foundation::result<parse_state<int>> {
                            return parse_state<int>{id, rest};
                        });
                };
                bool const named =
                    rc.table.syntax_of(first) == syntax_type::constituent &&
                    !after_first.empty() &&
                    rc.table.syntax_of(after_first.peek()) ==
                        syntax_type::constituent;
                if (!named) {
                    return finish(first, after_first);
                }
                cursor const name_end =
                    advance_while(after_first, [&rc](char c) {
                        return rc.table.syntax_of(c) ==
                               syntax_type::constituent;
                    });
                auto const raw = cur.remaining().substr(
                    0, static_cast<std::size_t>(name_end.position().offset -
                                                cur.position().offset));
                if (auto const value = lookup_char_name(raw)) {
                    return finish(*value, name_end);
                }
                return foundation::parse_error{where, "unknown character name"};
            }};
    })(cur, ctx);
}
// 766d64ea-9ba2-46fb-a63c-b2ce52c6fe6f end

} // namespace smd::cl::reader::detail

#endif
