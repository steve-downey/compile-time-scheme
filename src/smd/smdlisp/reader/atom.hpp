// src/smd/smdlisp/reader/atom.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_READER_ATOM_HPP
#define SRC_SMD_SMDLISP_READER_ATOM_HPP

#include <smd/smdlisp/reader/cl_chars.hpp>
#include <smd/smdscheme/foundation/parse_error.hpp>
#include <smd/smdscheme/parser/alt.hpp>
#include <smd/smdscheme/parser/cursor.hpp>
#include <smd/smdscheme/parser/parser.hpp>

#include <array>
#include <cstddef>
#include <string_view>
#include <variant>

namespace smd::smdlisp::reader {

/// Maximum accepted length, in characters, of a folded symbol or keyword
/// spelling (a keyword's leading `:` does not count toward this length).
inline constexpr int max_atom_name_length = 64;

/// A Common Lisp integer literal, e.g. @c 42 or @c -7.
///
/// Same grammar as the Scheme reader's @c atom_integer
/// (@c smd::smdscheme::reader::atom_integer): an optional leading @c - sign
/// followed by one or more decimal digits, with the whole token required to
/// be exactly that shape (see @ref integer_p for why "whole token" matters
/// here in a way it does not for the Scheme reader).
struct atom_integer {
    int value; ///< The parsed integer value.
};

/// Case-folded storage for a symbol or keyword spelling.
///
/// This cannot be a @c std::string_view into the source: folding (decision
/// D2, docs/cl-pivot-plan.md) rewrites characters, so the folded spelling
/// needs storage of its own rather than a view into someone else's buffer.
/// Ordinary value type; safe to copy and to embed in an @ref atom by value.
struct folded_name {
    std::array<char, max_atom_name_length> storage{}; ///< Folded characters.
    int length = 0; ///< Number of valid characters in @ref storage.

    /// Returns the folded spelling as a view into this object's own
    /// storage. The view is valid only as long as this @c folded_name (or
    /// the copy it was taken from) is alive.
    [[nodiscard]] constexpr auto view() const -> std::string_view {
        return std::string_view(storage.data(),
                                static_cast<std::size_t>(length));
    }

    friend constexpr auto operator==(folded_name const &, folded_name const &)
        -> bool = default;
};

/// A Common Lisp symbol, folded to uppercase per decision D2 (e.g. @c foo
/// reads as @c FOO).
///
/// @c t and @c nil read as ordinary symbols here (@c T, @c NIL); their
/// distinguished meaning as the canonical true value and the empty
/// list/false value is assigned downstream, at the value/elaborator layer,
/// not by this reader.
struct atom_symbol {
    folded_name name; ///< The folded symbol spelling.
};

/// A Common Lisp keyword (@c :foo), folded to uppercase per decision D2.
///
/// The spelling is stored WITHOUT the leading colon. A keyword is a
/// distinct atom kind from @ref atom_symbol per decision D7, so
/// keyword-ness is a static, checkable property of the atom rather than a
/// naming convention layered on top of @c atom_symbol.
struct atom_keyword {
    folded_name name; ///< The folded keyword spelling, colon stripped.
};

/// A Common Lisp atom: an integer, a symbol, or a keyword.
using atom = std::variant<atom_integer, atom_symbol, atom_keyword>;

namespace detail {

/// Scans a maximal run of constituent characters (per @ref
/// is_constituent_char, up to @ref max_atom_name_length of them) and
/// returns it as a view into the source, without classifying or folding it.
///
/// This is the shared "read one token" primitive that @ref integer_p, @ref
/// symbol_p, and @ref keyword_p all build on. Unlike the Scheme reader
/// (@c smd::smdscheme::reader), where numbers and symbols can never be
/// confused because symbols may not start with a digit, Common Lisp symbols
/// may start with a digit (e.g. the conventional symbol @c 1+), so an
/// integer cannot be recognized by greedily consuming a leading run of
/// digits: doing so would misread @c 1+ as the integer @c 1 followed by a
/// stray @c +. Reading the whole token first and classifying it afterward
/// (see @ref integer_p) avoids that.
[[nodiscard]] constexpr auto atom_token_p() {
    return smdscheme::parser::parser{
        [](smdscheme::parser::cursor cur)
            -> smdscheme::parser::parse_result<std::string_view> {
            auto start = cur;
            auto chars = smdscheme::parser::some<max_atom_name_length>(
                smdscheme::parser::satisfy(is_constituent_char, "atom"))(cur);
            if (!chars.has_value()) {
                return smdscheme::parser::parse_result<std::string_view>{
                    chars.error()};
            }
            auto end_cur = chars.value().rest;
            int len = end_cur.position().offset - start.position().offset;
            auto text =
                start.remaining().substr(0, static_cast<std::size_t>(len));
            return smdscheme::parser::parse_result<std::string_view>{
                smdscheme::parser::parse_state<std::string_view>{text,
                                                                 end_cur}};
        }};
}

/// Folds @p text to uppercase (decision D2) into a @ref folded_name.
/// @pre text.size() <= max_atom_name_length
[[nodiscard]] constexpr auto fold(std::string_view text) -> folded_name {
    folded_name name{};
    for (std::size_t i = 0; i < text.size(); ++i) {
        name.storage[i] = to_upper_char(text[i]);
    }
    name.length = static_cast<int>(text.size());
    return name;
}

} // namespace detail

// -- integer_p ------------------------------------------------------------

// 53ab854d-6317-4432-a49d-1bf4712a29a1
/// Returns a parser for a Common Lisp integer literal.
///
/// Reads a whole token (@ref detail::atom_token_p) and accepts it only if
/// it is entirely an optional leading @c - sign followed by one or more
/// decimal digits and nothing else; otherwise the parser fails without
/// misreading a digit-led symbol's numeric prefix (see @ref
/// detail::atom_token_p for why the whole token must be checked).
[[nodiscard]] constexpr auto integer_p() {
    return smdscheme::parser::parser{
        [](smdscheme::parser::cursor cur)
            -> smdscheme::parser::parse_result<atom_integer> {
            auto tok = detail::atom_token_p()(cur);
            if (!tok.has_value()) {
                return smdscheme::parser::parse_result<atom_integer>{
                    tok.error()};
            }
            auto text = tok.value().value;
            bool negative = !text.empty() && text[0] == '-';
            std::size_t digits_start = negative ? 1 : 0;
            if (digits_start >= text.size()) {
                return smdscheme::foundation::parse_error{cur.position(),
                                                          "integer"};
            }
            int n = 0;
            for (std::size_t i = digits_start; i < text.size(); ++i) {
                if (text[i] < '0' || text[i] > '9') {
                    return smdscheme::foundation::parse_error{cur.position(),
                                                              "integer"};
                }
                n = n * 10 + (text[i] - '0');
            }
            return smdscheme::parser::parse_result<atom_integer>{
                smdscheme::parser::parse_state<atom_integer>{
                    atom_integer{negative ? -n : n}, tok.value().rest}};
        }};
}
// 53ab854d-6317-4432-a49d-1bf4712a29a1 end

// -- symbol_p ---------------------------------------------------------------

/// Returns a parser for a Common Lisp symbol.
///
/// Reads a whole token (@ref detail::atom_token_p) and folds it to
/// uppercase (decision D2). Unlike the Scheme reader, there is no distinct
/// "initial character" set: CL symbols may start with any constituent
/// character, including a digit (e.g. @c 1+ or @c 2buffer). This parser
/// does not check whether the token also happens to look like an integer;
/// callers that need to prefer @ref integer_p for digit-shaped tokens (as
/// @ref atom_p does) must try it first.
[[nodiscard]] constexpr auto symbol_p() {
    return smdscheme::parser::parser{
        [](smdscheme::parser::cursor cur)
            -> smdscheme::parser::parse_result<atom_symbol> {
            auto tok = detail::atom_token_p()(cur);
            if (!tok.has_value()) {
                return smdscheme::parser::parse_result<atom_symbol>{
                    tok.error()};
            }
            return smdscheme::parser::parse_result<atom_symbol>{
                smdscheme::parser::parse_state<atom_symbol>{
                    atom_symbol{detail::fold(tok.value().value)},
                    tok.value().rest}};
        }};
}

// -- keyword_p --------------------------------------------------------------

/// Returns a parser for a Common Lisp keyword (@c :foo).
///
/// Requires a leading @c : followed by a whole token (@ref
/// detail::atom_token_p, at least one constituent character), folded to
/// uppercase (decision D2). The stored @ref atom_keyword::name does not
/// include the leading colon. Fails if @c : is not followed by at least
/// one constituent character (e.g. a bare @c : is not a well-formed
/// keyword).
[[nodiscard]] constexpr auto keyword_p() {
    return smdscheme::parser::parser{
        [](smdscheme::parser::cursor cur)
            -> smdscheme::parser::parse_result<atom_keyword> {
            auto colon = smdscheme::parser::char_p(':')(cur);
            if (!colon.has_value()) {
                return smdscheme::parser::parse_result<atom_keyword>{
                    colon.error()};
            }
            auto tok = detail::atom_token_p()(colon.value().rest);
            if (!tok.has_value()) {
                return smdscheme::parser::parse_result<atom_keyword>{
                    tok.error()};
            }
            return smdscheme::parser::parse_result<atom_keyword>{
                smdscheme::parser::parse_state<atom_keyword>{
                    atom_keyword{detail::fold(tok.value().value)},
                    tok.value().rest}};
        }};
}

// -- atom_p / read_atom -----------------------------------------------------

/// Returns a parser for a Common Lisp atom: an integer, a keyword, or a
/// symbol, tried in that order.
///
/// Integer is tried first so all-digit (optionally signed) tokens are not
/// mistaken for symbols; keyword is tried next so a leading @c : is not
/// swallowed by @ref symbol_p (which otherwise treats @c : as an ordinary
/// constituent character, since it is not a terminating macro character).
/// Alternatives are tried unconditionally in sequence, mirroring
/// @c smd::smdscheme::reader::detail::read_datum_node's fallback style,
/// rather than composed with @c operator|, whose no-input-consumed
/// short-circuit does not apply cleanly here.
[[nodiscard]] constexpr auto atom_p() {
    return smdscheme::parser::parser{
        [](smdscheme::parser::cursor cur)
            -> smdscheme::parser::parse_result<atom> {
            auto int_r = integer_p()(cur);
            if (int_r.has_value()) {
                return smdscheme::parser::parse_result<atom>{
                    smdscheme::parser::parse_state<atom>{
                        atom{int_r.value().value}, int_r.value().rest}};
            }
            auto kw_r = keyword_p()(cur);
            if (kw_r.has_value()) {
                return smdscheme::parser::parse_result<atom>{
                    smdscheme::parser::parse_state<atom>{
                        atom{kw_r.value().value}, kw_r.value().rest}};
            }
            auto sym_r = symbol_p()(cur);
            if (sym_r.has_value()) {
                return smdscheme::parser::parse_result<atom>{
                    smdscheme::parser::parse_state<atom>{
                        atom{sym_r.value().value}, sym_r.value().rest}};
            }
            return smdscheme::foundation::parse_error{cur.position(),
                                                      "expected atom"};
        }};
}

/// Reads a single atom from @p text: an integer, a keyword, or a symbol.
///
/// Convenience entry point over @ref atom_p for callers that have a whole
/// spelling in hand rather than an in-flight @c cursor (the datum reader,
/// step L6, will use @c atom_p directly instead). For example,
/// `read_atom("foo")` yields a symbol named `FOO`, and `read_atom(":bar")`
/// yields a keyword named `BAR`.
[[nodiscard]] constexpr auto read_atom(std::string_view text)
    -> smdscheme::parser::parse_result<atom> {
    return atom_p()(smdscheme::parser::cursor{text});
}

} // namespace smd::smdlisp::reader

#endif
