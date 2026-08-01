// src/smd/cl/foundation/identity.test.cpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/foundation/identity.hpp>
#include <smd/cl/foundation/identity.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::fmap;
using smd::cl::foundation::identity;
using smd::cl::foundation::identity_applicative_map;
using smd::cl::foundation::invoke;

TEST_CASE("IdentityTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

constexpr auto add_one = [](int x) { return x + 1; };
constexpr auto times_two = [](int x) { return x * 2; };

// Functor law: identity. fmap(id) == id.
constexpr auto functor_identity_law() -> bool {
    return fmap([](int x) { return x; }, identity<int>{5}) == identity<int>{5};
}

// Functor law: composition. fmap(g . f) == fmap(g) . fmap(f).
constexpr auto functor_composition_law() -> bool {
    constexpr identity<int> v{3};
    auto composed = fmap([](int x) { return times_two(add_one(x)); }, v);
    auto chained = fmap(times_two, fmap(add_one, v));
    return composed == chained;
}

// Applicative law: identity. apply(pure(id), v) == v.
constexpr auto applicative_identity_law() -> bool {
    identity_applicative_map m{};
    constexpr identity<int> v{9};
    return m.apply(m.pure([](int x) { return x; }), v) == v;
}

// Applicative law: homomorphism. apply(pure(f), pure(x)) == pure(f(x)).
constexpr auto applicative_homomorphism_law() -> bool {
    identity_applicative_map m{};
    return m.apply(m.pure(add_one), m.pure(4)) == m.pure(add_one(4));
}

// Applicative law: interchange. apply(u, pure(y)) == apply(pure($y), u).
constexpr auto applicative_interchange_law() -> bool {
    identity_applicative_map m{};
    auto u = m.pure(add_one);
    int const y = 7;
    auto applied = m.apply(u, m.pure(y));
    auto interchanged = m.apply(m.pure([y](auto const &f) { return f(y); }), u);
    return applied == interchanged;
}

// Applicative law: composition.
// apply(apply(apply(pure(compose), u), v), w) == apply(u, apply(v, w)).
constexpr auto applicative_composition_law() -> bool {
    identity_applicative_map m{};
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

} // namespace

static_assert(functor_identity_law());
static_assert(functor_composition_law());
static_assert(applicative_identity_law());
static_assert(applicative_homomorphism_law());
static_assert(applicative_interchange_law());
static_assert(applicative_composition_law());

TEST_CASE("IdentityTest - FunctorLaws") {
    CHECK(functor_identity_law());
    CHECK(functor_composition_law());
}

TEST_CASE("IdentityTest - ApplicativeLaws") {
    CHECK(applicative_identity_law());
    CHECK(applicative_homomorphism_law());
    CHECK(applicative_interchange_law());
    CHECK(applicative_composition_law());
}

TEST_CASE("IdentityTest - FmapCpo") {
    CHECK(fmap(add_one, identity<int>{1}) == identity<int>{2});
}

TEST_CASE("IdentityTest - InvokeCpo") {
    auto summed = invoke([](int x, int y) { return x + y; }, identity<int>{3},
                         identity<int>{4});
    CHECK(summed == identity<int>{7});
}
