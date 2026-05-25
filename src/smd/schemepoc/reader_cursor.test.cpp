// src/smd/schemepoc/reader_cursor.test.cpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/reader_cursor.hpp>
#include <smd/schemepoc/reader_cursor.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::schemepoc::cursor;
using smd::schemepoc::is_delimiter;
using smd::schemepoc::is_initial_symbol_char;
using smd::schemepoc::is_space;
using smd::schemepoc::is_symbol_char;
using smd::schemepoc::skip_intertoken_space;

static_assert(is_delimiter('('));
static_assert(is_delimiter(')'));
static_assert(is_delimiter('\''));
static_assert(is_delimiter(' '));
static_assert(!is_delimiter('a'));

static_assert([] {
    cursor c{"  x"};
    return skip_intertoken_space(c).peek() == 'x';
}());

TEST_CASE("ReaderCursorTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ReaderCursorTest - EmptyInput") {
    constexpr cursor c{""};
    STATIC_REQUIRE(c.empty());
}

TEST_CASE("ReaderCursorTest - PeekAtFirst") {
    constexpr cursor c{"abc"};
    STATIC_REQUIRE(c.peek() == 'a');
}

TEST_CASE("ReaderCursorTest - BumpAdvancesOffset") {
    constexpr cursor c{"ab"};
    constexpr cursor c2 = c.bump();
    STATIC_REQUIRE(c2.position().offset == 1);
    STATIC_REQUIRE(c2.position().column == 2);
    STATIC_REQUIRE(c2.peek() == 'b');
}

TEST_CASE("ReaderCursorTest - BumpOnNewlineUpdatesLineAndResetsColumn") {
    constexpr cursor c{"a\nb"};
    constexpr cursor after_a = c.bump();
    constexpr cursor after_nl = after_a.bump();
    STATIC_REQUIRE(after_nl.position().line == 2);
    STATIC_REQUIRE(after_nl.position().column == 1);
    STATIC_REQUIRE(after_nl.peek() == 'b');
}

TEST_CASE("ReaderCursorTest - PositionStartsAtLine1Col1") {
    constexpr cursor c{"x"};
    STATIC_REQUIRE(c.position().line == 1);
    STATIC_REQUIRE(c.position().column == 1);
    STATIC_REQUIRE(c.position().offset == 0);
}

TEST_CASE("ReaderCursorTest - RemainingReturnsUnconsumedInput") {
    constexpr cursor c{"hello"};
    constexpr cursor c2 = c.bump();
    STATIC_REQUIRE(c2.remaining() == "ello");
}

TEST_CASE("ReaderCursorTest - IsSpace") {
    STATIC_REQUIRE(is_space(' '));
    STATIC_REQUIRE(is_space('\t'));
    STATIC_REQUIRE(is_space('\n'));
    STATIC_REQUIRE(is_space('\r'));
    STATIC_REQUIRE(!is_space('a'));
    STATIC_REQUIRE(!is_space('('));
}

TEST_CASE("ReaderCursorTest - IsInitialSymbolChar") {
    STATIC_REQUIRE(is_initial_symbol_char('a'));
    STATIC_REQUIRE(is_initial_symbol_char('Z'));
    STATIC_REQUIRE(is_initial_symbol_char('+'));
    STATIC_REQUIRE(is_initial_symbol_char('-'));
    STATIC_REQUIRE(is_initial_symbol_char('*'));
    STATIC_REQUIRE(is_initial_symbol_char('/'));
    STATIC_REQUIRE(is_initial_symbol_char('='));
    STATIC_REQUIRE(is_initial_symbol_char('<'));
    STATIC_REQUIRE(is_initial_symbol_char('>'));
    STATIC_REQUIRE(is_initial_symbol_char('!'));
    STATIC_REQUIRE(is_initial_symbol_char('?'));
    STATIC_REQUIRE(!is_initial_symbol_char('0'));
    STATIC_REQUIRE(!is_initial_symbol_char('('));
}

TEST_CASE("ReaderCursorTest - IsSymbolChar") {
    STATIC_REQUIRE(is_symbol_char('a'));
    STATIC_REQUIRE(is_symbol_char('0'));
    STATIC_REQUIRE(is_symbol_char('9'));
    STATIC_REQUIRE(is_symbol_char('+'));
    STATIC_REQUIRE(!is_symbol_char('('));
    STATIC_REQUIRE(!is_symbol_char(' '));
}

TEST_CASE("ReaderCursorTest - SkipInterTokenSpaceStopsAtNonSpace") {
    constexpr cursor c{"   foo"};
    constexpr cursor result = skip_intertoken_space(c);
    STATIC_REQUIRE(result.peek() == 'f');
    STATIC_REQUIRE(result.position().offset == 3);
}

TEST_CASE("ReaderCursorTest - SkipInterTokenSpaceOnEmptyInput") {
    constexpr cursor c{""};
    constexpr cursor result = skip_intertoken_space(c);
    STATIC_REQUIRE(result.empty());
}
