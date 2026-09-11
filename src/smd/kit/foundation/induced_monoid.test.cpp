// src/smd/kit/foundation/induced_monoid.test.cpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/kit/foundation/induced_monoid.hpp>
#include <smd/kit/foundation/induced_monoid.hpp> // test 2nd include OK

#include <smd/kit/foundation/identity.hpp>
#include <smd/kit/foundation/monoid.hpp>
#include <smd/kit/foundation/parse_error.hpp>
#include <smd/kit/foundation/result.hpp>
#include <smd/kit/foundation/result_instances.hpp>

#include <catch2/catch_test_macros.hpp>

using smd::kit::foundation::identity;
using smd::kit::foundation::identity_applicative_map;
using smd::kit::foundation::kleisli_endo;
using smd::kit::foundation::lifted;
using smd::kit::foundation::monoid;
using smd::kit::foundation::parse_error;
using smd::kit::foundation::result;
using smd::kit::foundation::result_applicative_map;
using smd::kit::foundation::result_monad_map;
using smd::kit::foundation::source_pos;
using smd::kit::foundation::sum;

TEST_CASE("InducedMonoidTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

constexpr auto failure = parse_error{source_pos{}, "boom"};

constexpr auto halve(int const &n) -> result<int> {
    if (n % 2 != 0) {
        return failure;
    }
    return n / 2;
}

constexpr auto increment(int const &n) -> result<int> { return n + 1; }

using endo = kleisli_endo<result_monad_map, int>;

constexpr auto arrow(endo::arrow_type f) -> endo {
    endo e{};
    e.arrows.push_back(f);
    return e;
}

} // namespace

// --- The Kleisli endomorphism monoid. -------------------------------------
//
// pure is the two-sided unit of >=>, and >=> is associative. Those are the
// Kleisli-form monad laws, and they are exactly this monoid's laws -- which
// is the whole reason to name the carrier.

namespace {

constexpr auto kleisli_laws_hold(int input) -> bool {
    auto const &m = monoid<endo>;
    auto const f = arrow(halve);
    auto const g = arrow(increment);
    auto const h = arrow(halve);

    bool const left_unit = m.combine(m.identity(), f)(input) == f(input);
    bool const right_unit = m.combine(f, m.identity())(input) == f(input);
    bool const associative = m.combine(m.combine(f, g), h)(input) ==
                             m.combine(f, m.combine(g, h))(input);

    return left_unit && right_unit && associative;
}

} // namespace

static_assert(kleisli_laws_hold(8));
static_assert(kleisli_laws_hold(7));
static_assert(kleisli_laws_hold(12));

// The identity really is pure: an empty carrier run on a value yields it
// back in the context, unchanged.
static_assert(monoid<endo>.identity()(5) == result_monad_map{}.pure(5));

// Combine interprets as Kleisli composition -- run(fs ++ gs) is
// run(fs) >=> run(gs) -- which is what makes concatenation the right
// operation rather than merely a closed one.
static_assert(monoid<endo>.combine(arrow(halve), arrow(halve))(8) ==
              result_monad_map{}.kleisli(halve, halve)(8));
static_assert(monoid<endo>.combine(arrow(halve), arrow(halve))(7) ==
              result_monad_map{}.kleisli(halve, halve)(7));
static_assert(monoid<endo>.combine(arrow(halve), arrow(increment))(8) ==
              result<int>{5});

// A failed step is not resumed: the rest of the sequence is skipped, because
// it is bind doing the sequencing.
static_assert(monoid<endo>.combine(arrow(halve), arrow(halve))(7) ==
              result<int>{failure});

// --- The applicative-lifted monoid. ---------------------------------------
//
// identity lifts the element monoid's identity with pure; combine lifts its
// combine with invoke. Note the element type: a bare int has no registered
// monoid, so the carrier has to be named -- which is the no-numeric-defaults
// decision doing its work at the one place that would otherwise have picked
// addition on the caller's behalf.

namespace {

using lifted_result = lifted<result_applicative_map, result<sum<int>>>;
using lifted_identity = lifted<identity_applicative_map, identity<sum<int>>>;

template <class Carrier>
constexpr auto lifted_laws_hold(Carrier a, Carrier b, Carrier c) -> bool {
    auto const &m = monoid<Carrier>;
    return m.combine(m.identity(), a) == a && m.combine(a, m.identity()) == a &&
           m.combine(m.combine(a, b), c) == m.combine(a, m.combine(b, c));
}

constexpr auto lift(int n) -> lifted_result {
    return lifted_result{result<sum<int>>{sum<int>{n}}};
}

} // namespace

static_assert(lifted_laws_hold(lift(2), lift(3), lift(5)));
static_assert(lifted_laws_hold(lifted_identity{identity<sum<int>>{sum<int>{2}}},
                               lifted_identity{identity<sum<int>>{sum<int>{3}}},
                               lifted_identity{
                                   identity<sum<int>>{sum<int>{5}}}));

static_assert(monoid<lifted_result>.combine(lift(2), lift(3)) == lift(5));
static_assert(monoid<lifted_result>.identity() ==
              lifted_result{result<sum<int>>{sum<int>{0}}});

// The effect is preserved, not discarded: combining with a failure fails.
static_assert(monoid<lifted_result>.combine(
                  lift(2), lifted_result{result<sum<int>>{failure}}) ==
              lifted_result{result<sum<int>>{failure}});

// --- Neither induced instance claims a raw carrier. -----------------------

static_assert(!smd::kit::foundation::has_instance_v<decltype(monoid<int>)>);
static_assert(
    !smd::kit::foundation::has_instance_v<decltype(monoid<result<int>>)>);

TEST_CASE("InducedMonoidTest - BothInducedMonoidsAtRuntime") {
    CHECK(monoid<endo>.combine(arrow(halve), arrow(increment))(8) ==
          result<int>{5});
    CHECK(monoid<lifted_result>.combine(lift(2), lift(3)) == lift(5));
}
