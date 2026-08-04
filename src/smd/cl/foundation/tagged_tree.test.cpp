// src/smd/cl/foundation/tagged_tree.test.cpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/foundation/tagged_tree.hpp>
#include <smd/cl/foundation/tagged_tree.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>

using smd::cl::foundation::tagged_tree;

namespace {

using tree = tagged_tree<int, char, 8, 4>;

// The tree for (L 1 2 (Q 3)): children before parents, left to right.
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

constexpr auto builds_and_reads_back() -> bool {
    auto const t = sample_tree();
    return t.size() == 5 && t.root() == 4 && !t.is_leaf(t.root()) &&
           t.branch(t.root()).tag == 'L' &&
           t.branch(t.root()).children.size() == 3 && t.is_leaf(0) &&
           t.leaf(0) == 1 && t.leaf(1) == 2 && t.branch(3).tag == 'Q' &&
           t.leaf(t.branch(3).children[0]) == 3;
}

constexpr auto default_tree_is_empty_and_rootless() -> bool {
    tree const t;
    return t.size() == 0 && t.capacity() == 8 && t.max_children() == 4 &&
           t.root() == -1;
}

constexpr auto equality_is_structural() -> bool {
    return sample_tree() == sample_tree() && !(sample_tree() == tree{});
}

constexpr auto nodes_are_the_index_order() -> bool {
    auto const t = sample_tree();
    auto const by_index =
        std::views::iota(0, t.size()) |
        std::views::transform(
            [&t](int i) -> tree::node_type const & { return t.node(i); });
    return std::ranges::size(t.nodes()) == static_cast<std::size_t>(t.size()) &&
           std::ranges::equal(t.nodes(), by_index);
}

constexpr auto leaves_are_the_leaf_payloads_in_order() -> bool {
    // Branches are structure, not elements: (L 1 2 (Q 3)) has three.
    auto const t = sample_tree();
    return std::ranges::equal(t.leaves(), std::array{1, 2, 3});
}

constexpr auto leaves_of_a_branchless_tree() -> bool {
    tree t;
    t.set_root(t.add_leaf(42));
    return std::ranges::equal(t.leaves(), std::array{42});
}

constexpr auto leaves_of_a_leafless_tree_is_empty() -> bool {
    tree t;
    t.set_root(t.add_branch('L', tree::child_list{}));
    return std::ranges::empty(t.leaves()) && t.size() == 1;
}

constexpr auto from_nodes_round_trips() -> bool {
    auto const t = sample_tree();
    return tree::from_nodes(t.nodes(), t.root()) == t;
}

constexpr auto from_nodes_without_a_root() -> bool {
    auto const t = sample_tree();
    auto const rootless = tree::from_nodes(t.nodes(), -1);
    return rootless.root() == -1 && rootless.size() == t.size();
}

constexpr auto from_nodes_of_nothing_is_the_default_tree() -> bool {
    return tree::from_nodes(tree{}.nodes(), -1) == tree{};
}

} // namespace

static_assert(builds_and_reads_back());
static_assert(default_tree_is_empty_and_rootless());
static_assert(equality_is_structural());
static_assert(nodes_are_the_index_order());
static_assert(leaves_are_the_leaf_payloads_in_order());
static_assert(leaves_of_a_branchless_tree());
static_assert(leaves_of_a_leafless_tree_is_empty());
static_assert(from_nodes_round_trips());
static_assert(from_nodes_without_a_root());
static_assert(from_nodes_of_nothing_is_the_default_tree());

TEST_CASE("TaggedTreeTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("TaggedTreeTest - BuildsAndReadsBack") {
    CHECK(builds_and_reads_back());
}

TEST_CASE("TaggedTreeTest - DefaultTreeIsEmptyAndRootless") {
    CHECK(default_tree_is_empty_and_rootless());
}

TEST_CASE("TaggedTreeTest - EqualityIsStructural") {
    CHECK(equality_is_structural());
}

TEST_CASE("TaggedTreeTest - TreesAreSelfContainedValues") {
    // Copying a tree copies its nodes: mutating the copy leaves the
    // original alone, so no external arena has to outlive anything.
    auto const original = sample_tree();
    auto copy = original;
    copy.set_root(0);
    CHECK(original.root() == 4);
    CHECK(copy.root() == 0);
    CHECK(!(copy == original));
}

TEST_CASE("TaggedTreeTest - NodesAreTheIndexOrder") {
    CHECK(nodes_are_the_index_order());
}

TEST_CASE("TaggedTreeTest - LeavesAreTheLeafPayloadsInOrder") {
    CHECK(leaves_are_the_leaf_payloads_in_order());
    CHECK(leaves_of_a_branchless_tree());
    CHECK(leaves_of_a_leafless_tree_is_empty());
}

TEST_CASE("TaggedTreeTest - FromNodesRoundTrips") {
    CHECK(from_nodes_round_trips());
    CHECK(from_nodes_without_a_root());
    CHECK(from_nodes_of_nothing_is_the_default_tree());
}
