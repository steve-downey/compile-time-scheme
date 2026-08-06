// src/smd/cl/reader/readtable.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_READER_READTABLE_HPP
#define SRC_SMD_CL_READER_READTABLE_HPP

#include <array>

namespace smd::cl::reader {

/// The syntax type of a character, per ANSI CL 2.1.4. The reader consults
/// the readtable for every character decision, so this classification —
/// not hardcoded predicates — is what drives tokenization.
enum class syntax_type : unsigned char {
    constituent,           ///< May appear in a symbol or number token.
    whitespace,            ///< Separates tokens; consumed and discarded.
    terminating_macro,     ///< Ends a token and triggers a reader macro.
    non_terminating_macro, ///< Triggers a reader macro at token start only;
                           ///< an ordinary constituent mid-token (`#`).
    single_escape,         ///< Takes the next character literally (`\`).
    multiple_escape        ///< Toggles literal mode to the matching `|`.
};

/// Which reader-macro handler a macro character dispatches to.
///
/// Decision D19 asks that the reader be readtable-shaped from the start:
/// every dispatch decision reads this table, so `set-macro-character` is a
/// later widening of this enum with a "user function" alternative — an
/// exposure of structure that already exists — rather than a rewrite of
/// hardcoded character tests.
enum class macro_kind : unsigned char {
    none,         ///< Not a macro character.
    left_paren,   ///< `(` — reads a list.
    right_paren,  ///< `)` — always an error at datum position.
    single_quote, ///< `'` — quote shorthand.
    semicolon,    ///< `;` — line comment (consumed as intertoken space).
    double_quote, ///< `"` — string literal.
    backquote,    ///< `` ` `` — backquote template.
    comma,        ///< `,` — unquote / unquote-splicing.
    sharpsign     ///< `#` — dispatching macro; see @ref sharpsign_kind.
};

/// The sharpsign sub-dispatch: which handler the character after `#` (and
/// after any infix numeric argument, as in `#16r`) selects.
enum class sharpsign_kind : unsigned char {
    none,              ///< No handler; `#<char>` is an error.
    function_quote,    ///< `#'` — function shorthand.
    vector_open,       ///< `#(` — vector.
    character_literal, ///< `#\` — character.
    radix_binary,      ///< `#b` — rational in radix 2.
    radix_octal,       ///< `#o` — rational in radix 8.
    radix_hex,         ///< `#x` — rational in radix 16.
    radix_n,           ///< `#nnR` — rational in radix nn.
    block_comment      ///< `#|` — nestable block comment (intertoken space).
};

/// The character-dispatch table the reader runs on (decision D19): per
/// character, its @ref syntax_type, its @ref macro_kind, and its
/// @ref sharpsign_kind for sharpsign sub-dispatch.
///
/// A default-constructed readtable makes every character an ordinary
/// constituent with no macros; @ref standard_readtable carries the ANSI
/// standard syntax. Characters outside the 7-bit table are constituents.
class readtable {
  public:
    constexpr readtable() = default;

    /// Returns the syntax type of @p c.
    [[nodiscard]] constexpr auto syntax_of(char c) const -> syntax_type;

    /// Returns the macro handler for @p c.
    [[nodiscard]] constexpr auto macro_of(char c) const -> macro_kind;

    /// Returns the sharpsign sub-handler for @p c.
    [[nodiscard]] constexpr auto sharp_of(char c) const -> sharpsign_kind;

    /// Returns true if @p c is whitespace under this table.
    [[nodiscard]] constexpr auto is_whitespace(char c) const -> bool;

    /// Returns true if @p c may continue a token under this table: a
    /// constituent, or a non-terminating macro character (which is macro
    /// only at token start, ANSI 2.2).
    [[nodiscard]] constexpr auto is_token_constituent(char c) const -> bool;

    /// Sets the syntax type of @p c.
    constexpr auto set_syntax(char c, syntax_type s) -> void;

    /// Makes @p c a macro character with syntax @p s and handler @p k.
    constexpr auto set_macro(char c, syntax_type s, macro_kind k) -> void;

    /// Sets the sharpsign sub-handler for @p c.
    constexpr auto set_sharp(char c, sharpsign_kind k) -> void;

  private:
    static constexpr int table_size = 128;

    [[nodiscard]] static constexpr auto index_of(char c) -> int;

    std::array<syntax_type, table_size> syntax_{};
    std::array<macro_kind, table_size> macro_{};
    std::array<sharpsign_kind, table_size> sharp_{};
};

constexpr auto readtable::index_of(char c) -> int {
    auto const u = static_cast<unsigned char>(c);
    return u < table_size ? static_cast<int>(u) : -1;
}

constexpr auto readtable::syntax_of(char c) const -> syntax_type {
    int const i = index_of(c);
    return i < 0 ? syntax_type::constituent : syntax_[static_cast<unsigned>(i)];
}

constexpr auto readtable::macro_of(char c) const -> macro_kind {
    int const i = index_of(c);
    return i < 0 ? macro_kind::none : macro_[static_cast<unsigned>(i)];
}

constexpr auto readtable::sharp_of(char c) const -> sharpsign_kind {
    int const i = index_of(c);
    return i < 0 ? sharpsign_kind::none : sharp_[static_cast<unsigned>(i)];
}

constexpr auto readtable::is_whitespace(char c) const -> bool {
    return syntax_of(c) == syntax_type::whitespace;
}

constexpr auto readtable::is_token_constituent(char c) const -> bool {
    auto const s = syntax_of(c);
    return s == syntax_type::constituent ||
           s == syntax_type::non_terminating_macro;
}

constexpr auto readtable::set_syntax(char c, syntax_type s) -> void {
    int const i = index_of(c);
    if (i >= 0) {
        syntax_[static_cast<unsigned>(i)] = s;
    }
}

constexpr auto readtable::set_macro(char c, syntax_type s, macro_kind k)
    -> void {
    set_syntax(c, s);
    int const i = index_of(c);
    if (i >= 0) {
        macro_[static_cast<unsigned>(i)] = k;
    }
}

constexpr auto readtable::set_sharp(char c, sharpsign_kind k) -> void {
    int const i = index_of(c);
    if (i >= 0) {
        sharp_[static_cast<unsigned>(i)] = k;
    }
}

/// Builds the ANSI standard readtable: standard whitespace, the standard
/// terminating macro characters `( ) ' ; " ` ,`, the non-terminating
/// dispatching `#` with its sub-dispatch, and the escapes `\` and `|`.
///
/// consteval: this is table generation, meaningless at runtime.
[[nodiscard]] consteval auto make_standard_readtable() -> readtable {
    readtable rt;
    rt.set_syntax(' ', syntax_type::whitespace);
    rt.set_syntax('\t', syntax_type::whitespace);
    rt.set_syntax('\n', syntax_type::whitespace);
    rt.set_syntax('\r', syntax_type::whitespace);
    rt.set_syntax('\f', syntax_type::whitespace);
    rt.set_macro('(', syntax_type::terminating_macro, macro_kind::left_paren);
    rt.set_macro(')', syntax_type::terminating_macro, macro_kind::right_paren);
    rt.set_macro('\'', syntax_type::terminating_macro,
                 macro_kind::single_quote);
    rt.set_macro(';', syntax_type::terminating_macro, macro_kind::semicolon);
    rt.set_macro('"', syntax_type::terminating_macro, macro_kind::double_quote);
    rt.set_macro('`', syntax_type::terminating_macro, macro_kind::backquote);
    rt.set_macro(',', syntax_type::terminating_macro, macro_kind::comma);
    rt.set_macro('#', syntax_type::non_terminating_macro,
                 macro_kind::sharpsign);
    rt.set_syntax('\\', syntax_type::single_escape);
    rt.set_syntax('|', syntax_type::multiple_escape);
    rt.set_sharp('\'', sharpsign_kind::function_quote);
    rt.set_sharp('(', sharpsign_kind::vector_open);
    rt.set_sharp('\\', sharpsign_kind::character_literal);
    rt.set_sharp('b', sharpsign_kind::radix_binary);
    rt.set_sharp('B', sharpsign_kind::radix_binary);
    rt.set_sharp('o', sharpsign_kind::radix_octal);
    rt.set_sharp('O', sharpsign_kind::radix_octal);
    rt.set_sharp('x', sharpsign_kind::radix_hex);
    rt.set_sharp('X', sharpsign_kind::radix_hex);
    rt.set_sharp('r', sharpsign_kind::radix_n);
    rt.set_sharp('R', sharpsign_kind::radix_n);
    rt.set_sharp('|', sharpsign_kind::block_comment);
    return rt;
}

/// The ANSI standard readtable, the default for every read entry point.
inline constexpr readtable standard_readtable = make_standard_readtable();

} // namespace smd::cl::reader

#endif
