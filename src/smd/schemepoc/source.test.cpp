// src/smd/schemepoc/source.test.cpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/source.hpp>
#include <smd/schemepoc/source.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::schemepoc::parse_error;
using smd::schemepoc::source_pos;
using smd::schemepoc::source_span;

static_assert(source_pos{} == source_pos{0, 1, 1});
static_assert(source_pos{5, 2, 3} == source_pos{5, 2, 3});
static_assert(!(source_pos{0, 1, 1} == source_pos{1, 1, 2}));

static_assert(source_span{} == source_span{});
static_assert(source_span{source_pos{0, 1, 1}, source_pos{3, 1, 4}} ==
              source_span{source_pos{0, 1, 1}, source_pos{3, 1, 4}});

static_assert(parse_error{} == parse_error{});
static_assert(parse_error{source_pos{1, 1, 2}, "oops"} ==
              parse_error{source_pos{1, 1, 2}, "oops"});

TEST_CASE("SourceTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("SourceTest - SourcePosDefaultConstruction") {
    constexpr source_pos pos{};
    CHECK(pos.offset == 0);
    CHECK(pos.line == 1);
    CHECK(pos.column == 1);
}

TEST_CASE("SourceTest - SourcePosEquality") {
    constexpr source_pos a{5, 2, 3};
    constexpr source_pos b{5, 2, 3};
    constexpr source_pos c{6, 2, 4};
    CHECK(a == b);
    CHECK(!(a == c));
}

TEST_CASE("SourceTest - SourceSpanEquality") {
    constexpr source_span s{source_pos{0, 1, 1}, source_pos{3, 1, 4}};
    constexpr source_span t{source_pos{0, 1, 1}, source_pos{3, 1, 4}};
    CHECK(s == t);
}

TEST_CASE("SourceTest - ParseErrorEquality") {
    constexpr parse_error e1{source_pos{0, 1, 1}, "test"};
    constexpr parse_error e2{source_pos{0, 1, 1}, "test"};
    constexpr parse_error e3{source_pos{1, 1, 2}, "other"};
    CHECK(e1 == e2);
    CHECK(!(e1 == e3));
}

TEST_CASE("SourceTest - ParseErrorNullMessage") {
    constexpr parse_error e1{source_pos{}, nullptr};
    constexpr parse_error e2{source_pos{}, nullptr};
    CHECK(e1 == e2);
}
