// src/smd/schemepoc/parser.test.cpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/parser.hpp>
#include <smd/schemepoc/parser.hpp>  // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::schemepoc::char_p;
using smd::schemepoc::cursor;
using smd::schemepoc::pure;
using smd::schemepoc::satisfy;

static_assert(char_p('x')(cursor{"xyz"}).has_value());
static_assert(char_p('x')(cursor{"xyz"}).value().value == 'x');
static_assert(char_p('x')(cursor{"xyz"}).value().rest.peek() == 'y');

static_assert(pure(42)(cursor{"abc"}).has_value());
static_assert(pure(42)(cursor{"abc"}).value().value == 42);
static_assert(pure(42)(cursor{"abc"}).value().rest.remaining() == "abc");

static_assert(!char_p('x')(cursor{"abc"}).has_value());
static_assert(!char_p('x')(cursor{""}).has_value());

static_assert(!satisfy([](char c) { return c == 'z'; }, "z")(cursor{""}).has_value());

TEST_CASE("ParserTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ParserTest - CharPMatchesExpected") {
    constexpr cursor c{"xyz"};
    constexpr auto   r = char_p('x')(c);
    STATIC_REQUIRE(r.has_value());
    STATIC_REQUIRE(r.value().value == 'x');
    STATIC_REQUIRE(r.value().rest.peek() == 'y');
}

TEST_CASE("ParserTest - CharPMismatch") {
    constexpr cursor c{"abc"};
    constexpr auto   r = char_p('x')(c);
    STATIC_REQUIRE(!r.has_value());
}

TEST_CASE("ParserTest - CharPEmptyCursor") {
    constexpr cursor c{""};
    constexpr auto   r = char_p('x')(c);
    STATIC_REQUIRE(!r.has_value());
}

TEST_CASE("ParserTest - PureSucceedsWithUnchangedCursor") {
    constexpr cursor c{"hello"};
    constexpr auto   r = pure(42)(c);
    STATIC_REQUIRE(r.has_value());
    STATIC_REQUIRE(r.value().value == 42);
    STATIC_REQUIRE(r.value().rest.remaining() == "hello");
}

TEST_CASE("ParserTest - SatisfyOnEmptyReturnsError") {
    constexpr cursor c{""};
    constexpr auto   r = satisfy([](char ch) { return ch == 'a'; }, "a")(c);
    STATIC_REQUIRE(!r.has_value());
}
