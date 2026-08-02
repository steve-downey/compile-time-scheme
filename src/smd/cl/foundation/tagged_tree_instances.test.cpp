// src/smd/cl/foundation/tagged_tree_instances.test.cpp           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/foundation/tagged_tree_instances.hpp>
#include <smd/cl/foundation/tagged_tree_instances.hpp> // test 2nd include OK

#include <smd/cl/foundation/identity.hpp>
#include <smd/cl/foundation/result_instances.hpp>
#include <smd/cl/foundation/static_vector.hpp>

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::fmap;
using smd::cl::foundation::fold_left;
using smd::cl::foundation::fold_map;
using smd::cl::foundation::fold_right;
using smd::cl::foundation::identity;
using smd::cl::foundation::length;
using smd::cl::foundation::parse_error;
using smd::cl::foundation::result;
using smd::cl::foundation::source_pos;
using smd::cl::foundation::static_vector;
using smd::cl::foundation::sum_monoid;
using smd::cl::foundation::tagged_tree;
using smd::cl::foundation::traverse;

namespace {

using tree = tagged_tree<int, char, 8, 4>;

// The tree for (L 1 2 (Q 3)): children before parents, left to right, so
// ascending node-index order visits the leaves 1, 2, 3 in source order.
constexpr auto sample_tree() -> tree {
    tree t;
    int const one = t.add_leaf(1);
    int const two = t.add_leaf(2);
    int const three = t.add_leaf(3);
    tree::child_list inner;
    inner.push_back(three);
    int const quoted = t.add_branch('Q', inner);
    tree::child_list outer;
    outer.push_back(one);
    outer.push_back(two);
    outer.push_back(quoted);
    t.set_root(t.add_branch('L', outer));
    return t;
}

constexpr auto add_one = [](int x) { return x + 1; };
constexpr auto times_two = [](int x) { return x * 2; };

constexpr parse_error boom{source_pos{}, "boom"};

// Functor law: identity. fmap(id) == id.
constexpr auto functor_identity_law() -> bool {
    return fmap([](int x) { return x; }, sample_tree()) == sample_tree();
}

// Functor law: composition. fmap(g . f) == fmap(g) . fmap(f).
constexpr auto functor_composition_law() -> bool {
    auto composed =
        fmap([](int x) { return times_two(add_one(x)); }, sample_tree());
    auto chained = fmap(times_two, fmap(add_one, sample_tree()));
    return composed == chained;
}

// Functor contract: structure — node count, branch tags, children, root —
// is preserved; only leaf payloads change.
constexpr auto fmap_preserves_structure() -> bool {
    auto const t = sample_tree();
    auto const mapped = fmap(add_one, t);
    return mapped.size() == t.size() && mapped.root() == t.root() &&
           mapped.branch(mapped.root()).tag == 'L' &&
           mapped.branch(mapped.root()).children ==
               t.branch(t.root()).children &&
           mapped.branch(3).tag == 'Q' && mapped.leaf(0) == 2 &&
           mapped.leaf(1) == 3 && mapped.leaf(2) == 4;
}

// Foldable contract: leaves fold in ascending node-index order (source
// order for a children-first builder), observable through a
// non-commutative fold; branches contribute no elements.
constexpr auto fold_left_is_leaf_source_order() -> bool {
    return fold_left([](int acc, int x) { return acc * 10 + x; }, 0,
                     sample_tree()) == 123;
}

constexpr auto fold_right_is_reverse_leaf_order() -> bool {
    return fold_right([](int x, int acc) { return acc * 10 + x; }, 0,
                      sample_tree()) == 321;
}

constexpr auto fold_map_and_length_agree() -> bool {
    return fold_map([](int x) { return x; }, sample_tree(), sum_monoid) == 6 &&
           length(sample_tree()) == 3 && length(tree{}) == 0;
}

// Traversable law: identity. Traversing with the identity effect is fmap.
constexpr auto traverse_identity_law() -> bool {
    auto traversed = traverse([](int x) { return identity<int>{add_one(x)}; },
                              sample_tree());
    return traversed == identity<tree>{fmap(add_one, sample_tree())};
}

// Traversable contract: shape is preserved on success.
constexpr auto traverse_preserves_shape() -> bool {
    auto traversed =
        traverse([](int x) { return result<int>{x * 2}; }, sample_tree());
    return traversed.has_value() &&
           traversed.value() == fmap(times_two, sample_tree());
}

// Traversable contract with result: the leftmost failing leaf's error is
// the traversal's error.
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
        sample_tree());
    return !traversed.has_value() && traversed.error() == boom;
}

} // namespace

static_assert(functor_identity_law());
static_assert(functor_composition_law());
static_assert(fmap_preserves_structure());
static_assert(fold_left_is_leaf_source_order());
static_assert(fold_right_is_reverse_leaf_order());
static_assert(fold_map_and_length_agree());
static_assert(traverse_identity_law());
static_assert(traverse_preserves_shape());
static_assert(traverse_leftmost_error());

TEST_CASE("TaggedTreeInstancesTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("TaggedTreeInstancesTest - FunctorLaws") {
    CHECK(functor_identity_law());
    CHECK(functor_composition_law());
    CHECK(fmap_preserves_structure());
}

TEST_CASE("TaggedTreeInstancesTest - FoldableOrderAndDerivations") {
    CHECK(fold_left_is_leaf_source_order());
    CHECK(fold_right_is_reverse_leaf_order());
    CHECK(fold_map_and_length_agree());
}

TEST_CASE("TaggedTreeInstancesTest - TraversableLaws") {
    CHECK(traverse_identity_law());
    CHECK(traverse_preserves_shape());
    CHECK(traverse_leftmost_error());
}

TEST_CASE("TaggedTreeInstancesTest - EffectOrderIsLeafSourceOrder") {
    // Effect order contract: f is applied to the leaves in ascending
    // node-index order, and every leaf is visited even after a failure —
    // the applicative combines eagerly.
    static_vector<int, 4> seen;
    auto traversed = traverse(
        [&seen](int x) {
            seen.push_back(x);
            return x == 1 ? result<int>{boom} : result<int>{x};
        },
        sample_tree());
    CHECK(!traversed.has_value());
    REQUIRE(seen.size() == 3);
    CHECK(seen[0] == 1);
    CHECK(seen[1] == 2);
    CHECK(seen[2] == 3);
}

TEST_CASE("TaggedTreeInstancesTest - EmptyTree") {
    constexpr tree empty_tree{};
    CHECK(fmap(add_one, empty_tree) == empty_tree);
    CHECK(length(empty_tree) == 0);
    auto traversed = traverse([](int x) { return result<int>{x}; }, empty_tree);
    REQUIRE(traversed.has_value());
    CHECK(traversed.value() == empty_tree);
}
