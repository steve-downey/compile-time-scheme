// src/smd/cl/foundation/topo_fold.test.cpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/foundation/topo_fold.hpp>
#include <smd/cl/foundation/topo_fold.hpp> // test 2nd include OK

#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/static_vector.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <variant>

using smd::cl::foundation::fold_down;
using smd::cl::foundation::fold_up;
using smd::cl::foundation::fold_up_short;
using smd::cl::foundation::parse_error;
using smd::cl::foundation::result;
using smd::cl::foundation::source_pos;
using smd::cl::foundation::static_vector;

namespace {

// A test-local layer type, the way foldable.test.cpp keeps pair_box: a
// column of expression layers, leaves holding a number and branches
// naming their operands by earlier position.
struct num {
    int value;
};
struct plus {
    int left;
    int right;
};
using expr_layer = std::variant<num, plus>;

using expr_column = static_vector<expr_layer, 8>;

// The column for ((1 + 2) + 3): children before parents, so position
// order is 1, 2, (+ 0 1), 3, (+ 2 3) and the root is the last entry.
constexpr auto sample_column() -> expr_column {
    expr_column layers;
    layers.push_back(num{1});
    layers.push_back(num{2});
    layers.push_back(plus{0, 1});
    layers.push_back(num{3});
    layers.push_back(plus{2, 3});
    return layers;
}

// The same relation read the other way: each position's parent, -1 at
// the root. plus{0,1} at 2 and the root at 4 own everything else.
constexpr auto sample_parents() -> static_vector<int, 8> {
    static_vector<int, 8> parents;
    parents.push_back(2);
    parents.push_back(2);
    parents.push_back(4);
    parents.push_back(4);
    parents.push_back(-1);
    return parents;
}

// The evaluation algebra: a leaf is its value, a branch reads its
// operands' already-computed values out of the fold's own accumulator.
// std::visit dispatches one layer's alternatives; the fold drives.
constexpr auto eval_alg = [](static_vector<int, 8> const &done,
                             expr_layer const &layer) -> int {
    return std::visit(
        [&done](auto const &l) -> int {
            if constexpr (requires { l.value; }) {
                return l.value;
            } else {
                return done[l.left] + done[l.right];
            }
        },
        layer);
};

constexpr auto fold_up_evaluates_bottom_up() -> bool {
    auto const values = fold_up<int, 8>(sample_column(), eval_alg);
    return values.size() == 5 && values[0] == 1 && values[1] == 2 &&
           values[2] == 3 && values[3] == 3 && values[4] == 6;
}

// A child's entry is final when its parent reads it: the algebra sees
// exactly as many finished results as there are earlier positions.
constexpr auto children_are_final_when_read() -> bool {
    int position = 0;
    bool sizes_agree = true;
    auto const values = fold_up<int, 8>(
        sample_column(),
        [&](static_vector<int, 8> const &done, expr_layer const &layer) -> int {
            sizes_agree = sizes_agree && done.size() == position;
            ++position;
            return eval_alg(done, layer);
        });
    return sizes_agree && position == 5 && values.size() == 5;
}

constexpr auto fold_up_over_leaves_is_a_transform() -> bool {
    expr_column layers;
    layers.push_back(num{4});
    layers.push_back(num{5});
    auto const values = fold_up<int, 8>(layers, eval_alg);
    return values.size() == 2 && values[0] == 4 && values[1] == 5;
}

constexpr auto fold_up_of_nothing_is_nothing() -> bool {
    return fold_up<int, 8>(expr_column{}, eval_alg).empty();
}

constexpr parse_error negative{source_pos{}, "negative leaf"};

// The short-circuiting algebra rejects a chosen leaf.
constexpr auto reject_two = [](static_vector<int, 8> const &done,
                               expr_layer const &layer) -> result<int> {
    if (auto const *leaf = std::get_if<num>(&layer); leaf && leaf->value == 2) {
        return negative;
    }
    return eval_alg(done, layer);
};

constexpr auto fold_up_short_reports_the_leftmost_failure() -> bool {
    auto const folded = fold_up_short<int, 8>(sample_column(), reject_two);
    return !folded.has_value() && folded.error() == negative;
}

constexpr auto fold_up_short_visits_nothing_after_a_failure() -> bool {
    int visited = 0;
    auto const folded = fold_up_short<int, 8>(
        sample_column(),
        [&visited](static_vector<int, 8> const &done,
                   expr_layer const &layer) -> result<int> {
            ++visited;
            return reject_two(done, layer);
        });
    // The failing leaf is position 1; positions 2..4 are never visited.
    return !folded.has_value() && visited == 2;
}

constexpr auto fold_up_short_succeeds_when_nothing_fails() -> bool {
    auto const folded =
        fold_up_short<int, 8>(sample_column(),
                              [](static_vector<int, 8> const &done,
                                 expr_layer const &layer) -> result<int> {
                                  return eval_alg(done, layer);
                              });
    return folded.has_value() && folded.value()[4] == 6;
}

// Depth is the inherited attribute: the root is 0, everything else is
// one more than its parent.
constexpr auto fold_down_computes_depths() -> bool {
    auto const depths =
        fold_down<int, 8>(sample_parents(), [](int const *parent, int) {
            return parent ? *parent + 1 : 0;
        });
    return depths.size() == 5 && depths[0] == 2 && depths[1] == 2 &&
           depths[2] == 1 && depths[3] == 1 && depths[4] == 0;
}

// A parentless element that is not the root gets the same null the root
// does; distinguishing them is the step's business, via the index.
constexpr auto fold_down_step_sees_its_own_position() -> bool {
    static_vector<int, 8> parents;
    parents.push_back(-1);
    parents.push_back(-1);
    auto const values =
        fold_down<int, 8>(parents, [](int const *parent, int index) {
            return parent ? *parent : index * 10;
        });
    return values.size() == 2 && values[0] == 0 && values[1] == 10;
}

constexpr auto fold_down_of_nothing_is_nothing() -> bool {
    return fold_down<int, 8>(static_vector<int, 8>{},
                             [](int const *, int) { return 0; })
        .empty();
}

constexpr auto folds_fill_to_capacity() -> bool {
    static_vector<expr_layer, 3> layers;
    layers.push_back(num{1});
    layers.push_back(num{2});
    layers.push_back(plus{0, 1});
    auto const values = fold_up<int, 3>(
        layers,
        [](static_vector<int, 3> const &done, expr_layer const &layer) -> int {
            return std::visit(
                [&done](auto const &l) -> int {
                    if constexpr (requires { l.value; }) {
                        return l.value;
                    } else {
                        return done[l.left] + done[l.right];
                    }
                },
                layer);
        });
    return values.size() == values.capacity() && values[2] == 3;
}

} // namespace

static_assert(fold_up_evaluates_bottom_up());
static_assert(children_are_final_when_read());
static_assert(fold_up_over_leaves_is_a_transform());
static_assert(fold_up_of_nothing_is_nothing());
static_assert(fold_up_short_reports_the_leftmost_failure());
static_assert(fold_up_short_visits_nothing_after_a_failure());
static_assert(fold_up_short_succeeds_when_nothing_fails());
static_assert(fold_down_computes_depths());
static_assert(fold_down_step_sees_its_own_position());
static_assert(fold_down_of_nothing_is_nothing());
static_assert(folds_fill_to_capacity());

TEST_CASE("TopoFoldTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("TopoFoldTest - FoldUpEvaluatesBottomUp") {
    CHECK(fold_up_evaluates_bottom_up());
    CHECK(children_are_final_when_read());
    CHECK(fold_up_over_leaves_is_a_transform());
    CHECK(fold_up_of_nothing_is_nothing());
}

TEST_CASE("TopoFoldTest - FoldUpShortStopsAtTheLeftmostFailure") {
    CHECK(fold_up_short_reports_the_leftmost_failure());
    CHECK(fold_up_short_visits_nothing_after_a_failure());
    CHECK(fold_up_short_succeeds_when_nothing_fails());
}

TEST_CASE("TopoFoldTest - FoldDownInheritsFromParents") {
    CHECK(fold_down_computes_depths());
    CHECK(fold_down_step_sees_its_own_position());
    CHECK(fold_down_of_nothing_is_nothing());
}

TEST_CASE("TopoFoldTest - CapacityEdges") { CHECK(folds_fill_to_capacity()); }
