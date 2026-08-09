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

// The tree for (L 1 2 (Q 3)) again: leaves 0,1,2, branch 3 = (Q 3),
// branch 4 = the root. So 0 and 1 are the root's children 0 and 1, the
// quote branch is its child 2, and leaf 2 is the quote branch's child 0.
constexpr auto links_are_the_transpose_of_the_child_lists() -> bool {
    auto const t = sample_tree();
    using link = smd::cl::foundation::tree_link;
    return t.link(0) == link{4, 0} && t.link(1) == link{4, 1} &&
           t.link(2) == link{3, 0} && t.link(3) == link{4, 2} &&
           t.link(4) == link{-1, -1};
}

// Every edge appears in both directions and agrees with itself.
constexpr auto links_agree_with_the_child_lists() -> bool {
    auto const t = sample_tree();
    return std::ranges::all_of(std::views::iota(0, t.size()), [&t](int parent) {
        if (t.is_leaf(parent)) {
            return true;
        }
        auto const &children = t.branch(parent).children;
        return std::ranges::all_of(
            std::views::iota(0, children.size()), [&](int ordinal) {
                auto const l = t.link(children[ordinal]);
                return l.parent == parent && l.ordinal == ordinal;
            });
    });
}

constexpr auto a_node_no_branch_names_has_no_link() -> bool {
    tree t;
    t.add_leaf(7); // built, never referenced
    t.set_root(t.add_leaf(8));
    return t.link(0) == smd::cl::foundation::tree_link{} &&
           t.link(1) == smd::cl::foundation::tree_link{};
}

// from_nodes rebuilds the links, so a rebuilt tree equals the original —
// which is also what keeps fmap and traverse, both of which assemble
// through from_nodes, from producing a tree with a stale link column.
constexpr auto from_nodes_rebuilds_links() -> bool {
    auto const t = sample_tree();
    auto const rebuilt = tree::from_nodes(t.nodes(), t.root());
    return std::ranges::equal(rebuilt.links(), t.links());
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

// I1: every tree a builder can produce satisfies the invariant the
// recursion schemes read children through, and so does everything
// from_nodes will accept. The predicate exists so that from_nodes' third
// precondition is checkable by a caller assembling a node sequence rather
// than mapping one, which is the only way a forward reference can arise —
// add_branch can not name a node that does not exist yet.
constexpr auto built_trees_satisfy_i1() -> bool {
    return sample_tree().is_children_before_parent() &&
           tree{}.is_children_before_parent() &&
           tree::from_nodes(sample_tree().nodes(), sample_tree().root())
               .is_children_before_parent();
}

// A hand-assembled sequence is the case the predicate is for: this one
// satisfies I1, so from_nodes accepts it. The negative case — a branch
// naming a higher index — is a precondition violation and therefore
// untestable by design; it is asserted, not diagnosed.
constexpr auto a_synthesised_sequence_can_be_checked_before_use() -> bool {
    std::array<tree::node_type, 3> nodes{};
    nodes[0] = tree::node_type{7};
    nodes[1] = tree::node_type{8};
    tree::child_list children;
    children.push_back(0);
    children.push_back(1);
    nodes[2] = tree::node_type{tree::branch_type{'L', children}};
    auto const built = tree::from_nodes(nodes, 2);
    return built.is_children_before_parent() && built.size() == 3 &&
           built.link(0) == smd::cl::foundation::tree_link{2, 0} &&
           built.link(1) == smd::cl::foundation::tree_link{2, 1};
}

// I1 is weaker than I2, and this is the witness: two trees of the same
// shape — a branch over leaves 4 then 9, in that order — laid out in two
// different topological orders. Both satisfy I1, and they are different
// tagged_tree values, which is why the Foldable laws survive an element
// order that depends on layout. What the two layouts then do to a fold is
// pinned next door, in tagged_tree_schemes.test.cpp's
// cata_and_fold_map_disagree_on_permuted_trees; this one is about the
// invariant rather than its consequence.
constexpr auto source_order_layout() -> tree { // 4 at 0, 9 at 1
    tree t;
    int const first = t.add_leaf(4);
    int const second = t.add_leaf(9);
    tree::child_list children;
    children.push_back(first);
    children.push_back(second);
    t.set_root(t.add_branch('L', children));
    return t;
}

constexpr auto other_layout() -> tree { // 9 at 0, 4 at 1; same child order
    tree t;
    int const second = t.add_leaf(9);
    int const first = t.add_leaf(4);
    tree::child_list children;
    children.push_back(first);
    children.push_back(second);
    t.set_root(t.add_branch('L', children));
    return t;
}

constexpr auto i1_admits_more_than_one_layout_of_a_shape() -> bool {
    auto const source = source_order_layout();
    auto const other = other_layout();
    return source.is_children_before_parent() &&
           other.is_children_before_parent() &&
           // the same shape: same tag, same ordered children
           source.branch(source.root()).tag == other.branch(other.root()).tag &&
           source.leaf(source.branch(source.root()).children[0]) ==
               other.leaf(other.branch(other.root()).children[0]) &&
           source.leaf(source.branch(source.root()).children[1]) ==
               other.leaf(other.branch(other.root()).children[1]) &&
           // and not the same value, because layout is part of the value
           !(source == other) &&
           // which is exactly the difference between I1 and I2
           std::ranges::equal(source.leaves(), std::array{4, 9}) &&
           std::ranges::equal(other.leaves(), std::array{9, 4});
}

} // namespace

static_assert(builds_and_reads_back());
static_assert(default_tree_is_empty_and_rootless());
static_assert(equality_is_structural());
static_assert(nodes_are_the_index_order());
static_assert(leaves_are_the_leaf_payloads_in_order());
static_assert(leaves_of_a_branchless_tree());
static_assert(leaves_of_a_leafless_tree_is_empty());
static_assert(links_are_the_transpose_of_the_child_lists());
static_assert(links_agree_with_the_child_lists());
static_assert(a_node_no_branch_names_has_no_link());
static_assert(from_nodes_rebuilds_links());
static_assert(from_nodes_round_trips());
static_assert(from_nodes_without_a_root());
static_assert(from_nodes_of_nothing_is_the_default_tree());
static_assert(built_trees_satisfy_i1());
static_assert(a_synthesised_sequence_can_be_checked_before_use());
static_assert(i1_admits_more_than_one_layout_of_a_shape());

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
    CHECK(a_synthesised_sequence_can_be_checked_before_use());
}

TEST_CASE("TaggedTreeTest - ChildrenBeforeParentIsCheckable") {
    CHECK(built_trees_satisfy_i1());
    CHECK(i1_admits_more_than_one_layout_of_a_shape());
}

TEST_CASE("TaggedTreeTest - LinksAreTheTransposeOfTheChildLists") {
    CHECK(links_are_the_transpose_of_the_child_lists());
    CHECK(links_agree_with_the_child_lists());
    CHECK(a_node_no_branch_names_has_no_link());
    CHECK(from_nodes_rebuilds_links());
}
