// src/smd/smdscheme/parser/reader_cursor.hpp                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_PARSER_READER_CURSOR_HPP
#define SRC_SMD_SMDSCHEME_PARSER_READER_CURSOR_HPP

#include <smd/smdscheme/foundation/source.hpp>

#include <string_view>

namespace smd::smdscheme::parser {

class cursor {
    std::string_view input_{};
    foundation::source_pos pos_{};

  public:
    constexpr explicit cursor(std::string_view input) : input_{input} {}

    constexpr auto empty() const -> bool { return input_.empty(); }

    constexpr auto peek() const -> char { return input_.front(); }

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

    constexpr auto position() const -> foundation::source_pos { return pos_; }

    constexpr auto remaining() const -> std::string_view { return input_; }
};

constexpr auto is_space(char c) -> bool {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

constexpr auto is_initial_symbol_char(char c) -> bool {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        return true;
    }
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '=' ||
           c == '<' || c == '>' || c == '!' || c == '?';
}

constexpr auto is_symbol_char(char c) -> bool {
    return is_initial_symbol_char(c) || (c >= '0' && c <= '9');
}

constexpr auto is_delimiter(char c) -> bool {
    return is_space(c) || c == '(' || c == ')' || c == '\'';
}

constexpr auto skip_intertoken_space(cursor cur) -> cursor {
    while (!cur.empty() && is_space(cur.peek())) {
        cur = cur.bump();
    }
    return cur;
}

} // namespace smd::smdscheme::parser

#endif
