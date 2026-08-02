// src/smd/cl/core/ast.test.cpp                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/core/ast.hpp>
#include <smd/cl/core/ast.hpp> // test 2nd include OK

#include <smd/cl/foundation/foldable.hpp>
#include <smd/cl/foundation/functor.hpp>
#include <smd/cl/foundation/result_instances.hpp>
#include <smd/cl/foundation/traversable.hpp>

#include <catch2/catch_test_macros.hpp>

#include <variant>

using smd::cl::core::core_call;
using smd::cl::core::core_fixnum;
using smd::cl::core::core_if;
using smd::cl::core::core_keyword;
using smd::cl::core::core_leaf;
using smd::cl::core::core_nil;
using smd::cl::core::core_t;
using smd::cl::core::core_tag;
using smd::cl::core::core_tree;
using smd::cl::core::core_variable;
using smd::cl::foundation::fmap;
using smd::cl::foundation::fold_left;
using smd::cl::foundation::length;
using smd::cl::foundation::parse_error;
using smd::cl::foundation::result;
using smd::cl::foundation::traverse;
using smd::cl::symbol::symbol_id;

namespace {

using tree = core_tree<16, 4>;

// The core for (if (zerop n) 1 nil), built children-first as the
// elaborator will: leaves, then the call, then the if.
constexpr auto sample_core() -> tree {
    tree t;
    int const n_ref = t.add_leaf(core_leaf{core_variable{symbol_id{1}}});
    tree::child_list call_args;
    call_args.push_back(n_ref);
    int const test = t.add_branch(core_tag{core_call{symbol_id{2}}}, call_args);
    int const then_arm = t.add_leaf(core_leaf{core_fixnum{1}});
    int const else_arm = t.add_leaf(core_leaf{core_nil{}});
    tree::child_list arms;
    arms.push_back(test);
    arms.push_back(then_arm);
    arms.push_back(else_arm);
    t.set_root(t.add_branch(core_tag{core_if{}}, arms));
    return t;
}

constexpr auto leaves_are_comparable_values() -> bool {
    return core_fixnum{1} == core_fixnum{1} &&
           !(core_fixnum{1} == core_fixnum{2}) &&
           core_variable{symbol_id{1}} == core_variable{symbol_id{1}} &&
           !(core_variable{symbol_id{1}} == core_variable{symbol_id{2}}) &&
           core_keyword{symbol_id{3}} == core_keyword{symbol_id{3}} &&
           core_nil{} == core_nil{} && core_t{} == core_t{} &&
           core_call{symbol_id{2}} == core_call{symbol_id{2}} &&
           !(core_call{symbol_id{2}} == core_call{symbol_id{4}});
}

constexpr auto core_tree_composes() -> bool {
    auto const t = sample_core();
    auto const &root = t.branch(t.root());
    return t.size() == 5 && std::holds_alternative<core_if>(root.tag) &&
           root.children.size() == 3 &&
           std::holds_alternative<core_call>(t.branch(1).tag) &&
           std::get<core_call>(t.branch(1).tag).callee == symbol_id{2} &&
           std::holds_alternative<core_variable>(t.leaf(0)) &&
           std::holds_alternative<core_nil>(t.leaf(3));
}

// D15: the instances are present from the start, not retrofitted.
constexpr auto core_tree_is_foldable() -> bool {
    auto const t = sample_core();
    auto const count_variables = fold_left(
        [](int acc, core_leaf const &leaf) {
            return acc + (std::holds_alternative<core_variable>(leaf) ? 1 : 0);
        },
        0, t);
    return length(t) == 3 && count_variables == 1;
}

constexpr auto core_tree_is_functorial() -> bool {
    auto const t = sample_core();
    auto const relabeled = fmap(
        [](core_leaf const &leaf) -> core_leaf {
            if (std::holds_alternative<core_fixnum>(leaf)) {
                return core_leaf{core_fixnum{99}};
            }
            return leaf;
        },
        t);
    return relabeled.size() == t.size() &&
           std::get<core_fixnum>(relabeled.leaf(2)).value == 99 &&
           std::holds_alternative<core_nil>(relabeled.leaf(3));
}

constexpr auto core_tree_is_traversable() -> bool {
    // The elaborator's error propagation shape (D15): traverse the core
    // over the result applicative; the leftmost bad leaf's error wins.
    constexpr parse_error unbound{{}, "unbound variable"};
    auto const traversed = traverse(
        [](core_leaf const &leaf) -> result<core_leaf> {
            if (std::holds_alternative<core_variable>(leaf)) {
                return unbound;
            }
            return leaf;
        },
        sample_core());
    auto const all_good = traverse(
        [](core_leaf const &leaf) -> result<core_leaf> { return leaf; },
        sample_core());
    return !traversed.has_value() && traversed.error() == unbound &&
           all_good.has_value() && all_good.value() == sample_core();
}

} // namespace

static_assert(leaves_are_comparable_values());
static_assert(core_tree_composes());
static_assert(core_tree_is_foldable());
static_assert(core_tree_is_functorial());
static_assert(core_tree_is_traversable());

TEST_CASE("AstTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("AstTest - LeavesAreComparableValues") {
    CHECK(leaves_are_comparable_values());
}

TEST_CASE("AstTest - CoreTreeComposes") { CHECK(core_tree_composes()); }

TEST_CASE("AstTest - CoreTreeCarriesTheInstances") {
    CHECK(core_tree_is_foldable());
    CHECK(core_tree_is_functorial());
    CHECK(core_tree_is_traversable());
}
