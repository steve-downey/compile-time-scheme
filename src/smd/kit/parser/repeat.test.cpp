// src/smd/kit/parser/repeat.test.cpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/kit/parser/repeat.hpp>
#include <smd/kit/parser/repeat.hpp> // test 2nd include OK

#include <smd/kit/parser/parser.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

using smd::kit::parser::char_p;
using smd::kit::parser::cursor;
using smd::kit::parser::no_context;
using smd::kit::parser::pure;
using smd::kit::parser::skip_many;

TEST_CASE("RepeatTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

// Runs a parser over a fresh cursor on the same text, threading ctx. Same
// shape as src/smd/kit/parser/parser.test.cpp's helper -- reused verbatim
// per B2's handoff rather than inventing a variant.
template <class P, class Ctx>
constexpr auto run(P const &p, std::string_view text, Ctx &ctx) {
    return p(cursor{text}, ctx);
}

// Two parsers are equal when they produce the same parse_result for the
// same input and context.
template <class P, class Q, class Ctx>
constexpr auto same_parse(P const &p, Q const &q, std::string_view text,
                          Ctx &ctx) -> bool {
    return run(p, text, ctx) == run(q, text, ctx);
}

constexpr auto is_a = char_p('a');

} // namespace

// --- Laws, static_asserted. --------------------------------------------

// skip_many(p) never fails.
static_assert([] {
    no_context ctx{};
    return run(skip_many(is_a), "aaab", ctx).has_value() &&
           run(skip_many(is_a), "b", ctx).has_value() &&
           run(skip_many(is_a), "", ctx).has_value();
}());

// skip_many(p) at a position where p fails is pure of nothing (monostate)
// at that same position.
static_assert([] {
    no_context ctx{};
    return same_parse(skip_many(is_a), pure(std::monostate{}), "b", ctx) &&
           same_parse(skip_many(is_a), pure(std::monostate{}), "", ctx);
}());

// skip_many(skip_many(p)) is skip_many(p): once p's run is exhausted,
// repeating the (already-exhausting) repetition changes nothing further.
static_assert([] {
    no_context ctx{};
    return same_parse(skip_many(skip_many(is_a)), skip_many(is_a), "aaab",
                      ctx) &&
           same_parse(skip_many(skip_many(is_a)), skip_many(is_a), "b", ctx) &&
           same_parse(skip_many(skip_many(is_a)), skip_many(is_a), "", ctx);
}());

// Bounded-progress: a parser that consumes at least one character on every
// success terminates over ordinary, finite input rather than looping.
static_assert([] {
    no_context ctx{};
    auto const r = run(skip_many(is_a), "aaab", ctx);
    return r.has_value() && r.value().rest.remaining() == "b";
}());

// Bounded-progress against a zero-consumption parser: pure always succeeds
// without consuming anything, which is exactly the shape skip_many must not
// loop forever against. This is the case skip_many(skip_many(p)) exercises
// implicitly once the inner repetition reaches its own fixpoint.
static_assert([] {
    no_context ctx{};
    auto const r = run(skip_many(pure(0)), "abc", ctx);
    return r.has_value() && r.value().rest.remaining() == "abc";
}());

TEST_CASE("RepeatTest - SkipManyNeverFails") {
    no_context ctx{};
    CHECK(run(skip_many(is_a), "aaab", ctx).has_value());
    CHECK(run(skip_many(is_a), "b", ctx).has_value());
    CHECK(run(skip_many(is_a), "", ctx).has_value());
}

TEST_CASE("RepeatTest - SkipManyStopsAtFirstFailure") {
    no_context ctx{};
    auto const r = run(skip_many(is_a), "aaab", ctx);
    REQUIRE(r.has_value());
    CHECK(r.value().rest.remaining() == "b");
}

TEST_CASE(
    "RepeatTest - SkipManyStopsRatherThanLoopingOnAZeroConsumptionParser") {
    no_context ctx{};
    auto const r = run(skip_many(pure(0)), "abc", ctx);
    REQUIRE(r.has_value());
    CHECK(r.value().rest.remaining() == "abc");
}

TEST_CASE("RepeatTest - SkipManyAtImmediateFailureIsPureOfNothing") {
    no_context ctx{};
    CHECK(same_parse(skip_many(is_a), pure(std::monostate{}), "b", ctx));
    CHECK(same_parse(skip_many(is_a), pure(std::monostate{}), "", ctx));
}

TEST_CASE("RepeatTest - SkipManyIsIdempotent") {
    no_context ctx{};
    CHECK(same_parse(skip_many(skip_many(is_a)), skip_many(is_a), "aaab", ctx));
    CHECK(same_parse(skip_many(skip_many(is_a)), skip_many(is_a), "b", ctx));
}

TEST_CASE("RepeatTest - SkipManyConsumesToEndOfInputWhenNeverFailing") {
    no_context ctx{};
    auto const r = run(skip_many(is_a), "aaaa", ctx);
    REQUIRE(r.has_value());
    CHECK(r.value().rest.remaining().empty());
}
