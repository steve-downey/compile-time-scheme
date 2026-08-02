// src/smd/cl/reader/token.test.cpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/reader/token.hpp>
#include <smd/cl/reader/token.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <string_view>

using smd::cl::reader::cursor;
using smd::cl::reader::scan_token;
using smd::cl::reader::standard_readtable;
using smd::cl::reader::to_upper_char;

namespace {

constexpr auto scans_to(std::string_view text, std::string_view expected)
    -> bool {
    auto const r = scan_token(cursor{text}, standard_readtable);
    return r.has_value() && r.value().value.view() == expected;
}

constexpr auto folds_to_uppercase() -> bool {
    auto const r = scan_token(cursor{"foo"}, standard_readtable);
    return r.has_value() && r.value().value.view() == "FOO" &&
           !r.value().value.has_escape && r.value().rest.empty();
}

constexpr auto stops_at_terminating_macro() -> bool {
    auto const r = scan_token(cursor{"abc(def"}, standard_readtable);
    return r.has_value() && r.value().value.view() == "ABC" &&
           r.value().rest.peek() == '(';
}

constexpr auto stops_at_whitespace() -> bool {
    auto const r = scan_token(cursor{"abc def"}, standard_readtable);
    return r.has_value() && r.value().value.view() == "ABC" &&
           r.value().rest.peek() == ' ';
}

constexpr auto sharpsign_continues_a_token() -> bool {
    // `#` is a non-terminating macro character: constituent mid-token.
    return scans_to("a#b", "A#B");
}

constexpr auto single_escape_preserves_case() -> bool {
    auto const r = scan_token(cursor{"\\foo"}, standard_readtable);
    return r.has_value() && r.value().value.view() == "fOO" &&
           r.value().value.has_escape;
}

constexpr auto single_escape_takes_macro_chars() -> bool {
    return scans_to("a\\(b", "A(B");
}

constexpr auto multiple_escape_preserves_a_run() -> bool {
    auto const r = scan_token(cursor{"|Foo Bar|"}, standard_readtable);
    return r.has_value() && r.value().value.view() == "Foo Bar" &&
           r.value().value.has_escape && r.value().rest.empty();
}

constexpr auto multiple_escape_can_be_empty() -> bool {
    // `||` is the symbol with the empty name.
    auto const r = scan_token(cursor{"||"}, standard_readtable);
    return r.has_value() && r.value().value.view().empty() &&
           r.value().value.has_escape;
}

constexpr auto single_escape_inside_multiple_escape() -> bool {
    return scans_to("|a\\|b|", "a|b");
}

constexpr auto empty_input_is_an_error() -> bool {
    auto const r = scan_token(cursor{""}, standard_readtable);
    return !r.has_value() &&
           std::string_view{r.error().message} == "expected token";
}

constexpr auto delimiter_first_is_an_error() -> bool {
    return !scan_token(cursor{"(a)"}, standard_readtable).has_value();
}

constexpr auto dangling_single_escape_is_an_error() -> bool {
    auto const r = scan_token(cursor{"ab\\"}, standard_readtable);
    return !r.has_value() && std::string_view{r.error().message} ==
                                 "end of input after escape character";
}

constexpr auto unterminated_multiple_escape_is_an_error() -> bool {
    auto const r = scan_token(cursor{"|abc"}, standard_readtable);
    return !r.has_value() &&
           std::string_view{r.error().message} == "unterminated |";
}

constexpr auto overlong_token_is_an_error() -> bool {
    // The old tree silently split a 65-character token into two (a
    // defect, never pinned); this reader reports it instead.
    constexpr std::string_view sixty_five_a =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    static_assert(sixty_five_a.size() == 65);
    auto const r = scan_token(cursor{sixty_five_a}, standard_readtable);
    return !r.has_value() &&
           std::string_view{r.error().message} == "token too long";
}

constexpr auto to_upper_char_folds_ascii_only() -> bool {
    return to_upper_char('a') == 'A' && to_upper_char('z') == 'Z' &&
           to_upper_char('A') == 'A' && to_upper_char('1') == '1' &&
           to_upper_char('+') == '+';
}

} // namespace

static_assert(folds_to_uppercase());
static_assert(stops_at_terminating_macro());
static_assert(stops_at_whitespace());
static_assert(sharpsign_continues_a_token());
static_assert(single_escape_preserves_case());
static_assert(single_escape_takes_macro_chars());
static_assert(multiple_escape_preserves_a_run());
static_assert(multiple_escape_can_be_empty());
static_assert(single_escape_inside_multiple_escape());
static_assert(empty_input_is_an_error());
static_assert(delimiter_first_is_an_error());
static_assert(dangling_single_escape_is_an_error());
static_assert(unterminated_multiple_escape_is_an_error());
static_assert(overlong_token_is_an_error());
static_assert(to_upper_char_folds_ascii_only());

TEST_CASE("TokenTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("TokenTest - FoldingAndBoundaries") {
    CHECK(folds_to_uppercase());
    CHECK(stops_at_terminating_macro());
    CHECK(stops_at_whitespace());
    CHECK(sharpsign_continues_a_token());
    CHECK(to_upper_char_folds_ascii_only());
}

TEST_CASE("TokenTest - Escapes") {
    CHECK(single_escape_preserves_case());
    CHECK(single_escape_takes_macro_chars());
    CHECK(multiple_escape_preserves_a_run());
    CHECK(multiple_escape_can_be_empty());
    CHECK(single_escape_inside_multiple_escape());
}

TEST_CASE("TokenTest - Errors") {
    CHECK(empty_input_is_an_error());
    CHECK(delimiter_first_is_an_error());
    CHECK(dangling_single_escape_is_an_error());
    CHECK(unterminated_multiple_escape_is_an_error());
    CHECK(overlong_token_is_an_error());
}

TEST_CASE("TokenTest - TokenTextOwnsItsCharacters") {
    // The token survives the source buffer: copy it, compare later.
    auto const r = scan_token(cursor{"copy-me"}, standard_readtable);
    REQUIRE(r.has_value());
    auto const token = r.value().value;
    CHECK(token.view() == "COPY-ME");
}
