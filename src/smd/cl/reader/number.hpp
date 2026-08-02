// src/smd/cl/reader/number.hpp                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_READER_NUMBER_HPP
#define SRC_SMD_CL_READER_NUMBER_HPP

#include <algorithm>
#include <ranges>
#include <string_view>

namespace smd::cl::reader {

/// What a whole token's spelling denotes, numerically.
///
/// Whole-token classification is the load-bearing move (DIV-0003,
/// accepted-permanent): a token is read to its end and only then
/// classified, so @c 1+ is the symbol it is in ANSI CL rather than the
/// integer @c 1 followed by junk.
enum class number_class : unsigned char {
    none,    ///< Not a number; the token is a symbol.
    fixnum,  ///< An integer that fits the fixnum representation (int).
    bignum,  ///< Integer syntax too large for a fixnum; carried as its
             ///< spelling per decision D19.
    ratio,   ///< Ratio syntax (`1/2`); carried as its spelling per D19.
    floating ///< Float syntax (`1.5`, `.5e3`, `1d0`); carried as its
             ///< spelling per D19. Decimal radix only (ANSI 2.3.1).
};

/// The result of classifying one token: its @ref number_class, and for
/// @c fixnum the parsed value.
struct classified_number {
    number_class cls = number_class::none; ///< What the token denotes.
    int value = 0;                         ///< The value, fixnum only.

    // HIDDEN FRIEND
    friend constexpr auto operator==(classified_number const &,
                                     classified_number const &)
        -> bool = default;
};

/// Returns the digit value of @p c — `0`-`9` then `A`-`Z` for 10–35 —
/// or -1 if @p c is no digit in any radix. Lowercase is not handled
/// here: token scanning already folded unescaped characters to uppercase.
[[nodiscard]] constexpr auto digit_value(char c) -> int {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 10;
    }
    return -1;
}

namespace detail {

/// Locates @p target in @p text; @c std::string_view::npos if absent.
///
/// This is @c text.find(target) spelled as a predicate search, and the
/// spelling is load-bearing: GCC trunk r16-8246 misfolds memchr-lowered
/// searches (member @c find, @c ranges::find of a char) on a view offset
/// into a string literal — after @c remove_prefix(1), @c find reports the
/// offset in the original literal. A predicate @c find_if is not lowered
/// to memchr and folds correctly. Five-line reproduction in the R4 step
/// brief; flagged there for a toolchain divergence record.
[[nodiscard]] constexpr auto find_char(std::string_view text, char target)
    -> std::size_t {
    auto const found =
        std::ranges::find_if(text, [target](char c) { return c == target; });
    return found == text.end() ? std::string_view::npos
                               : static_cast<std::size_t>(found - text.begin());
}

/// Locates the first ANSI float exponent marker in @p text (the token
/// scanner already folded letters to uppercase); @c npos if absent.
/// A predicate search for the same reason as @ref find_char.
[[nodiscard]] constexpr auto find_exponent_marker(std::string_view text)
    -> std::size_t {
    auto const found = std::ranges::find_if(text, [](char c) {
        return c == 'E' || c == 'S' || c == 'F' || c == 'D' || c == 'L';
    });
    return found == text.end() ? std::string_view::npos
                               : static_cast<std::size_t>(found - text.begin());
}

/// Returns true if @p text is one or more digits of @p radix.
[[nodiscard]] constexpr auto all_digits(std::string_view text, int radix)
    -> bool {
    return !text.empty() && std::ranges::all_of(text, [radix](char c) {
        int const d = digit_value(c);
        return d >= 0 && d < radix;
    });
}

/// Accumulates the digits of @p text (all pre-validated for @p radix)
/// into a fixnum with sign @p negative, or reports @c bignum when the
/// magnitude exceeds the fixnum range.
[[nodiscard]] constexpr auto accumulate_integer(std::string_view text,
                                                int radix, bool negative)
    -> classified_number {
    constexpr long long fixnum_limit = 2147483648LL; // |INT_MIN|
    // Horner's rule is a left fold. The accumulator saturates rather than
    // carrying a separate overflow flag: once it is past the fixnum limit
    // the answer is `bignum` whatever the remaining digits say, and
    // freezing it there is also what keeps the multiply from overflowing.
    long long const magnitude =
        std::ranges::fold_left(text, 0LL, [radix](long long acc, char c) {
            return acc > fixnum_limit ? acc : acc * radix + digit_value(c);
        });
    if (magnitude > fixnum_limit || (magnitude == fixnum_limit && !negative)) {
        return classified_number{number_class::bignum, 0};
    }
    return classified_number{
        number_class::fixnum,
        static_cast<int>(negative ? -magnitude : magnitude)};
}

/// Returns true if @p text spells an ANSI float (radix 10 only):
/// `digit* . digit+ exponent?` or `digit+ [. digit*] exponent`, where
/// `exponent` is one of `E S F D L` then an optional sign then digits.
/// The whole-token scanner already folded letters to uppercase.
[[nodiscard]] constexpr auto is_float_spelling(std::string_view text) -> bool {
    auto const marker = find_exponent_marker(text);
    std::string_view const mantissa = text.substr(0, marker);
    if (marker != std::string_view::npos) {
        std::string_view expo = text.substr(marker + 1);
        if (!expo.empty() && (expo.front() == '+' || expo.front() == '-')) {
            expo.remove_prefix(1);
        }
        if (!all_digits(expo, 10)) {
            return false;
        }
    }
    auto const dot = find_char(mantissa, '.');
    if (dot == std::string_view::npos) {
        // No decimal point: an exponent and a nonempty digit string are
        // both required (`5E3`); otherwise this is integer syntax.
        return marker != std::string_view::npos && all_digits(mantissa, 10);
    }
    std::string_view const int_part = mantissa.substr(0, dot);
    std::string_view const frac_part = mantissa.substr(dot + 1);
    if (!int_part.empty() && !all_digits(int_part, 10)) {
        return false;
    }
    if (!frac_part.empty() && !all_digits(frac_part, 10)) {
        return false;
    }
    if (!frac_part.empty()) {
        // `.5`, `1.5`, `1.5E3` — a fractional part suffices.
        return true;
    }
    // `5.` is a decimal integer, but `5.E3` is a float: an empty
    // fraction needs an exponent and a nonempty integer part.
    return marker != std::string_view::npos && !int_part.empty();
}

} // namespace detail

/// Classifies the whole token @p text against the numeric grammar of
/// @p radix (ANSI 2.3.1): integer (including the trailing-dot decimal
/// spelling `10.`), ratio, or — in radix 10 only — float. Anything else,
/// including an empty token, is @c none: the token is a symbol.
///
/// @pre 2 <= radix <= 36
[[nodiscard]] constexpr auto classify_number(std::string_view text, int radix)
    -> classified_number {
    std::string_view rest = text;
    bool negative = false;
    if (!rest.empty() && (rest.front() == '+' || rest.front() == '-')) {
        negative = rest.front() == '-';
        rest.remove_prefix(1);
    }
    if (rest.empty()) {
        return classified_number{};
    }
    if (auto const slash = detail::find_char(rest, '/');
        slash != std::string_view::npos) {
        if (detail::all_digits(rest.substr(0, slash), radix) &&
            detail::all_digits(rest.substr(slash + 1), radix)) {
            return classified_number{number_class::ratio, 0};
        }
        return classified_number{};
    }
    if (detail::all_digits(rest, radix)) {
        return detail::accumulate_integer(rest, radix, negative);
    }
    if (radix == 10) {
        if (rest.size() > 1 && rest.back() == '.' &&
            detail::all_digits(rest.substr(0, rest.size() - 1), 10)) {
            // `10.` — a decimal integer with the trailing decimal point.
            return detail::accumulate_integer(rest.substr(0, rest.size() - 1),
                                              10, negative);
        }
        if (detail::is_float_spelling(rest)) {
            return classified_number{number_class::floating, 0};
        }
    }
    return classified_number{};
}

} // namespace smd::cl::reader

#endif
