// src/smd/kit/parser/parser.test.cpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/kit/parser/parser.hpp>
#include <smd/kit/parser/parser.hpp> // test 2nd include OK

#include <smd/kit/foundation/parse_error.hpp>
#include <smd/kit/foundation/source_pos.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>

using smd::kit::foundation::parse_error;
using smd::kit::foundation::source_pos;
using smd::kit::parser::char_p;
using smd::kit::parser::cursor;
using smd::kit::parser::map;
using smd::kit::parser::no_context;
using smd::kit::parser::parse_context;
using smd::kit::parser::parse_result;
using smd::kit::parser::parse_state;
using smd::kit::parser::parser;
using smd::kit::parser::pure;
using smd::kit::parser::satisfy;

TEST_CASE("ParserTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

// A non-trivial context: the Functor laws must hold for a context a parser
// actually reads, not only for the empty no_context (per B2's step file).
struct scale_context {
    int factor = 1;
};

// Ignores its context; runnable under any threaded context, including
// no_context.
constexpr auto char_val = parser{[](cursor cur,
                                    parse_context auto &) -> parse_result<int> {
    if (cur.empty()) {
        return parse_error{cur.position(), "expected char"};
    }
    return parse_state<int>{static_cast<unsigned char>(cur.peek()), cur.bump()};
}};

// Reads its context: the parsed value depends on scale_context::factor, so
// this genuinely exercises the threaded context rather than ignoring it.
constexpr auto scaled_char =
    parser{[](cursor cur, scale_context &ctx) -> parse_result<int> {
        if (cur.empty()) {
            return parse_error{cur.position(), "expected char"};
        }
        return parse_state<int>{
            static_cast<unsigned char>(cur.peek()) * ctx.factor, cur.bump()};
    }};

// Runs a parser over a fresh cursor on the same text, threading ctx.
template <class P, class Ctx>
constexpr auto run(P const &p, std::string_view text, Ctx &ctx) {
    return p(cursor{text}, ctx);
}

// Two parsers are equal when they produce the same parse_result -- same
// value, same rest cursor, same error and error position -- for the same
// input and context.
template <class P, class Q, class Ctx>
constexpr auto same_parse(P const &p, Q const &q, std::string_view text,
                          Ctx &ctx) -> bool {
    return run(p, text, ctx) == run(q, text, ctx);
}

constexpr auto add_one = [](int x) { return x + 1; };
constexpr auto times_two = [](int x) { return x * 2; };

} // namespace

// --- Functor laws, over no_context. ----------------------------------------

static_assert([] {
    no_context ctx{};
    auto const id = [](int x) { return x; };
    return same_parse(map(char_val, id), char_val, "a", ctx) &&
           same_parse(map(char_val, id), char_val, "", ctx);
}());

static_assert([] {
    no_context ctx{};
    auto const composed = [](int x) { return times_two(add_one(x)); };
    return same_parse(map(map(char_val, add_one), times_two),
                      map(char_val, composed), "a", ctx) &&
           same_parse(map(map(char_val, add_one), times_two),
                      map(char_val, composed), "", ctx);
}());

// --- The same two laws, over a context the parser actually reads. ----------
//
// A law that only holds for the empty context is not the law.

static_assert([] {
    scale_context ctx{3};
    auto const id = [](int x) { return x; };
    return same_parse(map(scaled_char, id), scaled_char, "a", ctx) &&
           same_parse(map(scaled_char, id), scaled_char, "", ctx);
}());

static_assert([] {
    scale_context ctx{3};
    auto const composed = [](int x) { return times_two(add_one(x)); };
    return same_parse(map(map(scaled_char, add_one), times_two),
                      map(scaled_char, composed), "a", ctx) &&
           same_parse(map(map(scaled_char, add_one), times_two),
                      map(scaled_char, composed), "", ctx);
}());

TEST_CASE("ParserTest - FunctorIdentityLaw") {
    no_context ctx{};
    auto const id = [](int x) { return x; };
    CHECK(same_parse(map(char_val, id), char_val, "a", ctx));
    CHECK(same_parse(map(char_val, id), char_val, "", ctx));
}

TEST_CASE("ParserTest - FunctorCompositionLaw") {
    no_context ctx{};
    auto const composed = [](int x) { return times_two(add_one(x)); };
    CHECK(same_parse(map(map(char_val, add_one), times_two),
                     map(char_val, composed), "a", ctx));
    CHECK(same_parse(map(map(char_val, add_one), times_two),
                     map(char_val, composed), "", ctx));
}

TEST_CASE("ParserTest - FunctorLawsHoldForANonTrivialContext") {
    scale_context ctx{3};
    auto const id = [](int x) { return x; };
    auto const composed = [](int x) { return times_two(add_one(x)); };
    CHECK(same_parse(map(scaled_char, id), scaled_char, "a", ctx));
    CHECK(same_parse(map(map(scaled_char, add_one), times_two),
                     map(scaled_char, composed), "a", ctx));
}

// --- Breathing tests for pure and map, beyond the laws. --------------------

TEST_CASE("ParserTest - PureConsumesNoInput") {
    no_context ctx{};
    auto const p = pure(42);
    auto const r = p(cursor{"xyz"}, ctx);
    REQUIRE(r.has_value());
    CHECK(r.value().value == 42);
    CHECK(r.value().rest == cursor{"xyz"});
}

TEST_CASE("ParserTest - MapAppliesOnSuccess") {
    no_context ctx{};
    auto const p = map(char_val, [](int x) { return x + 1; });
    auto const r = p(cursor{"a"}, ctx);
    REQUIRE(r.has_value());
    CHECK(r.value().value == static_cast<int>('a') + 1);
}

TEST_CASE("ParserTest - MapPassesErrorThrough") {
    no_context ctx{};
    auto const p = map(char_val, [](int x) { return x + 1; });
    auto const r = p(cursor{""}, ctx);
    REQUIRE_FALSE(r.has_value());
    CHECK(std::string_view{r.error().message} == "expected char");
}

TEST_CASE("ParserTest - ScaledCharReadsItsContext") {
    scale_context ctx{5};
    auto const r = scaled_char(cursor{"a"}, ctx);
    REQUIRE(r.has_value());
    CHECK(r.value().value == static_cast<int>('a') * 5);
}

// --- satisfy and char_p, for any threaded context. -------------------------

static_assert([] {
    no_context ctx{};
    auto const digit =
        satisfy([](char c) { return c >= '0' && c <= '9'; }, "expected digit");
    auto const r = run(digit, "5x", ctx);
    return r.has_value() && r.value().value == '5' &&
           r.value().rest.remaining() == "x";
}());

static_assert([] {
    no_context ctx{};
    auto const digit =
        satisfy([](char c) { return c >= '0' && c <= '9'; }, "expected digit");
    auto const r = run(digit, "x5", ctx);
    return !r.has_value() && r.error().where.offset == 0 &&
           std::string_view{r.error().message} == "expected digit";
}());

static_assert([] {
    no_context ctx{};
    return run(char_p('('), "(rest", ctx).has_value() &&
           run(char_p('('), "(rest", ctx).value().rest.remaining() == "rest" &&
           !run(char_p('('), ")rest", ctx).has_value();
}());

TEST_CASE("ParserTest - SatisfyConsumesOneCharacterOnMatch") {
    no_context ctx{};
    auto const digit =
        satisfy([](char c) { return c >= '0' && c <= '9'; }, "expected digit");
    auto const r = run(digit, "5x", ctx);
    REQUIRE(r.has_value());
    CHECK(r.value().value == '5');
    CHECK(r.value().rest.remaining() == "x");
}

TEST_CASE("ParserTest - SatisfyFailsAtCurrentPositionWithoutConsuming") {
    no_context ctx{};
    auto const digit =
        satisfy([](char c) { return c >= '0' && c <= '9'; }, "expected digit");
    auto const r = run(digit, "x5", ctx);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().where.offset == 0);
    CHECK(std::string_view{r.error().message} == "expected digit");
}

TEST_CASE("ParserTest - SatisfyFailsOnEmptyInput") {
    no_context ctx{};
    auto const digit =
        satisfy([](char c) { return c >= '0' && c <= '9'; }, "expected digit");
    CHECK_FALSE(run(digit, "", ctx).has_value());
}

TEST_CASE("ParserTest - CharPMatchesExactCharacter") {
    no_context ctx{};
    auto const r = run(char_p('('), "(rest", ctx);
    REQUIRE(r.has_value());
    CHECK(r.value().value == '(');
    CHECK(r.value().rest.remaining() == "rest");
}

TEST_CASE("ParserTest - CharPFailsOnMismatch") {
    no_context ctx{};
    CHECK_FALSE(run(char_p('('), ")rest", ctx).has_value());
}
