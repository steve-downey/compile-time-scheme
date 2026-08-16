// src/smd/kit/foundation/result_instances.test.cpp                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Moved in step R8 (decision 2, corrected) from
// src/smd/cl/foundation/result_instances.test.cpp.

#include <smd/kit/foundation/result_instances.hpp>
#include <smd/kit/foundation/result_instances.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::kit::foundation::fmap;
using smd::kit::foundation::invoke;
using smd::kit::foundation::parse_error;
using smd::kit::foundation::result;
using smd::kit::foundation::result_applicative_map;
using smd::kit::foundation::source_pos;

TEST_CASE("ResultInstancesTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

constexpr auto add_one = [](int x) { return x + 1; };
constexpr auto times_two = [](int x) { return x * 2; };

constexpr parse_error first_error{source_pos{1, 1, 2}, "first"};
constexpr parse_error second_error{source_pos{3, 1, 4}, "second"};

// Functor law: identity. fmap(id) == id, on both alternatives.
constexpr auto functor_identity_law() -> bool {
    auto id = [](int x) { return x; };
    return fmap(id, result<int>{5}) == result<int>{5} &&
           fmap(id, result<int>{first_error}) == result<int>{first_error};
}

// Functor law: composition. fmap(g . f) == fmap(g) . fmap(f).
constexpr auto functor_composition_law() -> bool {
    auto composed_of = [](result<int> const &r) {
        return fmap([](int x) { return times_two(add_one(x)); }, r) ==
               fmap(times_two, fmap(add_one, r));
    };
    return composed_of(result<int>{3}) && composed_of(result<int>{first_error});
}

// Applicative law: identity. apply(pure(id), v) == v.
constexpr auto applicative_identity_law() -> bool {
    result_applicative_map m{};
    auto identity_on = [&](result<int> const &v) {
        return m.apply(m.pure([](int x) { return x; }), v) == v;
    };
    return identity_on(result<int>{9}) && identity_on(result<int>{first_error});
}

// Applicative law: homomorphism. apply(pure(f), pure(x)) == pure(f(x)).
constexpr auto applicative_homomorphism_law() -> bool {
    result_applicative_map m{};
    return m.apply(m.pure(add_one), m.pure(4)) == m.pure(add_one(4));
}

// Applicative law: interchange. apply(u, pure(y)) == apply(pure($y), u).
constexpr auto applicative_interchange_law() -> bool {
    result_applicative_map m{};
    auto u = m.pure(add_one);
    int const y = 7;
    auto applied = m.apply(u, m.pure(y));
    auto interchanged = m.apply(m.pure([y](auto const &f) { return f(y); }), u);
    return applied == interchanged;
}

// Applicative law: composition.
// apply(apply(apply(pure(compose), u), v), w) == apply(u, apply(v, w)).
constexpr auto applicative_composition_law() -> bool {
    result_applicative_map m{};
    auto compose = [](auto f) {
        return [f](auto g) { return [f, g](int x) { return f(g(x)); }; };
    };
    auto u = m.pure(add_one);
    auto v = m.pure(times_two);
    auto w = m.pure(5);
    auto lhs = m.apply(m.apply(m.apply(m.pure(compose), u), v), w);
    auto rhs = m.apply(u, m.apply(v, w));
    return lhs == rhs;
}

// Contract: apply's leftmost error wins.
constexpr auto leftmost_error_wins() -> bool {
    result_applicative_map m{};
    result<int (*)(int)> failed_function{first_error};
    result<int> failed_argument{second_error};
    auto both_failed = m.apply(failed_function, failed_argument);
    auto function_failed = m.apply(failed_function, result<int>{1});
    auto argument_failed =
        m.apply(result<int (*)(int)>{add_one}, failed_argument);
    return both_failed == result<int>{first_error} &&
           function_failed == result<int>{first_error} &&
           argument_failed == result<int>{second_error};
}

} // namespace

static_assert(functor_identity_law());
static_assert(functor_composition_law());
static_assert(applicative_identity_law());
static_assert(applicative_homomorphism_law());
static_assert(applicative_interchange_law());
static_assert(applicative_composition_law());
static_assert(leftmost_error_wins());

TEST_CASE("ResultInstancesTest - FunctorLaws") {
    CHECK(functor_identity_law());
    CHECK(functor_composition_law());
}

TEST_CASE("ResultInstancesTest - ApplicativeLaws") {
    CHECK(applicative_identity_law());
    CHECK(applicative_homomorphism_law());
    CHECK(applicative_interchange_law());
    CHECK(applicative_composition_law());
}

TEST_CASE("ResultInstancesTest - LeftmostErrorWins") {
    CHECK(leftmost_error_wins());
}

TEST_CASE("ResultInstancesTest - FmapCpo") {
    CHECK(fmap(add_one, result<int>{1}) == result<int>{2});
    CHECK(fmap(add_one, result<int>{first_error}) == result<int>{first_error});
}

TEST_CASE("ResultInstancesTest - InvokeCpo") {
    auto summed = invoke([](int x, int y) { return x + y; }, result<int>{3},
                         result<int>{4});
    CHECK(summed == result<int>{7});
    auto failed = invoke([](int x, int y) { return x + y; },
                         result<int>{first_error}, result<int>{4});
    CHECK(failed == result<int>{first_error});
}
