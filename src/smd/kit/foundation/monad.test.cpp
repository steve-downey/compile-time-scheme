// src/smd/kit/foundation/monad.test.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/kit/foundation/monad.hpp>
#include <smd/kit/foundation/monad.hpp> // test 2nd include OK

#include <smd/kit/foundation/applicative.hpp>
#include <smd/kit/foundation/identity.hpp>
#include <smd/kit/foundation/parse_error.hpp>
#include <smd/kit/foundation/result.hpp>
#include <smd/kit/foundation/result_instances.hpp>

#include <catch2/catch_test_macros.hpp>

using smd::kit::foundation::bind;
using smd::kit::foundation::identity;
using smd::kit::foundation::identity_monad_map;
using smd::kit::foundation::invoke;
using smd::kit::foundation::join;
using smd::kit::foundation::parse_error;
using smd::kit::foundation::result;
using smd::kit::foundation::result_monad_map;
using smd::kit::foundation::source_pos;

TEST_CASE("MonadTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

constexpr auto failure = parse_error{source_pos{}, "boom"};
constexpr auto other_failure = parse_error{source_pos{}, "second"};

// A step that can fail, for the result instance.
constexpr auto halve(int n) -> result<int> {
    if (n % 2 != 0) {
        return failure;
    }
    return n / 2;
}

// The same shape over an effect that cannot fail, for the identity
// instance. The laws are the same laws; only one of the two instances can
// witness what happens on failure.
constexpr auto negate(int n) -> identity<int> { return identity<int>{-n}; }
constexpr auto increment(int n) -> identity<int> {
    return identity<int>{n + 1};
}

constexpr auto add = [](int a, int b) { return a + b; };

} // namespace

// --- The three laws, on both registered instances. ------------------------
//
// Each compares whole monadic values rather than unwrapped ones: comparing
// `.value()` cannot tell "these agree" from "these both happened to
// succeed".

// Left identity: pure(a) >>= f == f(a)
static_assert(bind(result_monad_map{}.pure(8), halve) == halve(8));
static_assert(bind(result_monad_map{}.pure(7), halve) == halve(7));
static_assert(bind(identity_monad_map{}.pure(8), negate) == negate(8));

// Right identity: m >>= pure == m
namespace {
constexpr auto result_pure = [](int n) { return result_monad_map{}.pure(n); };
constexpr auto identity_pure = [](int n) {
    return identity_monad_map{}.pure(n);
};
} // namespace

static_assert(bind(result<int>{5}, result_pure) == result<int>{5});
static_assert(bind(result<int>{failure}, result_pure) == result<int>{failure});
static_assert(bind(identity<int>{5}, identity_pure) == identity<int>{5});

// Associativity: (m >>= f) >>= g == m >>= (\x -> f x >>= g)
static_assert(bind(bind(result<int>{8}, halve), halve) ==
              bind(result<int>{8},
                   [](int n) { return bind(halve(n), halve); }));
static_assert(bind(bind(result<int>{8}, halve), halve) == result<int>{2});
static_assert(bind(bind(result<int>{12}, halve), halve) ==
              bind(result<int>{12},
                   [](int n) { return bind(halve(n), halve); }));
static_assert(bind(bind(identity<int>{8}, negate), increment) ==
              bind(identity<int>{8},
                   [](int n) { return bind(negate(n), increment); }));

// --- What only a failing instance can witness. ----------------------------

// bind does not invoke its function on a failed value.
constexpr auto invoked_after_failure() -> bool {
    bool invoked = false;
    auto const step = bind(result<int>{failure}, [&invoked](int n) {
        invoked = true;
        return result<int>{n};
    });
    return invoked || step.has_value();
}
static_assert(!invoked_after_failure());

// The error passes through unchanged rather than being rebuilt.
static_assert(bind(result<int>{failure}, halve) == result<int>{failure});

// --- Derived operations. --------------------------------------------------

static_assert(join(result<result<int>>{result<int>{3}}) == result<int>{3});
static_assert(join(result<result<int>>{failure}) == result<int>{failure});
static_assert(join(result<result<int>>{result<int>{failure}}) ==
              result<int>{failure});
static_assert(join(identity<identity<int>>{identity<int>{3}}) ==
              identity<int>{3});

static_assert(result_monad_map{}.then(result<int>{1}, result<int>{2}) ==
              result<int>{2});
static_assert(result_monad_map{}.then(result<int>{failure}, result<int>{2}) ==
              result<int>{failure});
static_assert(identity_monad_map{}.then(identity<int>{1}, identity<int>{2}) ==
              identity<int>{2});

// --- The monad-derived apply agrees with what it replaced. ----------------
//
// result's Applicative apply is now nested binds (result_instances.hpp).
// These pin the semantics the hand-written version had: the function runs
// only if both operands succeeded, and the leftmost error wins.

static_assert(invoke(add, result<int>{2}, result<int>{3}) == result<int>{5});
static_assert(invoke(add, result<int>{failure}, result<int>{3}) ==
              result<int>{failure});
static_assert(invoke(add, result<int>{2}, result<int>{failure}) ==
              result<int>{failure});
static_assert(invoke(add, result<int>{failure}, result<int>{other_failure}) ==
              result<int>{failure});

TEST_CASE("MonadTest - LawsHoldAtRuntimeToo") {
    CHECK(bind(result<int>{8}, halve) == result<int>{4});
    CHECK(bind(result<int>{7}, halve) == result<int>{failure});
    CHECK(bind(identity<int>{8}, negate) == identity<int>{-8});
    CHECK(join(result<result<int>>{result<int>{3}}) == result<int>{3});
    CHECK(invoke(add, result<int>{2}, result<int>{3}) == result<int>{5});
}
