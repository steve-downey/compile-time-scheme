// src/smd/cl/eval/outcome.test.cpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/eval/outcome.hpp>
#include <smd/cl/eval/outcome.hpp> // test 2nd include OK

#include <smd/cl/eval/value.hpp>
#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/symbol/symbol_id.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>

using smd::cl::eval::outcome;
using smd::cl::eval::symbol_value;
using smd::cl::eval::to_outcome;
using smd::cl::eval::unwind;
using smd::cl::eval::value;
using smd::cl::eval::value_fixnum;
using smd::cl::foundation::parse_error;
using smd::cl::foundation::result;
using smd::cl::symbol::symbol_id;

namespace {

constexpr symbol_id here{7};
constexpr symbol_id elsewhere{8};

constexpr auto a_value = value{value_fixnum{1}};
constexpr auto an_error = parse_error{{}, "diagnosed"};
constexpr auto an_unwind = unwind{here, value{value_fixnum{2}}};

// ------------------------------------------------------------------
// The three channels are three, and exactly one holds at a time.
// ------------------------------------------------------------------

constexpr auto exactly_one_channel_holds(outcome const &o) -> bool {
    return static_cast<int>(o.is_value()) + static_cast<int>(o.is_error()) +
               static_cast<int>(o.is_unwind()) ==
           1;
}

constexpr auto three_channels_are_disjoint() -> bool {
    return exactly_one_channel_holds(outcome{a_value}) &&
           exactly_one_channel_holds(outcome{an_error}) &&
           exactly_one_channel_holds(outcome{an_unwind});
}

constexpr auto each_channel_returns_what_it_was_given() -> bool {
    return outcome{a_value}.as_value() == a_value &&
           outcome{an_error}.as_error() == an_error &&
           outcome{an_unwind}.as_unwind() == an_unwind;
}

// Decision D13's point, stated as a test: an unwind is not an error. The
// pivot's closure backends could not say this, because both travelled the
// error channel of a two-alternative result and were told apart by comparing
// a `parse_error` message *by pointer identity*. Here the discriminator is
// the type, so there is nothing to compare.
constexpr auto an_unwind_is_not_an_error() -> bool {
    return !outcome{an_unwind}.is_error() && !outcome{an_error}.is_unwind() &&
           outcome{an_unwind} != outcome{an_error};
}

// ------------------------------------------------------------------
// The unwind knows where it is going
// ------------------------------------------------------------------

// "Is this aimed at me?" is a question about the target, asked by whoever is
// asking — no evaluator-wide register, and no sentinel.
constexpr auto an_unwind_names_its_target() -> bool {
    return an_unwind.target == here && an_unwind.target != elsewhere &&
           an_unwind.payload == value{value_fixnum{2}} &&
           unwind{here, a_value} != unwind{elsewhere, a_value};
}

constexpr auto an_unwind_may_carry_any_value() -> bool {
    return unwind{here, symbol_value(elsewhere)}.payload ==
           symbol_value(elsewhere);
}

// ------------------------------------------------------------------
// Widening from the two-alternative result
// ------------------------------------------------------------------

// The safe direction, and the only one that is a function at all: an unwind
// in flight has nowhere to go in a type with only value and error.
constexpr auto widening_preserves_the_channel() -> bool {
    return to_outcome(result<value>{a_value}) == outcome{a_value} &&
           to_outcome(result<value>{an_error}) == outcome{an_error} &&
           !to_outcome(result<value>{an_error}).is_unwind();
}

constexpr auto widening_keeps_the_message() -> bool {
    auto const widened = to_outcome(result<value>{an_error});
    return widened.is_error() &&
           std::string_view{widened.as_error().message} == "diagnosed";
}

} // namespace

static_assert(three_channels_are_disjoint());
static_assert(each_channel_returns_what_it_was_given());
static_assert(an_unwind_is_not_an_error());
static_assert(an_unwind_names_its_target());
static_assert(an_unwind_may_carry_any_value());
static_assert(widening_preserves_the_channel());
static_assert(widening_keeps_the_message());

TEST_CASE("OutcomeTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("OutcomeTest - ThreeChannels") {
    CHECK(three_channels_are_disjoint());
    CHECK(each_channel_returns_what_it_was_given());
    CHECK(an_unwind_is_not_an_error());
}

TEST_CASE("OutcomeTest - UnwindNamesItsTarget") {
    CHECK(an_unwind_names_its_target());
    CHECK(an_unwind_may_carry_any_value());
}

TEST_CASE("OutcomeTest - Widening") {
    CHECK(widening_preserves_the_channel());
    CHECK(widening_keeps_the_message());
}
