// src/smd/cl/foundation/monoid.test.cpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test (step R8, decision 2 corrected): the substantive monoid
// law tests moved to src/smd/kit/foundation/monoid.test.cpp with the
// definition. This file only verifies that the forwarding shim compiles
// and that the forwarded names are usable from smd::cl::foundation exactly
// as before.

#include <smd/cl/foundation/monoid.hpp>
#include <smd/cl/foundation/monoid.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::sum_monoid;

TEST_CASE("MonoidShimTest - HeaderIsIdempotent") { REQUIRE(true); }

static_assert(sum_monoid.combine(sum_monoid.empty(), 7) == 7);

TEST_CASE("MonoidShimTest - SumMonoidThroughShim") {
    CHECK(sum_monoid.combine(3, 4) == 7);
}
