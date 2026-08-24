// src/smd/kit/parser/cursor.test.cpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Moved here in step B2 from src/smd/cl/reader/cursor.test.cpp, which pins
// the same behaviour through the forwarding shim and does not change.

#include <smd/kit/parser/cursor.hpp>
#include <smd/kit/parser/cursor.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::kit::parser::advance_while;
using smd::kit::parser::cursor;
using smd::kit::parser::parse_state;

namespace {

constexpr auto starts_at_line_one_column_one() -> bool {
    cursor const cur{"ab"};
    return !cur.empty() && cur.peek() == 'a' && cur.position().offset == 0 &&
           cur.position().line == 1 && cur.position().column == 1 &&
           cur.remaining() == "ab";
}

constexpr auto bump_advances_column() -> bool {
    cursor const cur = cursor{"ab"}.bump();
    return cur.peek() == 'b' && cur.position().offset == 1 &&
           cur.position().line == 1 && cur.position().column == 2;
}

constexpr auto bump_over_newline_advances_line() -> bool {
    cursor const cur = cursor{"a\nb"}.bump().bump();
    return cur.peek() == 'b' && cur.position().offset == 2 &&
           cur.position().line == 2 && cur.position().column == 1;
}

constexpr auto bump_to_exhaustion() -> bool {
    cursor const cur = cursor{"a"}.bump();
    return cur.empty() && cur.remaining().empty();
}

constexpr auto advance_while_stops_on_predicate() -> bool {
    cursor const cur =
        advance_while(cursor{"aaab"}, [](char c) { return c == 'a'; });
    return cur.peek() == 'b' && cur.position().offset == 3;
}

constexpr auto advance_while_stops_at_end() -> bool {
    return advance_while(cursor{"aaa"}, [](char) { return true; }).empty();
}

constexpr auto parse_state_is_comparable() -> bool {
    cursor const cur{"x"};
    return parse_state<int>{1, cur} == parse_state<int>{1, cur} &&
           !(parse_state<int>{1, cur} == parse_state<int>{2, cur});
}

} // namespace

static_assert(starts_at_line_one_column_one());
static_assert(bump_advances_column());
static_assert(bump_over_newline_advances_line());
static_assert(bump_to_exhaustion());
static_assert(advance_while_stops_on_predicate());
static_assert(advance_while_stops_at_end());
static_assert(parse_state_is_comparable());

TEST_CASE("CursorTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("CursorTest - PositionTracking") {
    CHECK(starts_at_line_one_column_one());
    CHECK(bump_advances_column());
    CHECK(bump_over_newline_advances_line());
    CHECK(bump_to_exhaustion());
}

TEST_CASE("CursorTest - AdvanceWhile") {
    CHECK(advance_while_stops_on_predicate());
    CHECK(advance_while_stops_at_end());
}

TEST_CASE("CursorTest - CursorsAreValues") {
    // A saved cursor is unaffected by advancing a copy.
    cursor const saved{"abc"};
    cursor const advanced = saved.bump();
    CHECK(saved.peek() == 'a');
    CHECK(advanced.peek() == 'b');
    CHECK(!(saved == advanced));
    CHECK(parse_state_is_comparable());
}
