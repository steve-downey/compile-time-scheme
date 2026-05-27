// src/smd/smdscheme/parser/parser.test.cpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/parser/parser.hpp>
#include <smd/smdscheme/parser/parser.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::smdscheme::parser::char_p;
using smd::smdscheme::parser::cursor;
using smd::smdscheme::parser::lift2;
using smd::smdscheme::parser::map;
using smd::smdscheme::parser::pure;
using smd::smdscheme::parser::satisfy;
using smd::smdscheme::parser::sequence_left;
using smd::smdscheme::parser::sequence_right;

static_assert(char_p('x')(cursor{"xyz"}).has_value());
static_assert(char_p('x')(cursor{"xyz"}).value().value == 'x');
static_assert(char_p('x')(cursor{"xyz"}).value().rest.peek() == 'y');

static_assert(pure(42)(cursor{"abc"}).has_value());
static_assert(pure(42)(cursor{"abc"}).value().value == 42);
static_assert(pure(42)(cursor{"abc"}).value().rest.remaining() == "abc");

static_assert(!char_p('x')(cursor{"abc"}).has_value());
static_assert(!char_p('x')(cursor{""}).has_value());

static_assert(
    !satisfy([](char c) { return c == 'z'; }, "z")(cursor{""}).has_value());

// map: success
static_assert(
    map(char_p('x'), [](char c) { return int(c); })(cursor{"xyz"}).has_value());
static_assert(map(char_p('x'), [](char c) { return int(c); })(cursor{"xyz"})
                  .value()
                  .value == int('x'));

// map: failure propagates
static_assert(!map(char_p('x'), [](char c) { return int(c); })(cursor{"abc"})
                   .has_value());

// lift2: both succeed
static_assert(lift2(char_p('a'), char_p('b'),
                    [](char a, char b) {
                        return a == 'a' && b == 'b';
                    })(cursor{"ab"})
                  .value()
                  .value == true);

// sequence_left: cursor is past both chars
static_assert(
    sequence_left(char_p('a'), char_p('b'))(cursor{"ab"}).value().value == 'a');
static_assert(sequence_left(char_p('a'), char_p('b'))(cursor{"ab"})
                  .value()
                  .rest.remaining() == "");

// sequence_right: keeps the right value
static_assert(
    sequence_right(char_p('a'), char_p('b'))(cursor{"ab"}).value().value ==
    'b');

// operator|: second branch taken when first fails without consuming
static_assert((char_p('a') | char_p('b'))(cursor{"b"}).has_value());
static_assert((char_p('a') | char_p('b'))(cursor{"b"}).value().value == 'b');

// operator|: both fail
static_assert(!(char_p('a') | char_p('b'))(cursor{"c"}).has_value());

// operator|: first branch succeeds
static_assert((char_p('a') | char_p('b'))(cursor{"a"}).value().value == 'a');

TEST_CASE("ParserTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ParserTest - CharPMatchesExpected") {
    constexpr cursor c{"xyz"};
    constexpr auto r = char_p('x')(c);
    STATIC_REQUIRE(r.has_value());
    STATIC_REQUIRE(r.value().value == 'x');
    STATIC_REQUIRE(r.value().rest.peek() == 'y');
}

TEST_CASE("ParserTest - CharPMismatch") {
    constexpr cursor c{"abc"};
    constexpr auto r = char_p('x')(c);
    STATIC_REQUIRE(!r.has_value());
}

TEST_CASE("ParserTest - CharPEmptyCursor") {
    constexpr cursor c{""};
    constexpr auto r = char_p('x')(c);
    STATIC_REQUIRE(!r.has_value());
}

TEST_CASE("ParserTest - PureSucceedsWithUnchangedCursor") {
    constexpr cursor c{"hello"};
    constexpr auto r = pure(42)(c);
    STATIC_REQUIRE(r.has_value());
    STATIC_REQUIRE(r.value().value == 42);
    STATIC_REQUIRE(r.value().rest.remaining() == "hello");
}

TEST_CASE("ParserTest - SatisfyOnEmptyReturnsError") {
    constexpr cursor c{""};
    constexpr auto r = satisfy([](char ch) { return ch == 'a'; }, "a")(c);
    STATIC_REQUIRE(!r.has_value());
}

TEST_CASE("ParserTest - MapSuccess") {
    constexpr cursor c{"xyz"};
    constexpr auto r = map(char_p('x'), [](char ch) { return int(ch); })(c);
    STATIC_REQUIRE(r.has_value());
    STATIC_REQUIRE(r.value().value == int('x'));
}

TEST_CASE("ParserTest - MapFailurePropagates") {
    constexpr cursor c{"abc"};
    constexpr auto r = map(char_p('x'), [](char ch) { return int(ch); })(c);
    STATIC_REQUIRE(!r.has_value());
}

TEST_CASE("ParserTest - Lift2BothSucceed") {
    constexpr cursor c{"ab"};
    constexpr auto r = lift2(char_p('a'), char_p('b'), [](char a, char b) {
        return a == 'a' && b == 'b';
    })(c);
    STATIC_REQUIRE(r.has_value());
    STATIC_REQUIRE(r.value().value == true);
}

TEST_CASE("ParserTest - SequenceLeftKeepsLeft") {
    constexpr cursor c{"ab"};
    constexpr auto r = sequence_left(char_p('a'), char_p('b'))(c);
    STATIC_REQUIRE(r.has_value());
    STATIC_REQUIRE(r.value().value == 'a');
    STATIC_REQUIRE(r.value().rest.remaining() == "");
}

TEST_CASE("ParserTest - SequenceRightKeepsRight") {
    constexpr cursor c{"ab"};
    constexpr auto r = sequence_right(char_p('a'), char_p('b'))(c);
    STATIC_REQUIRE(r.has_value());
    STATIC_REQUIRE(r.value().value == 'b');
}

TEST_CASE("ParserTest - AlternationSecondSucceeds") {
    constexpr cursor c{"b"};
    constexpr auto r = (char_p('a') | char_p('b'))(c);
    STATIC_REQUIRE(r.has_value());
    STATIC_REQUIRE(r.value().value == 'b');
}

TEST_CASE("ParserTest - AlternationBothFail") {
    constexpr cursor c{"c"};
    constexpr auto r = (char_p('a') | char_p('b'))(c);
    STATIC_REQUIRE(!r.has_value());
}
