// src/smd/smdscheme/parser/cursor.hpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_PARSER_CURSOR_HPP
#define SRC_SMD_SMDSCHEME_PARSER_CURSOR_HPP

#include <smd/smdscheme/foundation/source_pos.hpp>

#include <string_view>

namespace smd::smdscheme::parser {

/// An immutable view into the remaining input with an associated source
/// position.
///
/// All advancing operations return a new @c cursor rather than mutating this
/// one, so parsers can checkpoint and backtrack freely.
class cursor {
    std::string_view input_{};
    foundation::source_pos pos_{};

  public:
    /// Constructs a cursor at the beginning of @p input.
    constexpr explicit cursor(std::string_view input) : input_{input} {}

    /// Returns true when no input remains.
    constexpr auto empty() const -> bool { return input_.empty(); }

    /// Returns the next character without consuming it.
    /// @pre !empty()
    constexpr auto peek() const -> char { return input_.front(); }

    /// Returns a new cursor that has consumed the next character, updating
    /// the position (line/column/offset).
    /// @pre !empty()
    constexpr auto bump() const -> cursor {
        cursor next{*this};
        if (!input_.empty()) {
            char c = input_.front();
            next.input_.remove_prefix(1);
            ++next.pos_.offset;
            if (c == '\n') {
                ++next.pos_.line;
                next.pos_.column = 1;
            } else {
                ++next.pos_.column;
            }
        }
        return next;
    }

    /// Returns the current source position.
    constexpr auto position() const -> foundation::source_pos { return pos_; }

    /// Returns the unconsumed portion of the input as a string_view.
    constexpr auto remaining() const -> std::string_view { return input_; }
};

/// Returns true if @p c is ASCII whitespace.
constexpr auto is_space(char c) -> bool {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/// Returns true if @p c is a valid first character of a Scheme symbol.
///
/// Allows letters and the operator characters @c + @c - @c * @c / @c =
/// @c < @c > @c ! @c ?.
constexpr auto is_initial_symbol_char(char c) -> bool {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        return true;
    }
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '=' ||
           c == '<' || c == '>' || c == '!' || c == '?';
}

/// Returns true if @p c may appear anywhere (including the first position)
/// in a Scheme symbol — letter, operator character, or decimal digit.
constexpr auto is_symbol_char(char c) -> bool {
    return is_initial_symbol_char(c) || (c >= '0' && c <= '9');
}

/// Returns true if @p c is a token boundary in Scheme source:
/// whitespace, @c ( @c ) or @c '.
constexpr auto is_delimiter(char c) -> bool {
    return is_space(c) || c == '(' || c == ')' || c == '\'';
}

/// Advances @p cur past all leading whitespace, returning the updated cursor.
constexpr auto skip_intertoken_space(cursor cur) -> cursor {
    while (!cur.empty() && is_space(cur.peek())) {
        cur = cur.bump();
    }
    return cur;
}

} // namespace smd::smdscheme::parser

#endif
