// src/smd/smdlisp/reader/cl_chars.hpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_READER_CL_CHARS_HPP
#define SRC_SMD_SMDLISP_READER_CL_CHARS_HPP

#include <smd/smdscheme/parser/cursor.hpp>

namespace smd::smdlisp::reader {

/// Returns true if @p c is Common Lisp whitespace[1]: space, tab, newline,
/// carriage return, line feed, or page (form feed).
///
/// Reuses @c smdscheme::parser::cursor unchanged (per docs/cl-pivot-plan.md
/// decision D1); this header supplies the CL-flavored character predicates
/// that stand in for the Scheme ones in
/// @c smdscheme/parser/cursor.hpp (@c is_space, @c is_initial_symbol_char,
/// @c is_symbol_char, @c is_delimiter, @c skip_intertoken_space).
constexpr auto is_whitespace(char c) -> bool {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

/// Returns true if @p c is one of the terminating macro characters this
/// reader subset recognizes: @c ( @c ) @c ' @c ; @c ` @c ,.
///
/// ANSI CL also assigns terminating-macro-character status to @c " and
/// @c #, but those arrive with later steps (string support;
/// docs/cl-pivot-plan.md step L6 already gives @c # sharpsign-quote
/// dispatch without needing terminating-macro-char status, since @c #'x
/// is always read at the start of a datum, never mid-token). @c ` and
/// @c , (backquote/unquote) join the terminating set in step L18.
constexpr auto is_terminating_macro_char(char c) -> bool {
    return c == '(' || c == ')' || c == '\'' || c == ';' || c == '`' ||
           c == ',';
}

/// Returns true if @p c is a constituent character: one that may appear in a
/// symbol or number token, i.e. anything that is neither whitespace nor a
/// terminating macro character.
constexpr auto is_constituent_char(char c) -> bool {
    return !is_whitespace(c) && !is_terminating_macro_char(c);
}

/// Returns true if @p c is a token boundary: whitespace or a terminating
/// macro character. Mirrors @c smdscheme::parser::is_delimiter.
constexpr auto is_delimiter(char c) -> bool {
    return is_whitespace(c) || is_terminating_macro_char(c);
}

// b56c684b-835e-471d-b113-ab07741071eb
/// Case-folds a single character to uppercase, ASCII only.
///
/// Per docs/cl-pivot-plan.md decision D2, @c smdlisp folds unescaped symbol
/// text to uppercase at read time, matching the ANSI default readtable; this
/// is the single-character primitive that step L5's symbol reader folds
/// with.
constexpr auto to_upper_char(char c) -> char {
    if (c >= 'a' && c <= 'z') {
        return static_cast<char>(c - 'a' + 'A');
    }
    return c;
}
// b56c684b-835e-471d-b113-ab07741071eb end

// 2bc98e28-002e-4db3-b4de-a8967e5d8cec
/// Advances @p cur past a @c ; line comment, consuming through (but not
/// including) the next newline, or through end of input if none remains.
/// @pre !cur.empty() && cur.peek() == ';'
constexpr auto skip_line_comment(smdscheme::parser::cursor cur)
    -> smdscheme::parser::cursor {
    while (!cur.empty() && cur.peek() != '\n') {
        cur = cur.bump();
    }
    return cur;
}
// 2bc98e28-002e-4db3-b4de-a8967e5d8cec end

/// Advances @p cur past all leading intertoken space: interleaved runs of
/// whitespace and @c ; line comments, to a fixpoint.
///
/// This is the CL-flavored counterpart of
/// @c smdscheme::parser::skip_intertoken_space, extended to also skip
/// comments, since Scheme's whitespace-only reader has no comment syntax at
/// all in this project's subset.
///
/// Named @c skip_cl_intertoken_space rather than reusing the Scheme name:
/// both take a @c smdscheme::parser::cursor, so an identically named
/// overload would be pulled into every unqualified call site by
/// argument-dependent lookup on @c cursor's namespace, making the two
/// permanently ambiguous wherever both are reachable.
constexpr auto skip_cl_intertoken_space(smdscheme::parser::cursor cur)
    -> smdscheme::parser::cursor {
    while (!cur.empty()) {
        if (is_whitespace(cur.peek())) {
            cur = cur.bump();
            continue;
        }
        if (cur.peek() == ';') {
            cur = skip_line_comment(cur);
            continue;
        }
        break;
    }
    return cur;
}

} // namespace smd::smdlisp::reader

#endif
