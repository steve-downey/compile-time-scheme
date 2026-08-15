// src/smd/kit/foundation/static_vector_instances.test.cpp           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Moved in step R8 (decision 2, corrected) from
// src/smd/cl/foundation/static_vector_instances.test.cpp.

#include <smd/kit/foundation/static_vector_instances.hpp>
#include <smd/kit/foundation/static_vector_instances.hpp> // test 2nd include OK

#include <smd/kit/foundation/fold_left_short.hpp>
#include <smd/kit/foundation/identity.hpp>
#include <smd/kit/foundation/result_instances.hpp>

#include <catch2/catch_test_macros.hpp>

using smd::kit::foundation::fmap;
using smd::kit::foundation::fold_left;
using smd::kit::foundation::fold_map;
using smd::kit::foundation::fold_right;
using smd::kit::foundation::identity;
using smd::kit::foundation::length;
using smd::kit::foundation::parse_error;
using smd::kit::foundation::result;
using smd::kit::foundation::sequence;
using smd::kit::foundation::source_pos;
using smd::kit::foundation::static_vector;
using smd::kit::foundation::sum_monoid;
using smd::kit::foundation::traverse;

TEST_CASE("StaticVectorInstancesTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

constexpr auto values_1_2_3() -> static_vector<int, 4> {
    static_vector<int, 4> xs;
    xs.push_back(1);
    xs.push_back(2);
    xs.push_back(3);
    return xs;
}

constexpr auto add_one = [](int x) { return x + 1; };
constexpr auto times_two = [](int x) { return x * 2; };

constexpr parse_error boom{source_pos{}, "boom"};

// Functor law: identity. fmap(id) == id.
constexpr auto functor_identity_law() -> bool {
    return fmap([](int x) { return x; }, values_1_2_3()) == values_1_2_3();
}

// Functor law: composition. fmap(g . f) == fmap(g) . fmap(f).
constexpr auto functor_composition_law() -> bool {
    auto composed =
        fmap([](int x) { return times_two(add_one(x)); }, values_1_2_3());
    auto chained = fmap(times_two, fmap(add_one, values_1_2_3()));
    return composed == chained;
}

// Functor contract: shape (length) is preserved.
constexpr auto fmap_preserves_shape() -> bool {
    return fmap(add_one, values_1_2_3()).size() == values_1_2_3().size();
}

// Foldable contract: fold_map combines in index order (observable through
// a non-commutative fold_left derived from it), and the derived operations
// agree with direct computation.
constexpr auto fold_left_is_index_order() -> bool {
    return fold_left([](int acc, int x) { return acc * 10 + x; }, 0,
                     values_1_2_3()) == 123;
}

constexpr auto fold_right_is_reverse_index_order() -> bool {
    return fold_right([](int x, int acc) { return acc * 10 + x; }, 0,
                      values_1_2_3()) == 321;
}

constexpr auto fold_map_and_length_agree() -> bool {
    return fold_map([](int x) { return x; }, values_1_2_3(), sum_monoid) == 6 &&
           length(values_1_2_3()) == 3 && length(static_vector<int, 4>{}) == 0;
}

// Traversable law: identity. Traversing with the identity effect is fmap.
constexpr auto traverse_identity_law() -> bool {
    auto traversed = traverse([](int x) { return identity<int>{add_one(x)}; },
                              values_1_2_3());
    return traversed ==
           identity<static_vector<int, 4>>{fmap(add_one, values_1_2_3())};
}

// Traversable contract: shape is preserved on success.
constexpr auto traverse_preserves_shape() -> bool {
    auto traversed =
        traverse([](int x) { return result<int>{x * 2}; }, values_1_2_3());
    return traversed.has_value() &&
           traversed.value().size() == values_1_2_3().size() &&
           traversed.value()[0] == 2 && traversed.value()[1] == 4 &&
           traversed.value()[2] == 6;
}

// Traversable contract with result: the leftmost failing element's error
// is the traversal's error.
constexpr auto traverse_leftmost_error() -> bool {
    constexpr parse_error later{source_pos{}, "later"};
    auto traversed = traverse(
        [](int x) {
            if (x == 2) {
                return result<int>{boom};
            }
            if (x == 3) {
                return result<int>{later};
            }
            return result<int>{x};
        },
        values_1_2_3());
    return !traversed.has_value() && traversed.error() == boom;
}

// Derived sequence: a vector of effects becomes an effect of a vector.
// The effect here is identity because static_vector's storage requires
// default-constructible elements, which result is not.
constexpr auto sequence_collects() -> bool {
    static_vector<identity<int>, 4> effects;
    effects.push_back(identity<int>{1});
    effects.push_back(identity<int>{2});
    auto collected = sequence(effects);
    static_vector<int, 4> expected;
    expected.push_back(1);
    expected.push_back(2);
    return collected == identity<static_vector<int, 4>>{expected};
}

} // namespace

static_assert(functor_identity_law());
static_assert(functor_composition_law());
static_assert(fmap_preserves_shape());
static_assert(fold_left_is_index_order());
static_assert(fold_right_is_reverse_index_order());
static_assert(fold_map_and_length_agree());
static_assert(traverse_identity_law());
static_assert(traverse_preserves_shape());
static_assert(traverse_leftmost_error());
static_assert(sequence_collects());

TEST_CASE("StaticVectorInstancesTest - FunctorLaws") {
    CHECK(functor_identity_law());
    CHECK(functor_composition_law());
    CHECK(fmap_preserves_shape());
}

TEST_CASE("StaticVectorInstancesTest - FoldableOrderAndDerivations") {
    CHECK(fold_left_is_index_order());
    CHECK(fold_right_is_reverse_index_order());
    CHECK(fold_map_and_length_agree());
}

TEST_CASE("StaticVectorInstancesTest - TraversableLaws") {
    CHECK(traverse_identity_law());
    CHECK(traverse_preserves_shape());
    CHECK(traverse_leftmost_error());
    CHECK(sequence_collects());
}

TEST_CASE("StaticVectorInstancesTest - TraverseVisitsEveryElement") {
    // Effect order contract: traverse visits every element even after a
    // failure — the applicative combines eagerly. Early exit is
    // fold_left_short's job (contrast in fold_left_short.test.cpp).
    int visited = 0;
    auto traversed = traverse(
        [&visited](int x) {
            ++visited;
            return x == 1 ? result<int>{boom} : result<int>{x};
        },
        values_1_2_3());
    CHECK(!traversed.has_value());
    CHECK(visited == 3);
}

TEST_CASE("StaticVectorInstancesTest - EmptyVector") {
    constexpr static_vector<int, 4> empty_vec{};
    CHECK(fmap(add_one, empty_vec) == empty_vec);
    CHECK(length(empty_vec) == 0);
    auto traversed = traverse([](int x) { return result<int>{x}; }, empty_vec);
    REQUIRE(traversed.has_value());
    CHECK(traversed.value() == empty_vec);
}
