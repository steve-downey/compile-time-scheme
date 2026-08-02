// src/smd/cl/reader/datum.test.cpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/reader/datum.hpp>
#include <smd/cl/reader/datum.hpp> // test 2nd include OK

#include <smd/cl/foundation/foldable.hpp>
#include <smd/cl/foundation/functor.hpp>

#include <catch2/catch_test_macros.hpp>

#include <variant>

using smd::cl::foundation::length;
using smd::cl::reader::datum_atom;
using smd::cl::reader::datum_branch;
using smd::cl::reader::datum_character;
using smd::cl::reader::datum_fixnum;
using smd::cl::reader::datum_keyword;
using smd::cl::reader::datum_string;
using smd::cl::reader::datum_symbol;
using smd::cl::reader::datum_tower;
using smd::cl::reader::datum_tree;
using smd::cl::reader::tower_kind;
using smd::cl::symbol::symbol_id;

namespace {

using tree = datum_tree<8, 4>;

// The datum tree for '(1 :k) built by hand: children first, left to right.
constexpr auto sample_tree() -> tree {
    tree t;
    int const one = t.add_leaf(datum_atom{datum_fixnum{1}});
    int const kw = t.add_leaf(datum_atom{datum_keyword{symbol_id{3}}});
    tree::child_list elements;
    elements.push_back(one);
    elements.push_back(kw);
    int const list = t.add_branch(datum_branch::list, elements);
    tree::child_list quoted;
    quoted.push_back(list);
    t.set_root(t.add_branch(datum_branch::quote, quoted));
    return t;
}

constexpr auto atoms_are_comparable_values() -> bool {
    datum_string hello{};
    hello.storage[0] = 'h';
    hello.storage[1] = 'i';
    hello.length = 2;
    return datum_fixnum{42} == datum_fixnum{42} &&
           !(datum_fixnum{1} == datum_fixnum{2}) &&
           datum_symbol{symbol_id{1}} == datum_symbol{symbol_id{1}} &&
           !(datum_symbol{symbol_id{1}} == datum_symbol{symbol_id{2}}) &&
           datum_character{'a'} == datum_character{'a'} &&
           hello.view() == "hi" && hello == hello;
}

constexpr auto tower_carries_spelling_and_radix() -> bool {
    datum_tower big{};
    big.kind = tower_kind::bignum;
    big.radix = 16;
    big.spelling.storage[0] = 'F';
    big.spelling.storage[1] = 'F';
    big.spelling.length = 2;
    return big.spelling.view() == "FF" && big.radix == 16 && big == big &&
           !(big == datum_tower{});
}

constexpr auto datum_tree_composes() -> bool {
    auto const t = sample_tree();
    return t.size() == 4 && t.root() == 3 &&
           t.branch(t.root()).tag == datum_branch::quote &&
           t.branch(2).tag == datum_branch::list &&
           std::holds_alternative<datum_fixnum>(t.leaf(0)) &&
           std::holds_alternative<datum_keyword>(t.leaf(1));
}

constexpr auto datum_tree_carries_the_instances() -> bool {
    // D15: the tree is Foldable (and Traversable, exercised in
    // read.test.cpp) through the tagged_tree registrations.
    return length(sample_tree()) == 2;
}

} // namespace

static_assert(atoms_are_comparable_values());
static_assert(tower_carries_spelling_and_radix());
static_assert(datum_tree_composes());
static_assert(datum_tree_carries_the_instances());

TEST_CASE("DatumTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("DatumTest - AtomsAreComparableValues") {
    CHECK(atoms_are_comparable_values());
    CHECK(tower_carries_spelling_and_radix());
}

TEST_CASE("DatumTest - TreeComposesAndCarriesInstances") {
    CHECK(datum_tree_composes());
    CHECK(datum_tree_carries_the_instances());
}
