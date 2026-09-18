// src/smd/kit/parser/parse_context.test.cpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/kit/parser/parse_context.hpp>
#include <smd/kit/parser/parse_context.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::kit::parser::cursor;
using smd::kit::parser::no_context;
using smd::kit::parser::parse_context;

namespace {

// An ordinary, non-trivial context: the concept says nothing about shape.
struct example_context {
    int calls = 0;
};

} // namespace

static_assert(parse_context<no_context>);
static_assert(parse_context<example_context>);
static_assert(parse_context<int>);
static_assert(!parse_context<cursor>);
static_assert(!parse_context<cursor const>);
static_assert(!parse_context<cursor &>);

TEST_CASE("ParseContextTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ParseContextTest - NamesTheRoleRatherThanTheShape") {
    // Any object type not a cursor qualifies -- the concept exists to catch
    // an argument-order mistake, not to constrain what a context looks like.
    CHECK(parse_context<no_context>);
    CHECK(parse_context<example_context>);
    CHECK(parse_context<int>);
}

TEST_CASE("ParseContextTest - RejectsACursorPassedAsContext") {
    CHECK_FALSE(parse_context<cursor>);
    CHECK_FALSE(parse_context<cursor const>);
    CHECK_FALSE(parse_context<cursor &>);
}
