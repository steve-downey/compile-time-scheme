// src/smd/cl/foundation/monad.test.cpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/foundation/monad.hpp>
#include <smd/cl/foundation/monad.hpp> // test 2nd include OK

#include <smd/cl/foundation/applicative.hpp>
#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/result_instances.hpp>

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::bind;
using smd::cl::foundation::invoke;
using smd::cl::foundation::join;
using smd::cl::foundation::parse_error;
using smd::cl::foundation::result;
using smd::cl::foundation::result_monad_map;
using smd::cl::foundation::source_pos;

TEST_CASE("MonadTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

constexpr auto failure = parse_error{source_pos{}, "boom"};
constexpr auto other_failure = parse_error{source_pos{}, "second"};

constexpr auto halve(int n) -> result<int> {
    if (n % 2 != 0) {
        return failure;
    }
    return n / 2;
}

constexpr auto add = [](int a, int b) { return a + b; };

} // namespace

// --- The three monad laws, on the registered result instance. -------------

// Left identity: pure(a) >>= f == f(a)
static_assert(bind(result<int>{8}, halve).value() == halve(8).value());
static_assert(!bind(result<int>{7}, halve).has_value());

// Right identity: m >>= pure == m
static_assert(
    bind(result<int>{5}, [](int v) { return result<int>{v}; }).value() == 5);
static_assert(
    !bind(result<int>{failure}, [](int v) { return result<int>{v}; })
         .has_value());

// Associativity: (m >>= f) >>= g == m >>= (\x -> f x >>= g)
constexpr auto left_assoc = bind(bind(result<int>{8}, halve), halve);
constexpr auto right_assoc =
    bind(result<int>{8}, [](int v) { return bind(halve(v), halve); });
static_assert(left_assoc.value() == right_assoc.value());
static_assert(left_assoc.value() == 2);

// --- bind does not invoke its function on a failed value. ----------------

constexpr auto invoked_after_failure() -> bool {
    bool invoked = false;
    auto step = bind(result<int>{failure}, [&invoked](int v) {
        invoked = true;
        return result<int>{v};
    });
    return invoked || step.has_value();
}
static_assert(!invoked_after_failure());

// An error passes through unchanged.
static_assert(bind(result<int>{failure}, halve).error() == failure);

// --- Derived operations. -------------------------------------------------

static_assert(join(result<result<int>>{result<int>{3}}).value() == 3);
static_assert(!join(result<result<int>>{failure}).has_value());
static_assert(!join(result<result<int>>{result<int>{failure}}).has_value());

static_assert(
    result_monad_map{}.then(result<int>{1}, result<int>{2}).value() == 2);
static_assert(
    !result_monad_map{}.then(result<int>{failure}, result<int>{2}).has_value());

// --- The monad-derived apply agrees with what it replaced. ---------------
//
// result's Applicative apply is now nested binds (result_instances.hpp).
// These pin the semantics the hand-written version had: leftmost error
// wins, and the function runs only if both operands succeed.

static_assert(invoke(add, result<int>{2}, result<int>{3}).value() == 5);
static_assert(!invoke(add, result<int>{failure}, result<int>{3}).has_value());
static_assert(!invoke(add, result<int>{2}, result<int>{failure}).has_value());

// Leftmost error wins when both operands failed.
static_assert(
    invoke(add, result<int>{failure}, result<int>{other_failure}).error() ==
    failure);

TEST_CASE("MonadTest - LawsHoldAtRuntimeToo") {
    CHECK(bind(result<int>{8}, halve).value() == 4);
    CHECK(!bind(result<int>{7}, halve).has_value());
    CHECK(join(result<result<int>>{result<int>{3}}).value() == 3);
    CHECK(invoke(add, result<int>{2}, result<int>{3}).value() == 5);
}
