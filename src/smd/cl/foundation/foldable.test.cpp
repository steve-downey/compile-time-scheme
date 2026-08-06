// src/smd/cl/foundation/foldable.test.cpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/foundation/foldable.hpp>
#include <smd/cl/foundation/foldable.hpp> // test 2nd include OK

#include <smd/cl/foundation/static_vector.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <utility>

using smd::cl::foundation::all_monoid;
using smd::cl::foundation::fold_left;
using smd::cl::foundation::fold_map;
using smd::cl::foundation::fold_right;
using smd::cl::foundation::foldable;
using smd::cl::foundation::foldable_typeclass;
using smd::cl::foundation::length;
using smd::cl::foundation::static_vector;
using smd::cl::foundation::sum_monoid;

TEST_CASE("FoldableTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

/// A minimal two-element container, local to this test, used to exercise
/// the @ref foldable CRTP base's derivations and the CPOs without pulling
/// in a production instance.
template <class T>
struct pair_box {
    T first;
    T second;

    friend constexpr auto operator==(pair_box const &, pair_box const &)
        -> bool = default;
};

/// Foldable @c Impl for @c pair_box.
/// Traversal order: @c first, then @c second.
struct pair_box_foldable_impl {
    template <class F, class T, class M>
    constexpr auto fold_map(this auto &&, F &&f, pair_box<T> const &pb,
                            M const &m) {
        // Sequenced deliberately. As arguments to one call the two f
        // invocations are unsequenced, and the derived fold_left observes
        // their order — it passes fold_map an f that updates an
        // accumulator. gcc-16 -O0 ran them right to left and fold_left
        // yielded 21 where the documented order says 12, while -O1, clang,
        // and every constant evaluation ran them left to right. Traversal
        // order is part of the instance contract, so it is stated here
        // rather than left to the compiler.
        auto first_image = f(pb.first);
        auto second_image = f(pb.second);
        return m.combine(std::move(first_image), std::move(second_image));
    }

    template <class F, class Acc, class T>
    constexpr auto fold_right(this auto &&, F &&f, Acc init,
                              pair_box<T> const &pb) -> Acc {
        return f(pb.first, f(pb.second, std::move(init)));
    }
};

struct pair_box_foldable_map : foldable<pair_box_foldable_impl> {
    using pair_box_foldable_impl::fold_map;
    using pair_box_foldable_impl::fold_right;
};

} // namespace

namespace smd::cl::foundation {
template <class T>
inline constexpr auto foldable_typeclass<pair_box<T>> = pair_box_foldable_map{};
}

namespace {

constexpr pair_box<int> one_two{1, 2};

// fold_map is the semantic centre: map into a monoid, combine in order.
constexpr auto fold_map_sums() -> bool {
    pair_box_foldable_map m{};
    return m.fold_map([](int x) { return x * 10; }, one_two, sum_monoid) == 30;
}

// Derived fold_left visits first-then-second (order observable through a
// non-commutative accumulator).
constexpr auto fold_left_is_left_to_right() -> bool {
    pair_box_foldable_map m{};
    return m.fold_left([](int acc, int x) { return acc * 10 + x; }, 0,
                       one_two) == 12;
}

// Primitive fold_right combines from the second element back to the first.
constexpr auto fold_right_is_right_to_left() -> bool {
    pair_box_foldable_map m{};
    return m.fold_right([](int x, int acc) { return acc * 10 + x; }, 0,
                        one_two) == 21;
}

// Derived length counts elements.
constexpr auto length_counts() -> bool {
    pair_box_foldable_map m{};
    return m.length(one_two) == 2;
}

// Derived to_vector copies the elements in traversal order.
constexpr auto to_vector_preserves_order() -> bool {
    pair_box_foldable_map m{};
    auto copied = m.to_vector<static_vector<int, 4>>(one_two);
    return copied.size() == 2 && copied[0] == 1 && copied[1] == 2;
}

} // namespace

static_assert(fold_map_sums());
static_assert(fold_left_is_left_to_right());
static_assert(fold_right_is_right_to_left());
static_assert(length_counts());
static_assert(to_vector_preserves_order());

// The CPOs dispatch through the registered typeclass instance.
static_assert(fold_map([](int x) { return x > 0; }, one_two, all_monoid));
static_assert(fold_left([](int acc, int x) { return acc * 10 + x; }, 0,
                        one_two) == 12);
static_assert(fold_right([](int x, int acc) { return acc * 10 + x; }, 0,
                         one_two) == 21);
static_assert(length(one_two) == 2);

TEST_CASE("FoldableTest - FoldMapIsTheCentre") { CHECK(fold_map_sums()); }

TEST_CASE("FoldableTest - DerivedFoldLeft") {
    CHECK(fold_left_is_left_to_right());
}

TEST_CASE("FoldableTest - PrimitiveFoldRight") {
    CHECK(fold_right_is_right_to_left());
}

TEST_CASE("FoldableTest - DerivedLength") { CHECK(length_counts()); }

TEST_CASE("FoldableTest - DerivedToVector") {
    CHECK(to_vector_preserves_order());
}

TEST_CASE("FoldableTest - TypeclassLookup") {
    const auto &tc = foldable_typeclass<pair_box<int>>;
    static_assert(
        !std::is_same_v<std::remove_cvref_t<decltype(tc)>, std::false_type>);
    CHECK(tc.length(one_two) == 2);
}

TEST_CASE("FoldableTest - Cpos") {
    CHECK(fold_map([](int x) { return x; }, one_two, sum_monoid) == 3);
    CHECK(fold_left([](int acc, int x) { return acc + x; }, 0, one_two) == 3);
    CHECK(fold_right([](int x, int acc) { return acc + x; }, 0, one_two) == 3);
    CHECK(length(one_two) == 2);
}
