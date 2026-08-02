// src/smd/cl/reader/token.hpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_READER_TOKEN_HPP
#define SRC_SMD_CL_READER_TOKEN_HPP

#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/reader/cursor.hpp>
#include <smd/cl/reader/readtable.hpp>

#include <array>
#include <cstddef>
#include <string_view>

namespace smd::cl::reader {

/// Maximum accepted length, in characters, of one token spelling.
inline constexpr int max_token_chars = 64;

/// One scanned token: its accumulated characters (unescaped characters
/// case-folded to uppercase, escaped characters verbatim) in the token's
/// own storage, and whether any character was escaped.
///
/// Own fixed storage, not a view into source: folding rewrites characters
/// and escape processing removes them, so the accumulated spelling cannot
/// alias the input. Ordinary value type, safe to copy and embed.
struct token_text {
    std::array<char, max_token_chars> storage{}; ///< Accumulated characters.
    int length = 0;          ///< Number of valid characters in storage.
    bool has_escape = false; ///< True if any character was escaped; an
                             ///< escaped token is always a symbol, never
                             ///< a number (ANSI 2.3.4).

    /// Returns the spelling as a view into this object's own storage,
    /// valid while this @c token_text (or the copy it was taken from)
    /// is alive.
    [[nodiscard]] constexpr auto view() const -> std::string_view {
        return std::string_view(storage.data(),
                                static_cast<std::size_t>(length));
    }

    // HIDDEN FRIEND
    friend constexpr auto operator==(token_text const &, token_text const &)
        -> bool = default;
};

/// Case-folds a single character to uppercase, ASCII only — the ANSI
/// default readtable's @c :upcase read case, applied to unescaped token
/// characters only.
[[nodiscard]] constexpr auto to_upper_char(char c) -> char {
    if (c >= 'a' && c <= 'z') {
        return static_cast<char>(c - 'a' + 'A');
    }
    return c;
}

/// Scans one token from @p cur under @p table: a maximal run of
/// constituent characters (non-terminating macro characters count as
/// constituents mid-token, ANSI 2.2), honouring single escape (`\x`
/// takes @c x verbatim) and multiple escape (`|...|` takes a run
/// verbatim). Unescaped characters are folded to uppercase.
///
/// Errors: an empty token ("expected token"), input ending after `\`
/// ("end of input after escape character"), an unterminated `|...|`
/// ("unterminated |"), and a token longer than @ref max_token_chars
/// ("token too long" — the old tree silently split an over-long token
/// instead, a defect this reader does not reproduce).
[[nodiscard]] constexpr auto scan_token(cursor cur, readtable const &table)
    -> foundation::result<parse_state<token_text>> {
    auto const start = cur.position();
    token_text token{};
    auto const append = [&token](char c) -> bool {
        if (token.length >= max_token_chars) {
            return false;
        }
        token.storage[static_cast<std::size_t>(token.length)] = c;
        ++token.length;
        return true;
    };
    // Substrate generic algorithm: the reader's token accumulator. The
    // escape states make each step depend on scanner state, so this is
    // the token layer's one primitive loop, built directly on cursor.
    bool in_multiple_escape = false;
    while (!cur.empty()) {
        char const c = cur.peek();
        auto const s = table.syntax_of(c);
        if (in_multiple_escape) {
            if (s == syntax_type::multiple_escape) {
                in_multiple_escape = false;
                cur = cur.bump();
                continue;
            }
            if (s == syntax_type::single_escape) {
                cur = cur.bump();
                if (cur.empty()) {
                    return foundation::parse_error{
                        cur.position(), "end of input after escape character"};
                }
                if (!append(cur.peek())) {
                    return foundation::parse_error{start, "token too long"};
                }
                cur = cur.bump();
                continue;
            }
            if (!append(c)) {
                return foundation::parse_error{start, "token too long"};
            }
            cur = cur.bump();
            continue;
        }
        if (s == syntax_type::single_escape) {
            token.has_escape = true;
            cur = cur.bump();
            if (cur.empty()) {
                return foundation::parse_error{
                    cur.position(), "end of input after escape character"};
            }
            if (!append(cur.peek())) {
                return foundation::parse_error{start, "token too long"};
            }
            cur = cur.bump();
            continue;
        }
        if (s == syntax_type::multiple_escape) {
            token.has_escape = true;
            in_multiple_escape = true;
            cur = cur.bump();
            continue;
        }
        if (!table.is_token_constituent(c)) {
            break;
        }
        if (!append(to_upper_char(c))) {
            return foundation::parse_error{start, "token too long"};
        }
        cur = cur.bump();
    }
    if (in_multiple_escape) {
        return foundation::parse_error{cur.position(), "unterminated |"};
    }
    if (token.length == 0 && !token.has_escape) {
        return foundation::parse_error{start, "expected token"};
    }
    return parse_state<token_text>{token, cur};
}

} // namespace smd::cl::reader

#endif
