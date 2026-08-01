// src/smd/cl/foundation/monoid.test.cpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/foundation/monoid.hpp>
#include <smd/cl/foundation/monoid.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::all_monoid;
using smd::cl::foundation::any_monoid;
using smd::cl::foundation::monoid_for;
using smd::cl::foundation::sum_monoid;
using smd::cl::foundation::unit;
using smd::cl::foundation::unit_monoid;

TEST_CASE("MonoidTest - HeaderIsIdempotent") { REQUIRE(true); }

// Each instance object models the monoid_for concept over its carrier.
static_assert(monoid_for<decltype(unit_monoid), unit>);
static_assert(monoid_for<decltype(sum_monoid), int>);
static_assert(monoid_for<decltype(all_monoid), bool>);
static_assert(monoid_for<decltype(any_monoid), bool>);

// Law: identity. combine(empty(), a) == a == combine(a, empty()).
static_assert(sum_monoid.combine(sum_monoid.empty(), 7) == 7);
static_assert(sum_monoid.combine(7, sum_monoid.empty()) == 7);
static_assert(all_monoid.combine(all_monoid.empty(), false) == false);
static_assert(all_monoid.combine(false, all_monoid.empty()) == false);
static_assert(any_monoid.combine(any_monoid.empty(), true) == true);
static_assert(any_monoid.combine(true, any_monoid.empty()) == true);
static_assert(unit_monoid.combine(unit_monoid.empty(), unit{}) == unit{});

// Law: associativity.
static_assert(sum_monoid.combine(sum_monoid.combine(1, 2), 3) ==
              sum_monoid.combine(1, sum_monoid.combine(2, 3)));
static_assert(all_monoid.combine(all_monoid.combine(true, false), true) ==
              all_monoid.combine(true, all_monoid.combine(false, true)));
static_assert(any_monoid.combine(any_monoid.combine(false, true), false) ==
              any_monoid.combine(false, any_monoid.combine(true, false)));

TEST_CASE("MonoidTest - SumIdentityLaw") {
    CHECK(sum_monoid.combine(sum_monoid.empty(), 7) == 7);
    CHECK(sum_monoid.combine(7, sum_monoid.empty()) == 7);
}

TEST_CASE("MonoidTest - SumAssociativityLaw") {
    CHECK(sum_monoid.combine(sum_monoid.combine(1, 2), 3) ==
          sum_monoid.combine(1, sum_monoid.combine(2, 3)));
}

TEST_CASE("MonoidTest - BoolMonoids") {
    CHECK(all_monoid.combine(true, true));
    CHECK(!all_monoid.combine(true, false));
    CHECK(any_monoid.combine(false, true));
    CHECK(!any_monoid.combine(false, false));
}

TEST_CASE("MonoidTest - UnitCarrierHasOneValue") {
    CHECK(unit_monoid.empty() == unit{});
    CHECK(unit_monoid.combine(unit{}, unit{}) == unit{});
}
