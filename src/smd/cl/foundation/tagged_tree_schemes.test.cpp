// src/smd/cl/foundation/tagged_tree_schemes.test.cpp             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/foundation/tagged_tree_schemes.hpp>
#include <smd/cl/foundation/tagged_tree_schemes.hpp> // test 2nd include OK

#include <smd/cl/foundation/foldable.hpp>
#include <smd/cl/foundation/monoid.hpp>
#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/tagged_tree_instances.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <ranges>
#include <variant>

using smd::cl::foundation::branch_f;
using smd::cl::foundation::cata;
using smd::cl::foundation::cata_short;
using smd::cl::foundation::fold_left;
using smd::cl::foundation::node_f;
using smd::cl::foundation::para;
using smd::cl::foundation::para_short;
using smd::cl::foundation::parse_error;
using smd::cl::foundation::result;
using smd::cl::foundation::scan_down;
using smd::cl::foundation::source_pos;
using smd::cl::foundation::static_vector;
using smd::cl::foundation::tagged_tree;

namespace {

using tree = tagged_tree<int, char, 8, 4>;
using layer_of_int = node_f<int, char, int, 4>;

// The tree for (L 1 2 (Q 3)) again: leaves 0,1,2, branch 3 = (Q 3),
// branch 4 = the root.
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

// The summing algebra: a leaf is its value, a branch is the sum of its
// children's results. std::visit dispatches one layer; cata drives.
constexpr auto sum_alg = [](layer_of_int const &layer) -> int {
    return std::visit(
        [](auto const &l) -> int {
            if constexpr (requires { l.children; }) {
                return std::ranges::fold_left(l.children, 0, std::plus{});
            } else {
                return l;
            }
        },
        layer);
};

constexpr auto cata_sums_the_leaves() -> bool {
    return cata<int>(sample_tree(), sum_alg) == 6;
}

// Height: a leaf is 0, a branch one more than its tallest child.
constexpr auto cata_computes_height() -> bool {
    auto const height = cata<int>(sample_tree(), [](layer_of_int const &layer) {
        return std::visit(
            [](auto const &l) -> int {
                if constexpr (requires { l.children; }) {
                    return std::ranges::fold_left(
                               l.children, -1,
                               [](int a, int b) { return std::max(a, b); }) +
                           1;
                } else {
                    return 0;
                }
            },
            layer);
    });
    return height == 2;
}

// The reflection law: the rebuilding algebra reproduces the tree — the
// witness that the knot is tied. Each node's carrier is its index in
// the copy; because cata appends in index order, the copy's indices
// coincide with the original's and the child lists carry over verbatim.
constexpr auto reflection_law() -> bool {
    tree copy;
    int const root =
        cata<int>(sample_tree(), [&copy](layer_of_int const &layer) -> int {
            return std::visit(
                [&copy](auto const &l) -> int {
                    if constexpr (requires { l.children; }) {
                        return copy.add_branch(l.tag, l.children);
                    } else {
                        return copy.add_leaf(l);
                    }
                },
                layer);
        });
    copy.set_root(root);
    return copy == sample_tree();
}

// para with an algebra that ignores the tree and index is cata.
constexpr auto para_agrees_with_cata() -> bool {
    auto const summed = para<int>(
        sample_tree(), [](tree const &, int, layer_of_int const &layer) -> int {
            return sum_alg(layer);
        });
    return summed == cata<int>(sample_tree(), sum_alg);
}

// para's extra arguments are the node's identity: an algebra can weight
// each node by its own index.
constexpr auto para_sees_the_node() -> bool {
    auto const weighted = para<int>(
        sample_tree(),
        [](tree const &t, int index, layer_of_int const &layer) -> int {
            return sum_alg(layer) + (t.is_leaf(index) ? 0 : 100);
        });
    // Leaves contribute their values; the two branches add 100 each.
    return weighted == 206;
}

// On a reader-built tree — children added left to right — a monoidal
// cata agrees with the Foldable instance's leaf fold.
constexpr auto cata_agrees_with_fold_map_on_built_trees() -> bool {
    auto const digits_structural =
        cata<int>(sample_tree(), [](layer_of_int const &layer) {
            return std::visit(
                [](auto const &l) -> int {
                    if constexpr (requires { l.children; }) {
                        return std::ranges::fold_left(
                            l.children, 0, [](int acc, int child) {
                                return acc * 10 + child;
                            });
                    } else {
                        return l;
                    }
                },
                layer);
        });
    auto const digits_index_order = fold_left(
        [](int acc, int leaf) { return acc * 10 + leaf; }, 0, sample_tree());
    return digits_structural == 123 && digits_index_order == 123;
}

// And the pinned disagreement: from_nodes admits a tree whose child
// lists run against index order, and there the two folds differ —
// cata combines in structural child order, the Foldable instance in
// ascending index order. This is why the instances are not re-expressed
// through cata: their documented contract is the index order.
constexpr auto cata_and_fold_map_disagree_on_permuted_trees() -> bool {
    static_vector<tree::node_type, 8> nodes;
    nodes.push_back(tree::node_type{2}); // index 0
    nodes.push_back(tree::node_type{1}); // index 1
    branch_f<char, int, 4> swapped{'L', {}};
    swapped.children.push_back(1); // structurally first: the leaf 1
    swapped.children.push_back(0); // structurally second: the leaf 2
    nodes.push_back(tree::node_type{swapped});
    auto const permuted = tree::from_nodes(nodes, 2);
    auto const structural = cata<int>(permuted, [](layer_of_int const &layer) {
        return std::visit(
            [](auto const &l) -> int {
                if constexpr (requires { l.children; }) {
                    return std::ranges::fold_left(
                        l.children, 0,
                        [](int acc, int child) { return acc * 10 + child; });
                } else {
                    return l;
                }
            },
            layer);
    });
    auto const index_order = fold_left(
        [](int acc, int leaf) { return acc * 10 + leaf; }, 0, permuted);
    return structural == 12 && index_order == 21;
}

constexpr parse_error poison{source_pos{}, "poisoned leaf"};

constexpr auto cata_short_stops_at_the_leftmost_failure() -> bool {
    int visited = 0;
    auto const folded = cata_short<int>(
        sample_tree(), [&visited](layer_of_int const &layer) -> result<int> {
            ++visited;
            if (auto const *leaf = std::get_if<int>(&layer);
                leaf && *leaf == 2) {
                return poison;
            }
            return sum_alg(layer);
        });
    // The poisoned leaf is node 1; nodes 2..4 are never visited.
    return !folded.has_value() && folded.error() == poison && visited == 2;
}

constexpr auto para_short_succeeds_like_para() -> bool {
    auto const folded = para_short<int>(
        sample_tree(),
        [](tree const &, int, layer_of_int const &layer) -> result<int> {
            return sum_alg(layer);
        });
    return folded.has_value() && folded.value() == 6;
}

// Depth is the inherited attribute: the root 0, every child one more
// than its parent — scan_down through the link column.
constexpr auto scan_down_computes_depths() -> bool {
    auto const depths = scan_down<int>(
        sample_tree(), [](tree const &, int, int const *parent) -> int {
            return parent ? *parent + 1 : 0;
        });
    return depths.size() == 5 && depths[0] == 1 && depths[1] == 1 &&
           depths[2] == 2 && depths[3] == 1 && depths[4] == 0;
}

// A built-but-unreferenced node gets the same null the root does; the
// step tells them apart by index.
constexpr auto scan_down_distinguishes_root_by_index() -> bool {
    tree t;
    t.add_leaf(7); // never referenced
    t.set_root(t.add_leaf(8));
    auto const marks = scan_down<int>(
        t, [](tree const &within, int index, int const *parent) -> int {
            if (parent) {
                return 1;
            }
            return index == within.root() ? 2 : 3;
        });
    return marks.size() == 2 && marks[0] == 3 && marks[1] == 2;
}

} // namespace

static_assert(cata_sums_the_leaves());
static_assert(cata_computes_height());
static_assert(reflection_law());
static_assert(para_agrees_with_cata());
static_assert(para_sees_the_node());
static_assert(cata_agrees_with_fold_map_on_built_trees());
static_assert(cata_and_fold_map_disagree_on_permuted_trees());
static_assert(cata_short_stops_at_the_leftmost_failure());
static_assert(para_short_succeeds_like_para());
static_assert(scan_down_computes_depths());
static_assert(scan_down_distinguishes_root_by_index());

TEST_CASE("TaggedTreeSchemesTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("TaggedTreeSchemesTest - CataIsTheBottomUpFold") {
    CHECK(cata_sums_the_leaves());
    CHECK(cata_computes_height());
}

TEST_CASE("TaggedTreeSchemesTest - ReflectionLaw") { CHECK(reflection_law()); }

TEST_CASE("TaggedTreeSchemesTest - ParaExtendsCata") {
    CHECK(para_agrees_with_cata());
    CHECK(para_sees_the_node());
}

TEST_CASE("TaggedTreeSchemesTest - StructuralAndIndexOrderAgreeOnBuiltTrees") {
    CHECK(cata_agrees_with_fold_map_on_built_trees());
    CHECK(cata_and_fold_map_disagree_on_permuted_trees());
}

TEST_CASE("TaggedTreeSchemesTest - ShortCircuitingCarriers") {
    CHECK(cata_short_stops_at_the_leftmost_failure());
    CHECK(para_short_succeeds_like_para());
}

TEST_CASE("TaggedTreeSchemesTest - ScanDownInheritsThroughTheLinks") {
    CHECK(scan_down_computes_depths());
    CHECK(scan_down_distinguishes_root_by_index());
}
