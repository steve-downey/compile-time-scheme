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

using smd::cl::core::core_block;
using smd::cl::core::core_call;
using smd::cl::core::core_character;
using smd::cl::core::core_cons;
using smd::cl::core::core_defun;
using smd::cl::core::core_fixnum;
using smd::cl::core::core_if;
using smd::cl::core::core_keyword;
using smd::cl::core::core_lambda;
using smd::cl::core::core_leaf;
using smd::cl::core::core_nil;
using smd::cl::core::core_quoted_symbol;
using smd::cl::core::core_return_from;
using smd::cl::core::core_string;
using smd::cl::core::core_t;
using smd::cl::core::core_tag;
using smd::cl::core::core_tree;
using smd::cl::core::core_unwind_protect;
using smd::cl::core::core_variable;
using smd::cl::core::max_lambda_params;
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
           !(core_call{symbol_id{2}} == core_call{symbol_id{4}}) &&
           core_character{'a'} == core_character{'a'} &&
           !(core_character{'a'} == core_character{'b'}) &&
           core_cons{} == core_cons{};
}

// A symbol as data and the same symbol as a variable reference are
// different leaves carrying one id: the position picks the namespace, and
// the leaf records which one won.
constexpr auto a_quoted_symbol_is_not_a_variable() -> bool {
    return core_quoted_symbol{symbol_id{1}} ==
               core_quoted_symbol{symbol_id{1}} &&
           !(core_quoted_symbol{symbol_id{1}} ==
             core_quoted_symbol{symbol_id{2}}) &&
           !(core_leaf{core_quoted_symbol{symbol_id{1}}} ==
             core_leaf{core_variable{symbol_id{1}}});
}

// A string leaf owns its characters, so it survives the source text it was
// read from — the dangling-view failure the old tree's core_symbol records
// cannot arise here.
constexpr auto a_string_leaf_owns_its_characters() -> bool {
    auto const copied = [] {
        core_string original{};
        original.length = 2;
        original.storage[0] = 'h';
        original.storage[1] = 'i';
        return original;
    }();
    core_string other{};
    other.length = 2;
    other.storage[0] = 'h';
    other.storage[1] = 'i';
    return copied.view() == "hi" && copied == other &&
           copied.view().data() != other.view().data();
}

// The shape 'a elaborates to: one hermetic pair, car a symbol as data,
// cdr the empty list. core_cons exists because this must not be a call to
// whatever the function slot of CONS holds.
constexpr auto quoted_data_composes() -> bool {
    using pair_tree = core_tree<8, 2>;
    pair_tree t;
    int const car = t.add_leaf(core_leaf{core_quoted_symbol{symbol_id{1}}});
    int const cdr = t.add_leaf(core_leaf{core_nil{}});
    pair_tree::child_list halves;
    halves.push_back(car);
    halves.push_back(cdr);
    t.set_root(t.add_branch(core_tag{core_cons{}}, halves));
    auto const &root = t.branch(t.root());
    return t.size() == 3 && std::holds_alternative<core_cons>(root.tag) &&
           root.children.size() == 2 &&
           std::holds_alternative<core_quoted_symbol>(t.leaf(root.children[0]));
}

// The five tags step R5 added, and the one thing each says that no other
// tag does — decision D18's bar, restated as a test.
constexpr auto the_control_tags_are_comparable_values() -> bool {
    return core_defun{symbol_id{1}} == core_defun{symbol_id{1}} &&
           !(core_defun{symbol_id{1}} == core_defun{symbol_id{2}}) &&
           core_block{symbol_id{1}} == core_block{symbol_id{1}} &&
           !(core_block{symbol_id{1}} == core_block{symbol_id{2}}) &&
           core_return_from{symbol_id{1}} == core_return_from{symbol_id{1}} &&
           core_unwind_protect{} == core_unwind_protect{} &&
           core_lambda{} == core_lambda{};
}

// A block and a return-from that name the same symbol are still different
// tags: one is where an unwind lands, the other is where it starts. That
// distinction is why the two ids being equal does not make the tags equal.
constexpr auto naming_a_block_is_not_leaving_it() -> bool {
    return !(core_tag{core_block{symbol_id{1}}} ==
             core_tag{core_return_from{symbol_id{1}}});
}

// A lambda's parameters are storage inside the node, so max_lambda_params is
// a constant rather than a template parameter — the same reason
// max_string_chars is one, and the same rule (decision D14).
constexpr auto a_lambda_carries_its_parameters() -> bool {
    core_lambda one_parameter{};
    one_parameter.params.push_back(symbol_id{5});
    core_lambda same{};
    same.params.push_back(symbol_id{5});
    return max_lambda_params > 0 && one_parameter.params.size() == 1 &&
           one_parameter.params[0] == symbol_id{5} && one_parameter == same &&
           !(one_parameter == core_lambda{});
}

// The shape (defun f (x) x) elaborates to: a defun over a lambda over a
// block over the body. Three nested nodes, because each says exactly one
// thing — install in a function slot, bind parameters, catch an unwind.
constexpr auto a_definition_nests_three_tags() -> bool {
    using definition_tree = core_tree<8, 2>;
    constexpr symbol_id f{1};
    constexpr symbol_id x{2};
    definition_tree t;
    int const body = t.add_leaf(core_leaf{core_variable{x}});
    definition_tree::child_list one;
    one.push_back(body);
    int const block = t.add_branch(core_tag{core_block{f}}, one);
    core_lambda parameters{};
    parameters.params.push_back(x);
    definition_tree::child_list wrapped;
    wrapped.push_back(block);
    int const lambda = t.add_branch(core_tag{core_lambda{parameters}}, wrapped);
    definition_tree::child_list installed;
    installed.push_back(lambda);
    t.set_root(t.add_branch(core_tag{core_defun{f}}, installed));
    auto const &root = t.branch(t.root());
    return std::get<core_defun>(root.tag).name == f &&
           std::get<core_lambda>(t.branch(root.children[0]).tag)
                   .params.size() == 1 &&
           std::get<core_block>(
               t.branch(t.branch(root.children[0]).children[0]).tag)
                   .name == f;
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
static_assert(a_quoted_symbol_is_not_a_variable());
static_assert(a_string_leaf_owns_its_characters());
static_assert(quoted_data_composes());
static_assert(the_control_tags_are_comparable_values());
static_assert(naming_a_block_is_not_leaving_it());
static_assert(a_lambda_carries_its_parameters());
static_assert(a_definition_nests_three_tags());
static_assert(core_tree_composes());
static_assert(core_tree_is_foldable());
static_assert(core_tree_is_functorial());
static_assert(core_tree_is_traversable());

TEST_CASE("AstTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("AstTest - LeavesAreComparableValues") {
    CHECK(leaves_are_comparable_values());
    CHECK(a_quoted_symbol_is_not_a_variable());
    CHECK(a_string_leaf_owns_its_characters());
}

TEST_CASE("AstTest - QuotedDataComposes") { CHECK(quoted_data_composes()); }

TEST_CASE("AstTest - ControlTags") {
    CHECK(the_control_tags_are_comparable_values());
    CHECK(naming_a_block_is_not_leaving_it());
    CHECK(a_lambda_carries_its_parameters());
    CHECK(a_definition_nests_three_tags());
}

TEST_CASE("AstTest - CoreTreeComposes") { CHECK(core_tree_composes()); }

TEST_CASE("AstTest - CoreTreeCarriesTheInstances") {
    CHECK(core_tree_is_foldable());
    CHECK(core_tree_is_functorial());
    CHECK(core_tree_is_traversable());
}
