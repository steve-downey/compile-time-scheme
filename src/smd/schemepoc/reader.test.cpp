// src/smd/schemepoc/reader.test.cpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/reader.hpp>
#include <smd/schemepoc/reader.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
using namespace smd::schemepoc;
using namespace std::string_view_literals;

// "42" -> one datum_integer{42}
static_assert([] {
    auto r = read_datum<32, 16>(cursor{"42"sv});
    if (!r.has_value())
        return false;
    auto const &tree = r.value().value;
    if (tree.size() != 1)
        return false;
    return std::holds_alternative<datum_integer>(tree.get(0)) &&
           std::get<datum_integer>(tree.get(0)).value == 42;
}());

// "foo" -> one datum_symbol{"foo"}
static_assert([] {
    auto r = read_datum<32, 16>(cursor{"foo"sv});
    if (!r.has_value())
        return false;
    auto const &tree = r.value().value;
    if (tree.size() != 1)
        return false;
    return std::holds_alternative<datum_symbol>(tree.get(0)) &&
           std::get<datum_symbol>(tree.get(0)).name == "foo"sv;
}());

// "#t" -> one datum_boolean{true}
static_assert([] {
    auto r = read_datum<32, 16>(cursor{"#t"sv});
    if (!r.has_value())
        return false;
    auto const &tree = r.value().value;
    if (tree.size() != 1)
        return false;
    return std::holds_alternative<datum_boolean>(tree.get(0)) &&
           std::get<datum_boolean>(tree.get(0)).value == true;
}());

// "#f" -> one datum_boolean{false}
static_assert([] {
    auto r = read_datum<32, 16>(cursor{"#f"sv});
    if (!r.has_value())
        return false;
    auto const &tree = r.value().value;
    if (tree.size() != 1)
        return false;
    return std::holds_alternative<datum_boolean>(tree.get(0)) &&
           std::get<datum_boolean>(tree.get(0)).value == false;
}());

// "()" -> one empty datum_list
static_assert([] {
    auto r = read_datum<32, 16>(cursor{"()"sv});
    if (!r.has_value())
        return false;
    auto const &tree = r.value().value;
    if (tree.size() != 1)
        return false;
    if (!std::holds_alternative<datum_list<16>>(tree.get(0)))
        return false;
    return std::get<datum_list<16>>(tree.get(0)).elements.size() == 0;
}());

// "(1 2)" -> three nodes: int(1), int(2), list[0,1]
static_assert([] {
    auto r = read_datum<32, 16>(cursor{"(1 2)"sv});
    if (!r.has_value())
        return false;
    auto const &tree = r.value().value;
    if (tree.size() != 3)
        return false;
    if (!std::holds_alternative<datum_integer>(tree.get(0)))
        return false;
    if (std::get<datum_integer>(tree.get(0)).value != 1)
        return false;
    if (!std::holds_alternative<datum_integer>(tree.get(1)))
        return false;
    if (std::get<datum_integer>(tree.get(1)).value != 2)
        return false;
    if (!std::holds_alternative<datum_list<16>>(tree.get(2)))
        return false;
    auto const &lst = std::get<datum_list<16>>(tree.get(2));
    return lst.elements.size() == 2 && lst.elements[0] == 0 &&
           lst.elements[1] == 1;
}());

// "'x" -> two nodes: symbol(x), quote{0}
static_assert([] {
    auto r = read_datum<32, 16>(cursor{"'x"sv});
    if (!r.has_value())
        return false;
    auto const &tree = r.value().value;
    if (tree.size() != 2)
        return false;
    if (!std::holds_alternative<datum_symbol>(tree.get(0)))
        return false;
    if (std::get<datum_symbol>(tree.get(0)).name != "x"sv)
        return false;
    if (!std::holds_alternative<datum_quote>(tree.get(1)))
        return false;
    return std::get<datum_quote>(tree.get(1)).quoted == 0;
}());

// "(1 (2 3))" -> five nodes: int(1), int(2), int(3), list[1,2], list[0,3]
static_assert([] {
    auto r = read_datum<32, 16>(cursor{"(1 (2 3))"sv});
    if (!r.has_value())
        return false;
    auto const &tree = r.value().value;
    if (tree.size() != 5)
        return false;
    if (!std::holds_alternative<datum_integer>(tree.get(0)))
        return false;
    if (std::get<datum_integer>(tree.get(0)).value != 1)
        return false;
    if (!std::holds_alternative<datum_integer>(tree.get(1)))
        return false;
    if (std::get<datum_integer>(tree.get(1)).value != 2)
        return false;
    if (!std::holds_alternative<datum_integer>(tree.get(2)))
        return false;
    if (std::get<datum_integer>(tree.get(2)).value != 3)
        return false;
    if (!std::holds_alternative<datum_list<16>>(tree.get(3)))
        return false;
    auto const &inner = std::get<datum_list<16>>(tree.get(3));
    if (inner.elements.size() != 2)
        return false;
    if (inner.elements[0] != 1 || inner.elements[1] != 2)
        return false;
    if (!std::holds_alternative<datum_list<16>>(tree.get(4)))
        return false;
    auto const &outer = std::get<datum_list<16>>(tree.get(4));
    return outer.elements.size() == 2 && outer.elements[0] == 0 &&
           outer.elements[1] == 3;
}());

} // namespace

TEST_CASE("ReaderTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ReaderTest - ParseInteger") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto r = read_datum<32, 16>(cursor{"42"sv});
    REQUIRE(r.has_value());
    auto const &tree = r.value().value;
    REQUIRE(tree.size() == 1);
    REQUIRE(std::holds_alternative<datum_integer>(tree.get(0)));
    REQUIRE(std::get<datum_integer>(tree.get(0)).value == 42);
}

TEST_CASE("ReaderTest - ParseNegativeInteger") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto r = read_datum<32, 16>(cursor{"-7"sv});
    REQUIRE(r.has_value());
    auto const &tree = r.value().value;
    REQUIRE(tree.size() == 1);
    REQUIRE(std::holds_alternative<datum_integer>(tree.get(0)));
    REQUIRE(std::get<datum_integer>(tree.get(0)).value == -7);
}

TEST_CASE("ReaderTest - ParseSymbol") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto r = read_datum<32, 16>(cursor{"hello"sv});
    REQUIRE(r.has_value());
    auto const &tree = r.value().value;
    REQUIRE(tree.size() == 1);
    REQUIRE(std::holds_alternative<datum_symbol>(tree.get(0)));
    REQUIRE(std::get<datum_symbol>(tree.get(0)).name == "hello"sv);
}

TEST_CASE("ReaderTest - ParseTrue") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto r = read_datum<32, 16>(cursor{"#t"sv});
    REQUIRE(r.has_value());
    auto const &tree = r.value().value;
    REQUIRE(tree.size() == 1);
    REQUIRE(std::holds_alternative<datum_boolean>(tree.get(0)));
    REQUIRE(std::get<datum_boolean>(tree.get(0)).value == true);
}

TEST_CASE("ReaderTest - ParseFalse") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto r = read_datum<32, 16>(cursor{"#f"sv});
    REQUIRE(r.has_value());
    auto const &tree = r.value().value;
    REQUIRE(tree.size() == 1);
    REQUIRE(std::holds_alternative<datum_boolean>(tree.get(0)));
    REQUIRE(std::get<datum_boolean>(tree.get(0)).value == false);
}

TEST_CASE("ReaderTest - ParseEmptyList") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto r = read_datum<32, 16>(cursor{"()"sv});
    REQUIRE(r.has_value());
    auto const &tree = r.value().value;
    REQUIRE(tree.size() == 1);
    REQUIRE(std::holds_alternative<datum_list<16>>(tree.get(0)));
    REQUIRE(std::get<datum_list<16>>(tree.get(0)).elements.size() == 0);
}

TEST_CASE("ReaderTest - ParseFlatList") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto r = read_datum<32, 16>(cursor{"(1 2)"sv});
    REQUIRE(r.has_value());
    auto const &tree = r.value().value;
    REQUIRE(tree.size() == 3);
    REQUIRE(std::holds_alternative<datum_list<16>>(tree.get(2)));
    auto const &lst = std::get<datum_list<16>>(tree.get(2));
    REQUIRE(lst.elements.size() == 2);
    REQUIRE(lst.elements[0] == 0);
    REQUIRE(lst.elements[1] == 1);
}

TEST_CASE("ReaderTest - ParseQuote") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto r = read_datum<32, 16>(cursor{"'x"sv});
    REQUIRE(r.has_value());
    auto const &tree = r.value().value;
    REQUIRE(tree.size() == 2);
    REQUIRE(std::holds_alternative<datum_symbol>(tree.get(0)));
    REQUIRE(std::get<datum_symbol>(tree.get(0)).name == "x"sv);
    REQUIRE(std::holds_alternative<datum_quote>(tree.get(1)));
    REQUIRE(std::get<datum_quote>(tree.get(1)).quoted == 0);
}

TEST_CASE("ReaderTest - ParseNestedList") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto r = read_datum<32, 16>(cursor{"(1 (2 3))"sv});
    REQUIRE(r.has_value());
    auto const &tree = r.value().value;
    REQUIRE(tree.size() == 5);
    REQUIRE(std::holds_alternative<datum_list<16>>(tree.get(4)));
    auto const &outer = std::get<datum_list<16>>(tree.get(4));
    REQUIRE(outer.elements.size() == 2);
    REQUIRE(outer.elements[0] == 0);
    REQUIRE(outer.elements[1] == 3);
}

TEST_CASE("ReaderTest - ParseWithLeadingWhitespace") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto r = read_datum<32, 16>(cursor{"  42"sv});
    REQUIRE(r.has_value());
    auto const &tree = r.value().value;
    REQUIRE(tree.size() == 1);
    REQUIRE(std::holds_alternative<datum_integer>(tree.get(0)));
    REQUIRE(std::get<datum_integer>(tree.get(0)).value == 42);
}
