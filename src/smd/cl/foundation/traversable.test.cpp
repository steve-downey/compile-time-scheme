// src/smd/cl/foundation/traversable.test.cpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/foundation/traversable.hpp>
#include <smd/cl/foundation/traversable.hpp> // test 2nd include OK

#include <smd/cl/foundation/applicative.hpp>
#include <smd/cl/foundation/identity.hpp>
#include <smd/cl/foundation/result_instances.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <utility>

using smd::cl::foundation::applicative_typeclass;
using smd::cl::foundation::identity;
using smd::cl::foundation::parse_error;
using smd::cl::foundation::result;
using smd::cl::foundation::sequence;
using smd::cl::foundation::source_pos;
using smd::cl::foundation::traversable;
using smd::cl::foundation::traversable_typeclass;
using smd::cl::foundation::traverse;

TEST_CASE("TraversableTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

/// A minimal two-element container, local to this test, used to exercise
/// the @ref traversable CRTP base and CPOs without pulling in a production
/// instance.
template <class T>
struct pair_box {
    T first;
    T second;

    friend constexpr auto operator==(pair_box const &, pair_box const &)
        -> bool = default;
};

/// Traversable @c Impl for @c pair_box.
/// Traversal order: @c first, then @c second; effects are sequenced in that
/// order through the effect's registered Applicative instance.
struct pair_box_traversable_impl {
    template <class F, class T>
    constexpr auto traverse(this auto &&, F &&f, pair_box<T> const &pb) {
        using effect_type =
            std::remove_cvref_t<std::invoke_result_t<F &, T const &>>;
        using B = typename effect_type::value_type;
        auto const &tc = applicative_typeclass<effect_type>;
        return tc.invoke(
            [](B a, B b) { return pair_box<B>{std::move(a), std::move(b)}; },
            f(pb.first), f(pb.second));
    }
};

struct pair_box_traversable_map : traversable<pair_box_traversable_impl> {
    using pair_box_traversable_impl::traverse;
};

} // namespace

namespace smd::cl::foundation {
template <class T>
inline constexpr auto traversable_typeclass<pair_box<T>> =
    pair_box_traversable_map{};
}

namespace {

constexpr pair_box<int> one_two{1, 2};
constexpr parse_error boom{source_pos{}, "boom"};

// Shape preservation with the identity effect: traverse rebuilds the same
// shape with mapped values and performs no effect.
constexpr auto traverse_identity_effect() -> bool {
    auto traversed =
        traverse([](int x) { return identity<int>{x * 2}; }, one_two);
    return traversed == identity<pair_box<int>>{pair_box<int>{2, 4}};
}

// Traversing with the result effect succeeds when every element does.
constexpr auto traverse_result_success() -> bool {
    auto traversed =
        traverse([](int x) { return result<int>{x + 10}; }, one_two);
    return traversed.has_value() && traversed.value() == pair_box<int>{11, 12};
}

// A failing element fails the whole traversal.
constexpr auto traverse_result_failure() -> bool {
    auto traversed = traverse(
        [](int x) { return x % 2 == 0 ? result<int>{x} : result<int>{boom}; },
        one_two);
    return !traversed.has_value() && traversed.error() == boom;
}

// Derived sequence: a container of effects becomes an effect of a container.
constexpr auto sequence_collects() -> bool {
    pair_box<identity<int>> effects{identity<int>{1}, identity<int>{2}};
    return sequence(effects) == identity<pair_box<int>>{pair_box<int>{1, 2}};
}

} // namespace

static_assert(traverse_identity_effect());
static_assert(traverse_result_success());
static_assert(traverse_result_failure());
static_assert(sequence_collects());

TEST_CASE("TraversableTest - IdentityEffect") {
    CHECK(traverse_identity_effect());
}

TEST_CASE("TraversableTest - ResultSuccess") {
    CHECK(traverse_result_success());
}

TEST_CASE("TraversableTest - ResultFailure") {
    CHECK(traverse_result_failure());
}

TEST_CASE("TraversableTest - Sequence") { CHECK(sequence_collects()); }

TEST_CASE("TraversableTest - TypeclassLookup") {
    const auto &tc = traversable_typeclass<pair_box<int>>;
    static_assert(
        !std::is_same_v<std::remove_cvref_t<decltype(tc)>, std::false_type>);
    auto traversed =
        tc.traverse([](int x) { return identity<int>{x}; }, one_two);
    CHECK(traversed == identity<pair_box<int>>{one_two});
}
