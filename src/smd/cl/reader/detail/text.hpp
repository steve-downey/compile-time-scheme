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
#include <smd/kit/parser/parse_context.hpp>
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
/// The body is @ref smd::kit::parser::many_until over a step that owns the
/// accumulator and the capacity check — the division of labour B4's @ref
/// read_delimited established, where @c many_until supplies only "keep
/// asking until told to stop or handed a failure". What used to be an
/// escape flag threaded by hand through each turn of a @c while is now an
/// alternative inside the repeated parser: either a single escape and
/// whatever follows it taken literally, or any character at all. That is
/// the simplification this conversion buys; the loop had to remember
/// between iterations what the choice now says in one place.
///
/// The closing quote is the step's stopping condition, so it is tested
/// once, ahead of the alternatives, exactly as @ref read_delimited tests
/// for its closing delimiter. Neither alternative tests for it, which is
/// why the second one really is "any character" rather than "any character
/// that is not a quote".
template <reader_context Ctx>
[[nodiscard]] constexpr auto read_string(cursor cur, Ctx &ctx,
                                         foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    // One character, or the diagnostic the whole literal fails with when
    // input runs out. smd::kit::parser::satisfy is the primitive this
    // wants to be and cannot: satisfy reports at the cursor, and every
    // diagnostic here reports at the opening quote instead.
    auto const any_char = smd::kit::parser::parser{
        [where](cursor c, smd::kit::parser::parse_context auto &)
            -> smd::kit::parser::parse_result<char> {
            if (c.empty()) {
                return foundation::parse_error{where, "unterminated string"};
            }
            return parse_state<char>{c.peek(), c.bump()};
        }};
    // A single escape, and then whatever follows it. Failing *before* the
    // escape is consumed is what lets operator| fall through to any_char;
    // failing *after* it -- a literal ending in a bare backslash -- is a
    // committed failure that propagates instead, carrying any_char's
    // "unterminated string" rather than a diagnostic of its own, which is
    // what the hand-written loop's break did.
    auto const escaped_char =
        smd::kit::parser::parser{[any_char](cursor c, reader_context auto &rc)
                                     -> smd::kit::parser::parse_result<char> {
            if (c.empty() ||
                rc.table.syntax_of(c.peek()) != syntax_type::single_escape) {
                return foundation::parse_error{c.position(), "single escape"};
            }
            return any_char(c.bump(), rc);
        }};
    auto const string_char = escaped_char | any_char;

    datum_string contents{};
    auto const step = [&contents, string_char, where](cursor c,
                                                      reader_context auto &rc)
        -> foundation::result<parse_state<std::optional<char>>> {
        if (!c.empty() &&
            rc.table.macro_of(c.peek()) == macro_kind::double_quote) {
            return parse_state<std::optional<char>>{std::nullopt, c};
        }
        // Running out of room is one more way reading a character fails,
        // so the check chains after the character rather than standing in
        // a ladder beside it. It is `>=` before the append, so a literal
        // of exactly max_string_chars characters fits and one more does
        // not, and it reports at the opening quote rather than at the
        // character that would not fit.
        return and_then(
            string_char(c, rc),
            [&](parse_state<char> const &ch)
                -> foundation::result<parse_state<std::optional<char>>> {
                if (contents.length >= max_string_chars) {
                    return foundation::parse_error{where, "string too long"};
                }
                contents.storage[static_cast<std::size_t>(contents.length)] =
                    ch.value;
                ++contents.length;
                return parse_state<std::optional<char>>{ch.value, ch.rest};
            });
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
/// Whether the first character stands for itself or opens a character name
/// depends on the character *after* it, so the second parser is chosen by
/// the first one's result rather than fixed in advance: a @c bind. D28
/// (docs/cl-parser-scoping.md) asks that applicative composition stay the
/// default and @c bind be spent only where the grammar is really
/// context-dependent, because @c lift2 says something stronger about a
/// parser than @c and_then does and that information should not be
/// discarded where it holds. Here it does not hold, and this is what the
/// exception looks like.
template <reader_context Ctx>
[[nodiscard]] constexpr auto read_character(cursor cur, Ctx &ctx,
                                            foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    auto const first_char = smd::kit::parser::parser{
        [where](cursor c, smd::kit::parser::parse_context auto &)
            -> smd::kit::parser::parse_result<char> {
            if (c.empty()) {
                return foundation::parse_error{where,
                                               "expected character after #\\"};
            }
            return parse_state<char>{c.peek(), c.bump()};
        }};
    // The continuation. It holds the first character, so it is the first
    // thing here that can ask whether a name follows. The outer cursor
    // rides along because a name's source span is measured from the first
    // character, which by then is behind the parser's own cursor.
    auto const name_or_itself = [cur, where](char first) {
        return smd::kit::parser::parser{
            [cur, first, where](cursor after_first, reader_context auto &rc)
                -> foundation::result<parse_state<int>> {
                auto const finish =
                    [&](char value,
                        cursor rest) -> foundation::result<parse_state<int>> {
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
    };
    return bind(first_char, name_or_itself)(cur, ctx);
}
// 766d64ea-9ba2-46fb-a63c-b2ce52c6fe6f end

} // namespace smd::cl::reader::detail

#endif
