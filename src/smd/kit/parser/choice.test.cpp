// src/smd/kit/parser/choice.test.cpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/kit/parser/choice.hpp>
#include <smd/kit/parser/choice.hpp> // test 2nd include OK

#include <smd/kit/foundation/parse_error.hpp>
#include <smd/kit/parser/parser.hpp>
#include <smd/kit/parser/parser_instances.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string_view>

using smd::kit::foundation::bind;
using smd::kit::foundation::parse_error;
using smd::kit::parser::alt;
using smd::kit::parser::char_p;
using smd::kit::parser::cursor;
using smd::kit::parser::no_context;
using smd::kit::parser::optional;
using smd::kit::parser::parse_context;
using smd::kit::parser::parse_result;
using smd::kit::parser::parser;

TEST_CASE("ChoiceTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

// Fails at the current position without consuming, for any threaded
// context: the identity element ordered choice needs for its left-identity
// law.
constexpr auto never =
    parser{[](cursor cur, parse_context auto &) -> parse_result<char> {
        return parse_error{cur.position(), "never"};
    }};

constexpr auto is_a = char_p('a');
constexpr auto is_b = char_p('b');
constexpr auto is_c = char_p('c');

// Matches 'x' and then fails without consuming anything further: a
// parser that has committed (consumed the 'x') before its failure, so the
// failure must propagate rather than fall through to a second
// alternative.
constexpr auto x_then_fail = bind(char_p('x'), [](char) {
    return parser{[](cursor cur, parse_context auto &) -> parse_result<char> {
        return parse_error{cur.position(), "deep failure"};
    }};
});

template <class P, class Ctx>
constexpr auto run(P const &p, std::string_view text, Ctx &ctx) {
    return p(cursor{text}, ctx);
}

template <class P, class Q, class Ctx>
constexpr auto same_parse(P const &p, Q const &q, std::string_view text,
                          Ctx &ctx) -> bool {
    return run(p, text, ctx) == run(q, text, ctx);
}

} // namespace

// --- Alternative laws, static_asserted. ------------------------------------

// Left identity: the always-failing parser is a no-op on the left of `|`.
static_assert([] {
    no_context ctx{};
    return same_parse(never | is_a, is_a, "a", ctx) &&
           same_parse(never | is_a, is_a, "b", ctx) &&
           same_parse(never | is_a, is_a, "", ctx);
}());

// Associativity of choice.
static_assert([] {
    no_context ctx{};
    return same_parse((is_a | is_b) | is_c, is_a | (is_b | is_c), "a", ctx) &&
           same_parse((is_a | is_b) | is_c, is_a | (is_b | is_c), "b", ctx) &&
           same_parse((is_a | is_b) | is_c, is_a | (is_b | is_c), "c", ctx) &&
           same_parse((is_a | is_b) | is_c, is_a | (is_b | is_c), "d", ctx);
}());

// No backtracking after consumption: a left alternative that consumes and
// then fails wins outright, even though a later alternative would have
// matched the very next character.
static_assert([] {
    no_context ctx{};
    auto const r = run(x_then_fail | is_a, "xa", ctx);
    return !r.has_value() &&
           std::string_view{r.error().message} == "deep failure";
}());

// Diagnostic survivorship on a double failure: the second (right-most
// tried) alternative's message and position are what a chain reports, not
// a synthesized "expected one of ...". Documented at the operator|
// anchor; pinned here as a law rather than only as prose, because B7's
// readtable dispatch depends on this exact behaviour.
static_assert([] {
    no_context ctx{};
    auto const r = run(is_a | is_b, "z", ctx);
    return !r.has_value() && r.error() == is_b(cursor{"z"}, ctx).error();
}());

TEST_CASE("ChoiceTest - LeftIdentityOfTheFailingParser") {
    no_context ctx{};
    CHECK(same_parse(never | is_a, is_a, "a", ctx));
    CHECK(same_parse(never | is_a, is_a, "", ctx));
}

TEST_CASE("ChoiceTest - Associativity") {
    no_context ctx{};
    CHECK(same_parse((is_a | is_b) | is_c, is_a | (is_b | is_c), "b", ctx));
    CHECK(same_parse((is_a | is_b) | is_c, is_a | (is_b | is_c), "d", ctx));
}

TEST_CASE("ChoiceTest - FirstAlternativeWinsOnSuccess") {
    no_context ctx{};
    auto const r = run(is_a | is_b, "a", ctx);
    REQUIRE(r.has_value());
    CHECK(r.value().value == 'a');
}

TEST_CASE("ChoiceTest - FallsThroughToSecondAlternativeWithoutConsumption") {
    no_context ctx{};
    auto const r = run(is_a | is_b, "b", ctx);
    REQUIRE(r.has_value());
    CHECK(r.value().value == 'b');
}

TEST_CASE("ChoiceTest - ConsumedFailurePropagatesRatherThanFallingThrough") {
    no_context ctx{};
    auto const r = run(x_then_fail | is_a, "xa", ctx);
    REQUIRE_FALSE(r.has_value());
    CHECK(std::string_view{r.error().message} == "deep failure");
}

TEST_CASE("ChoiceTest - DoubleFailureReportsTheSecondAlternativesError") {
    no_context ctx{};
    auto const chain_error = run(is_a | is_b, "z", ctx).error();
    auto const second_alone_error = is_b(cursor{"z"}, ctx).error();
    CHECK(chain_error == second_alone_error);
}

TEST_CASE("ChoiceTest - AltIsOperatorPipe") {
    no_context ctx{};
    CHECK(same_parse(alt(is_a, is_b), is_a | is_b, "a", ctx));
    CHECK(same_parse(alt(is_a, is_b), is_a | is_b, "b", ctx));
    CHECK(same_parse(alt(is_a, is_b), is_a | is_b, "z", ctx));
}

// --- optional. --------------------------------------------------------------

TEST_CASE("OptionalTest - EngagedOnSuccess") {
    no_context ctx{};
    auto const r = run(optional(is_a), "ab", ctx);
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.has_value());
    CHECK(*r.value().value == 'a');
    CHECK(r.value().rest.remaining() == "b");
}

TEST_CASE("OptionalTest - NulloptAtOriginalCursorOnUnconsumedFailure") {
    no_context ctx{};
    auto const r = run(optional(is_a), "b", ctx);
    REQUIRE(r.has_value());
    CHECK_FALSE(r.value().value.has_value());
    CHECK(r.value().rest.remaining() == "b");
}

TEST_CASE("OptionalTest - PropagatesFailureAfterConsumption") {
    no_context ctx{};
    auto const r = run(optional(x_then_fail), "xa", ctx);
    REQUIRE_FALSE(r.has_value());
    CHECK(std::string_view{r.error().message} == "deep failure");
}
