// src/smd/cl/foundation/result.test.cpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Reviewed union of the two prior copies of this test:
// src/smd/smdscheme/foundation/result.test.cpp (compile-time-scheme) and
// src/smd/forth/foundation/result.test.cpp (compile-time-forth).

#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/result.hpp> // test 2nd include OK
#include <smd/cl/foundation/result_instances.hpp>

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::bind;
using smd::cl::foundation::parse_error;
using smd::cl::foundation::result;
using smd::cl::foundation::source_pos;

static_assert(result<int>{42}.has_value());
static_assert(result<int>{42}.value() == 42);
static_assert(!result<int>{parse_error{}}.has_value());

namespace {

constexpr parse_error first{source_pos{1, 1, 1}, "first"};
constexpr parse_error second{source_pos{2, 1, 2}, "second"};

constexpr auto doubled(int n) -> result<int> { return n * 2; }
constexpr auto always_fails(int) -> result<int> { return second; }

// The monad laws, which are what make this a bind rather than a helper.
constexpr auto left_identity() -> bool {
    return bind(result<int>{21}, doubled) == doubled(21);
}

constexpr auto right_identity() -> bool {
    constexpr auto pure = [](int n) { return result<int>{n}; };
    return bind(result<int>{21}, pure) == result<int>{21} &&
           bind(result<int>{first}, pure) == result<int>{first};
}

constexpr auto associativity() -> bool {
    constexpr auto plus_one = [](int n) { return result<int>{n + 1}; };
    auto const left = bind(bind(result<int>{5}, doubled), plus_one);
    auto const right =
        bind(result<int>{5}, [&](int n) { return bind(doubled(n), plus_one); });
    return left == right && left == result<int>{11};
}

// The point of the bind: a failed step's error passes through untouched and
// the continuation never runs, so no ladder is needed to skip it.
constexpr auto an_error_short_circuits() -> bool {
    return bind(result<int>{first}, always_fails) == result<int>{first};
}

// The continuation may change the carried type.
constexpr auto the_carried_type_may_change() -> bool {
    auto const widened =
        bind(result<int>{7}, [](int n) { return result<bool>{n > 0}; });
    return widened.has_value() && widened.value();
}

} // namespace

static_assert(left_identity());
static_assert(right_identity());
static_assert(associativity());
static_assert(an_error_short_circuits());
static_assert(the_carried_type_may_change());

TEST_CASE("ResultTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ResultTest - AndThenObeysTheMonadLaws") {
    CHECK(left_identity());
    CHECK(right_identity());
    CHECK(associativity());
}

TEST_CASE("ResultTest - AndThenPropagatesErrors") {
    CHECK(an_error_short_circuits());
    CHECK(the_carried_type_may_change());
}

TEST_CASE("ResultTest - ConstructFromValue") {
    constexpr result<int> r{7};
    CHECK(r.has_value());
    CHECK(r.value() == 7);
}

TEST_CASE("ResultTest - ConstructFromError") {
    constexpr parse_error err{source_pos{3, 1, 4}, "bad"};
    constexpr result<int> r{err};
    CHECK(!r.has_value());
    CHECK(r.error() == err);
}

TEST_CASE("ResultTest - ValueTypePreservation") {
    constexpr result<bool> r{true};
    CHECK(r.has_value());
    CHECK(r.value() == true);
}
