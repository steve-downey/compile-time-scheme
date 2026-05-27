// src/smd/smdscheme/parser/parser_alternative.test.cpp         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/parser/parser_alternative.hpp>
#include <smd/smdscheme/parser/parser_alternative.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

namespace {

using namespace smd::smdscheme::parser;

// alt
static_assert(alt(char_p('a'), char_p('b'))(cursor{"a"}).value().value == 'a');
static_assert(alt(char_p('a'), char_p('b'))(cursor{"b"}).value().value == 'b');
static_assert(!alt(char_p('a'), char_p('b'))(cursor{"c"}).has_value());

// many
static_assert(many<4>(char_p('a'))(cursor{""}).value().value.size() == 0);
static_assert(many<4>(char_p('a'))(cursor{"aaa"}).value().value.size() == 3);

// some
static_assert(!some<4>(char_p('a'))(cursor{""}).has_value());
static_assert(some<4>(char_p('a'))(cursor{"aa"}).value().value.size() == 2);

// optional
static_assert(optional(char_p('a'))(cursor{"b"}).has_value());
static_assert(!optional(char_p('a'))(cursor{"b"}).value().value.has_value());
static_assert(optional(char_p('a'))(cursor{"a"}).value().value.has_value());

// lexeme
static_assert(lexeme(char_p('x'))(cursor{"  x  "}).value().value == 'x');

} // namespace

TEST_CASE("ParserAlternativeTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ParserAlternativeTest - alt first branch") {
    constexpr auto p = alt(char_p('a'), char_p('b'));
    REQUIRE(p(cursor{"a"}).value().value == 'a');
    REQUIRE(p(cursor{"b"}).value().value == 'b');
    REQUIRE(!p(cursor{"c"}).has_value());
}

TEST_CASE("ParserAlternativeTest - many") {
    constexpr auto p = many<4>(char_p('a'));
    REQUIRE(p(cursor{""}).value().value.size() == 0);
    REQUIRE(p(cursor{"aaa"}).value().value.size() == 3);
    REQUIRE(p(cursor{"aaaa"}).value().value.size() == 4);
    REQUIRE(p(cursor{"aaaab"}).value().value.size() == 4);
}

TEST_CASE("ParserAlternativeTest - some") {
    constexpr auto p = some<4>(char_p('a'));
    REQUIRE(!p(cursor{""}).has_value());
    REQUIRE(p(cursor{"aa"}).value().value.size() == 2);
}

TEST_CASE("ParserAlternativeTest - optional") {
    constexpr auto p = optional(char_p('a'));
    REQUIRE(p(cursor{"b"}).has_value());
    REQUIRE(!p(cursor{"b"}).value().value.has_value());
    REQUIRE(p(cursor{"a"}).value().value.has_value());
    REQUIRE(p(cursor{"a"}).value().value.value() == 'a');
}

TEST_CASE("ParserAlternativeTest - lexeme strips whitespace") {
    constexpr auto p = lexeme(char_p('x'));
    REQUIRE(p(cursor{"  x  "}).value().value == 'x');
    REQUIRE(p(cursor{"x"}).value().value == 'x');
    REQUIRE(!p(cursor{"  y"}).has_value());
}
