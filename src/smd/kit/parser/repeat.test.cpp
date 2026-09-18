// src/smd/kit/parser/repeat.test.cpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/kit/parser/repeat.hpp>
#include <smd/kit/parser/repeat.hpp> // test 2nd include OK

#include <smd/kit/foundation/parse_error.hpp>
#include <smd/kit/foundation/static_vector.hpp>
#include <smd/kit/parser/parser.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string_view>
#include <variant>

using smd::kit::foundation::parse_error;
using smd::kit::foundation::static_vector;
using smd::kit::parser::char_p;
using smd::kit::parser::cursor;
using smd::kit::parser::many_until;
using smd::kit::parser::no_context;
using smd::kit::parser::parse_context;
using smd::kit::parser::parse_result;
using smd::kit::parser::parse_state;
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

// --- many_until: the diagnosing collecting repetition. ---------------------
//
// many_until itself owns no accumulator or capacity -- these tests build
// one the same way src/smd/cl/reader/detail/forms.hpp's read_delimited
// does, a step closure that pushes into a static_vector it captures by
// reference and diagnoses its own overflow, so what is under test here is
// the repetition's control flow: stop cleanly on std::nullopt, propagate
// a step failure (including that overflow) unchanged, and do not loop on
// a zero-consumption "keep going" value.

namespace {

// Reads one 'a', collecting it into out (capacity Capacity) unless out is
// already full, in which case that is a failure with overflow_message at
// the position of the 'a' that would not fit -- the same shape
// read_delimited's own step gives its element check. Stops (std::nullopt)
// at the first non-'a' character or end of input, without consuming.
template <int Capacity>
constexpr auto collect_as(static_vector<char, Capacity> &out,
                          char const *overflow_message) {
    return [&out, overflow_message](
               cursor cur,
               parse_context auto &ctx) -> parse_result<std::optional<char>> {
        auto const r = char_p('a')(cur, ctx);
        if (!r.has_value()) {
            return parse_state<std::optional<char>>{std::nullopt, cur};
        }
        if (out.size() >= out.capacity()) {
            return parse_error{cur.position(), overflow_message};
        }
        out.push_back(r.value().value);
        return parse_state<std::optional<char>>{r.value().value,
                                                r.value().rest};
    };
}

// A step that always yields a value without ever consuming: the hazard
// many_until must not spin on, the collecting counterpart of
// skip_many(pure(0)) in the section above.
constexpr auto zero_consuming_step =
    [](cursor cur, parse_context auto &) -> parse_result<std::optional<int>> {
    return parse_state<std::optional<int>>{std::optional<int>{0}, cur};
};

} // namespace

TEST_CASE("ManyUntilTest - StopsAtNulloptAndKeepsWhatWasCollected") {
    no_context ctx{};
    static_vector<char, 4> out;
    auto const r = run(many_until(collect_as(out, "too many")), "aab", ctx);
    REQUIRE(r.has_value());
    CHECK(r.value().rest.remaining() == "b");
    REQUIRE(out.size() == 2);
    CHECK(out[0] == 'a');
    CHECK(out[1] == 'a');
}

TEST_CASE("ManyUntilTest - StopsAtEndOfInputWithWhatWasCollected") {
    no_context ctx{};
    static_vector<char, 4> out;
    auto const r = run(many_until(collect_as(out, "too many")), "aa", ctx);
    REQUIRE(r.has_value());
    CHECK(r.value().rest.remaining().empty());
    CHECK(out.size() == 2);
}

TEST_CASE("ManyUntilTest - DiagnosesAtCapacityPlusOneRatherThanTruncating") {
    // The property the step file asks for by name: at capacity plus one,
    // many_until fails, with the supplied message, at the position of the
    // element that would not fit -- not the retired many<Capacity>'s
    // silent success at Capacity elements.
    no_context ctx{};
    static_vector<char, 2> out;
    auto const r = run(many_until(collect_as(out, "too many")), "aaaa", ctx);
    REQUIRE_FALSE(r.has_value());
    CHECK(std::string_view{r.error().message} == "too many");
    CHECK(r.error().where.offset == 2);
    // The two that did fit are still in out: many_until propagates the
    // step's failure, it does not unwind what the step already committed.
    REQUIRE(out.size() == 2);
}

TEST_CASE("ManyUntilTest - PropagatesAnOrdinaryStepFailure") {
    no_context ctx{};
    auto const always_fails =
        [](cursor cur,
           parse_context auto &) -> parse_result<std::optional<int>> {
        return parse_error{cur.position(), "ordinary failure"};
    };
    auto const r = run(many_until(always_fails), "x", ctx);
    REQUIRE_FALSE(r.has_value());
    CHECK(std::string_view{r.error().message} == "ordinary failure");
}

TEST_CASE("ManyUntilTest - StopsRatherThanLoopingOnAZeroConsumptionValue") {
    no_context ctx{};
    auto const r = run(many_until(zero_consuming_step), "abc", ctx);
    REQUIRE(r.has_value());
    CHECK(r.value().rest.remaining() == "abc");
}

static_assert([] {
    no_context ctx{};
    return run(many_until(zero_consuming_step), "abc", ctx).has_value();
}());
