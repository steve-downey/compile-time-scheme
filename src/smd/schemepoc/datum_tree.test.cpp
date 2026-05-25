// src/smd/schemepoc/datum_tree.test.cpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/datum_tree.hpp>
#include <smd/schemepoc/datum_tree.hpp>  // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <string_view>

using namespace smd::schemepoc;

static_assert([] {
    datum_tree<8, 4> tree;
    auto a = tree.add(datum_integer{1});
    auto b = tree.add(datum_integer{2});
    datum_list<4> xs;
    xs.elements.push_back(a);
    xs.elements.push_back(b);
    tree.add(xs);
    return tree.size() == 3;
}());

static_assert([] {
    datum_tree<8, 4> tree;
    auto id = tree.add(datum_integer{42});
    auto const& node = tree.get(id);
    return std::get<datum_integer>(node).value == 42;
}());

static_assert([] {
    datum_tree<8, 4> tree;
    auto id = tree.add(datum_symbol{"hello"});
    auto const& node = tree.get(id);
    return std::get<datum_symbol>(node).name == std::string_view{"hello"};
}());

static_assert([] {
    datum_tree<8, 4> tree;
    auto id = tree.add(datum_boolean{true});
    auto const& node = tree.get(id);
    return std::get<datum_boolean>(node).value == true;
}());

static_assert([] {
    datum_tree<8, 4> tree;
    auto a  = tree.add(datum_integer{10});
    auto id = tree.add(datum_quote{a});
    auto const& node = tree.get(id);
    return std::get<datum_quote>(node).quoted == a;
}());

TEST_CASE("DatumTree - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("DatumTree - AddAndGetInteger") {
    datum_tree<8, 4> tree;
    auto id         = tree.add(datum_integer{99});
    auto const& node = tree.get(id);
    REQUIRE(std::holds_alternative<datum_integer>(node));
    REQUIRE(std::get<datum_integer>(node).value == 99);
}

TEST_CASE("DatumTree - AddAndGetSymbol") {
    datum_tree<8, 4> tree;
    auto id          = tree.add(datum_symbol{"foo"});
    auto const& node = tree.get(id);
    REQUIRE(std::holds_alternative<datum_symbol>(node));
    REQUIRE(std::get<datum_symbol>(node).name == "foo");
}

TEST_CASE("DatumTree - AddAndGetBoolean") {
    datum_tree<8, 4> tree;
    auto id          = tree.add(datum_boolean{false});
    auto const& node = tree.get(id);
    REQUIRE(std::holds_alternative<datum_boolean>(node));
    REQUIRE(std::get<datum_boolean>(node).value == false);
}

TEST_CASE("DatumTree - ListPointsToChildren") {
    datum_tree<8, 4> tree;
    auto a = tree.add(datum_integer{1});
    auto b = tree.add(datum_integer{2});
    datum_list<4> xs;
    xs.elements.push_back(a);
    xs.elements.push_back(b);
    auto list_id     = tree.add(xs);
    auto const& node = tree.get(list_id);
    REQUIRE(std::holds_alternative<datum_list<4>>(node));
    auto const& lst = std::get<datum_list<4>>(node);
    REQUIRE(lst.elements.size() == 2);
    REQUIRE(lst.elements[0] == a);
    REQUIRE(lst.elements[1] == b);
}

TEST_CASE("DatumTree - Quote") {
    datum_tree<8, 4> tree;
    auto a  = tree.add(datum_symbol{"x"});
    auto id = tree.add(datum_quote{a});
    REQUIRE(std::get<datum_quote>(tree.get(id)).quoted == a);
}

TEST_CASE("DatumTree - SizeTracking") {
    datum_tree<8, 4> tree;
    REQUIRE(tree.size() == 0);
    tree.add(datum_integer{1});
    REQUIRE(tree.size() == 1);
    tree.add(datum_integer{2});
    REQUIRE(tree.size() == 2);
}
