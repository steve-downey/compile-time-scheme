// src/smd/kit/foundation/monoid.test.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Moved in step R8 (decision 2, corrected) from
// src/smd/cl/foundation/monoid.test.cpp.

#include <smd/kit/foundation/monoid.hpp>
#include <smd/kit/foundation/monoid.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <limits>

using smd::kit::foundation::all;
using smd::kit::foundation::any;
using smd::kit::foundation::maximum;
using smd::kit::foundation::maximum_monoid_t;
using smd::kit::foundation::minimum;
using smd::kit::foundation::minimum_monoid_t;
using smd::kit::foundation::monoid;
using smd::kit::foundation::product;
using smd::kit::foundation::sum;
using smd::kit::foundation::all_monoid;
using smd::kit::foundation::any_monoid;
using smd::kit::foundation::monoid_for;
using smd::kit::foundation::sum_monoid;
using smd::kit::foundation::unit;
using smd::kit::foundation::unit_monoid;

TEST_CASE("MonoidTest - HeaderIsIdempotent") { REQUIRE(true); }

// Each instance object models the monoid_for concept over its carrier.
static_assert(monoid_for<decltype(unit_monoid), unit>);
static_assert(monoid_for<decltype(sum_monoid<int>), int>);
static_assert(monoid_for<decltype(all_monoid), bool>);
static_assert(monoid_for<decltype(any_monoid), bool>);

// Law: identity. combine(empty(), a) == a == combine(a, empty()).
static_assert(sum_monoid<int>.combine(sum_monoid<int>.identity(), 7) == 7);
static_assert(sum_monoid<int>.combine(7, sum_monoid<int>.identity()) == 7);
static_assert(all_monoid.combine(all_monoid.identity(), false) == false);
static_assert(all_monoid.combine(false, all_monoid.identity()) == false);
static_assert(any_monoid.combine(any_monoid.identity(), true) == true);
static_assert(any_monoid.combine(true, any_monoid.identity()) == true);
static_assert(unit_monoid.combine(unit_monoid.identity(), unit{}) == unit{});

// Law: associativity.
static_assert(sum_monoid<int>.combine(sum_monoid<int>.combine(1, 2), 3) ==
              sum_monoid<int>.combine(1, sum_monoid<int>.combine(2, 3)));
static_assert(all_monoid.combine(all_monoid.combine(true, false), true) ==
              all_monoid.combine(true, all_monoid.combine(false, true)));
static_assert(any_monoid.combine(any_monoid.combine(false, true), false) ==
              any_monoid.combine(false, any_monoid.combine(true, false)));

TEST_CASE("MonoidTest - SumIdentityLaw") {
    CHECK(sum_monoid<int>.combine(sum_monoid<int>.identity(), 7) == 7);
    CHECK(sum_monoid<int>.combine(7, sum_monoid<int>.identity()) == 7);
}

TEST_CASE("MonoidTest - SumAssociativityLaw") {
    CHECK(sum_monoid<int>.combine(sum_monoid<int>.combine(1, 2), 3) ==
          sum_monoid<int>.combine(1, sum_monoid<int>.combine(2, 3)));
}

TEST_CASE("MonoidTest - BoolMonoids") {
    CHECK(all_monoid.combine(true, true));
    CHECK(!all_monoid.combine(true, false));
    CHECK(any_monoid.combine(false, true));
    CHECK(!any_monoid.combine(false, false));
}

TEST_CASE("MonoidTest - UnitCarrierHasOneValue") {
    CHECK(unit_monoid.identity() == unit{});
    CHECK(unit_monoid.combine(unit{}, unit{}) == unit{});
}

// --- No numeric defaults. -------------------------------------------------
//
// A number is not a monoid; a number under a chosen operation is. Nothing
// registers int or bool, so no caller silently gets addition or conjunction
// because the library picked one. This is the sentinel for that decision: if
// a later change registers a bare number, it fails here rather than
// somewhere a fold quietly means something else.

static_assert(!smd::kit::foundation::has_instance_v<decltype(monoid<int>)>);
static_assert(!smd::kit::foundation::has_instance_v<decltype(monoid<long>)>);
static_assert(
    !smd::kit::foundation::has_instance_v<decltype(monoid<std::size_t>)>);
static_assert(!smd::kit::foundation::has_instance_v<decltype(monoid<bool>)>);
static_assert(!smd::kit::foundation::has_instance_v<decltype(monoid<double>)>);

// --- The named carriers, each registered under its own name. --------------

static_assert(smd::kit::foundation::has_instance_v<decltype(monoid<unit>)>);
static_assert(
    smd::kit::foundation::has_instance_v<decltype(monoid<sum<int>>)>);
static_assert(
    smd::kit::foundation::has_instance_v<decltype(monoid<product<int>>)>);
static_assert(smd::kit::foundation::has_instance_v<decltype(monoid<any>)>);
static_assert(smd::kit::foundation::has_instance_v<decltype(monoid<all>)>);

namespace {

/// The two laws, over a registered carrier, at compile time.
template <class Carrier>
constexpr auto laws_hold(Carrier a, Carrier b, Carrier c) -> bool {
    auto const &m = monoid<Carrier>;
    return m.combine(m.identity(), a) == a &&
           m.combine(a, m.identity()) == a &&
           m.combine(m.combine(a, b), c) == m.combine(a, m.combine(b, c));
}

} // namespace

static_assert(laws_hold(sum<int>{2}, sum<int>{3}, sum<int>{5}));
static_assert(laws_hold(product<int>{2}, product<int>{3}, product<int>{5}));
static_assert(laws_hold(maximum<int>{2}, maximum<int>{3}, maximum<int>{5}));
static_assert(laws_hold(minimum<int>{2}, minimum<int>{3}, minimum<int>{5}));
static_assert(laws_hold(any{true}, any{false}, any{true}));
static_assert(laws_hold(all{true}, all{false}, all{true}));
static_assert(laws_hold(unit{}, unit{}, unit{}));

static_assert(monoid<sum<int>>.combine(sum<int>{2}, sum<int>{3}) ==
              sum<int>{5});
static_assert(monoid<product<int>>.combine(product<int>{2}, product<int>{3}) ==
              product<int>{6});
static_assert(monoid<maximum<int>>.combine(maximum<int>{2}, maximum<int>{3}) ==
              maximum<int>{3});
static_assert(monoid<minimum<int>>.combine(minimum<int>{2}, minimum<int>{3}) ==
              minimum<int>{2});

// The saturating bounds are the identities, so no adjoined element and no
// widened type. A carrier with infinities uses them instead.
static_assert(maximum_monoid_t<int>{}.identity() ==
              std::numeric_limits<int>::lowest());
static_assert(minimum_monoid_t<int>{}.identity() ==
              std::numeric_limits<int>::max());
static_assert(maximum_monoid_t<double>{}.identity() ==
              -std::numeric_limits<double>::infinity());

TEST_CASE("MonoidTest - NamedCarriersAtRuntime") {
    CHECK(monoid<sum<int>>.combine(sum<int>{2}, sum<int>{3}) == sum<int>{5});
    CHECK(monoid<any>.combine(any{false}, any{true}) == any{true});
    CHECK(monoid<all>.combine(all{true}, all{false}) == all{false});
}
