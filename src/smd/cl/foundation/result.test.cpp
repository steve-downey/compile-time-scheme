// src/smd/cl/foundation/result.test.cpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test (step R8): the substantive result/and_then monad-law
// tests moved to src/smd/kit/foundation/result.test.cpp with the
// definition. This file only verifies that the forwarding shim compiles
// and that the forwarded names are usable from smd::cl::foundation exactly
// as before, including the value_type alias and equality that
// result_instances.hpp (which stays here) depends on.

#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/result.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

using smd::cl::foundation::and_then;
using smd::cl::foundation::parse_error;
using smd::cl::foundation::result;
using smd::cl::foundation::source_pos;

static_assert(result<int>{42}.has_value());
static_assert(result<int>{42}.value() == 42);
static_assert(std::is_same_v<result<int>::value_type, int>);

TEST_CASE("ResultShimTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ResultShimTest - AndThenThroughShim") {
    constexpr auto doubled = [](int n) -> result<int> { return n * 2; };
    auto const r = and_then(result<int>{21}, doubled);
    CHECK(r.has_value());
    CHECK(r.value() == 42);
}

TEST_CASE("ResultShimTest - ErrorRoundTripsThroughShim") {
    constexpr parse_error err{source_pos{3, 1, 4}, "bad"};
    constexpr result<int> r{err};
    CHECK(!r.has_value());
    CHECK(r.error() == err);
}
