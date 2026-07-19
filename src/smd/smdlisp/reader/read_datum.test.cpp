// src/smd/smdlisp/reader/read_datum.test.cpp                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/reader/read_datum.hpp>
#include <smd/smdlisp/reader/read_datum.hpp> // test 2nd include OK

#include <smd/smdscheme/parser/cursor.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace smd::smdlisp::reader {

namespace {

constexpr int MaxNodes = 64;
constexpr int MaxList = 16;
using datum = datum_type<MaxNodes, MaxList>;
using arena_t = smdscheme::foundation::tree_arena<datum, MaxNodes>;
using list_t = datum_list<datum, MaxNodes, MaxList>;
using quote_t = datum_quote<datum, MaxNodes>;
using function_t = datum_function<datum, MaxNodes>;

/// Reads one datum from @p src for tests.
auto read(std::string_view src, arena_t &arena)
    -> smdscheme::parser::parse_result<datum> {
    return read_datum<MaxNodes, MaxList>(smdscheme::parser::cursor{src}, arena);
}

} // namespace

TEST_CASE("ReadDatumTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- atoms delegate to atom_p, not reimplemented here (DIV-0003) -----------

TEST_CASE("ReadDatumTest - integer atom") {
    arena_t arena;
    auto r = read("42", arena);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<datum_integer>(r.value().value.inner));
    REQUIRE(std::get<datum_integer>(r.value().value.inner).value == 42);
}

TEST_CASE("ReadDatumTest - symbol atom is folded to uppercase (D2)") {
    arena_t arena;
    auto r = read("foo", arena);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<datum_symbol>(r.value().value.inner));
    REQUIRE(std::get<datum_symbol>(r.value().value.inner).name.view() == "FOO");
}

TEST_CASE("ReadDatumTest - keyword atom is a distinct kind (D7)") {
    arena_t arena;
    auto r = read(":bar", arena);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<datum_keyword>(r.value().value.inner));
    REQUIRE(std::get<datum_keyword>(r.value().value.inner).name.view() ==
            "BAR");
}

TEST_CASE("ReadDatumTest - digit-led symbol like 1+ is not misread as an "
          "integer (DIV-0003)") {
    arena_t arena;
    auto r = read("1+", arena);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<datum_symbol>(r.value().value.inner));
    REQUIRE(std::get<datum_symbol>(r.value().value.inner).name.view() == "1+");
}

// -- lists (no dotted pairs, decision D6) -----------------------------------

TEST_CASE("ReadDatumTest - empty list") {
    arena_t arena;
    auto r = read("()", arena);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<list_t>(r.value().value.inner));
    REQUIRE(std::get<list_t>(r.value().value.inner).elements.size() == 0);
}

TEST_CASE("ReadDatumTest - list of atoms") {
    arena_t arena;
    auto r = read("(1 foo :bar)", arena);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<list_t>(r.value().value.inner));
    auto const &list = std::get<list_t>(r.value().value.inner);
    REQUIRE(list.elements.size() == 3);
    REQUIRE(std::holds_alternative<datum_integer>(
        arena.get(list.elements[0]).inner));
    REQUIRE(std::holds_alternative<datum_symbol>(
        arena.get(list.elements[1]).inner));
    REQUIRE(std::holds_alternative<datum_keyword>(
        arena.get(list.elements[2]).inner));
}

TEST_CASE("ReadDatumTest - nested list") {
    arena_t arena;
    auto r = read("(1 (2 3) 4)", arena);
    REQUIRE(r.has_value());
    auto const &list = std::get<list_t>(r.value().value.inner);
    REQUIRE(list.elements.size() == 3);
    REQUIRE(std::holds_alternative<list_t>(arena.get(list.elements[1]).inner));
    auto const &inner = std::get<list_t>(arena.get(list.elements[1]).inner);
    REQUIRE(inner.elements.size() == 2);
}

// -- quote and function-quote (grammar: datum := atom | list | 'datum |
//    #'datum) -----------------------------------------------------------

TEST_CASE("ReadDatumTest - 'x lowers to a quote node") {
    arena_t arena;
    auto r = read("'x", arena);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<quote_t>(r.value().value.inner));
    auto const &q = std::get<quote_t>(r.value().value.inner);
    REQUIRE(std::holds_alternative<datum_symbol>(arena.get(q.quoted).inner));
    REQUIRE(std::get<datum_symbol>(arena.get(q.quoted).inner).name.view() ==
            "X");
}

TEST_CASE("ReadDatumTest - #'f lowers to a function-quote node, not a "
          "(function f) list") {
    arena_t arena;
    auto r = read("#'f", arena);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<function_t>(r.value().value.inner));
    auto const &fn = std::get<function_t>(r.value().value.inner);
    REQUIRE(std::holds_alternative<datum_symbol>(arena.get(fn.target).inner));
    REQUIRE(std::get<datum_symbol>(arena.get(fn.target).inner).name.view() ==
            "F");
    REQUIRE_FALSE(std::holds_alternative<list_t>(r.value().value.inner));
}

TEST_CASE("ReadDatumTest - #'(lambda (x) x) sharpsign-quotes a compound "
          "datum") {
    arena_t arena;
    auto r = read("#'(lambda (x) x)", arena);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<function_t>(r.value().value.inner));
    auto const &fn = std::get<function_t>(r.value().value.inner);
    REQUIRE(std::holds_alternative<list_t>(arena.get(fn.target).inner));
    auto const &lam = std::get<list_t>(arena.get(fn.target).inner);
    REQUIRE(lam.elements.size() == 3);
    REQUIRE(
        std::get<datum_symbol>(arena.get(lam.elements[0]).inner).name.view() ==
        "LAMBDA");
}

TEST_CASE("ReadDatumTest - # not followed by ' is an error") {
    arena_t arena;
    auto r = read("#x", arena);
    REQUIRE(!r.has_value());
}

TEST_CASE("ReadDatumTest - a lone # at end of input is an error") {
    arena_t arena;
    auto r = read("#", arena);
    REQUIRE(!r.has_value());
}

// -- the merge-criteria round trip: (defun f (x) (if x 1 2)) ---------------

TEST_CASE("ReadDatumTest - (defun f (x) (if x 1 2)) round-trips into the "
          "tree") {
    arena_t arena;
    auto r = read("(defun f (x) (if x 1 2))", arena);
    REQUIRE(r.has_value());
    auto const &top = std::get<list_t>(r.value().value.inner);
    REQUIRE(top.elements.size() == 4);

    REQUIRE(
        std::holds_alternative<datum_symbol>(arena.get(top.elements[0]).inner));
    REQUIRE(
        std::get<datum_symbol>(arena.get(top.elements[0]).inner).name.view() ==
        "DEFUN");

    REQUIRE(
        std::holds_alternative<datum_symbol>(arena.get(top.elements[1]).inner));
    REQUIRE(
        std::get<datum_symbol>(arena.get(top.elements[1]).inner).name.view() ==
        "F");

    REQUIRE(std::holds_alternative<list_t>(arena.get(top.elements[2]).inner));
    auto const &params = std::get<list_t>(arena.get(top.elements[2]).inner);
    REQUIRE(params.elements.size() == 1);
    REQUIRE(std::holds_alternative<datum_symbol>(
        arena.get(params.elements[0]).inner));
    REQUIRE(std::get<datum_symbol>(arena.get(params.elements[0]).inner)
                .name.view() == "X");

    REQUIRE(std::holds_alternative<list_t>(arena.get(top.elements[3]).inner));
    auto const &body = std::get<list_t>(arena.get(top.elements[3]).inner);
    REQUIRE(body.elements.size() == 4);
    REQUIRE(
        std::get<datum_symbol>(arena.get(body.elements[0]).inner).name.view() ==
        "IF");
    REQUIRE(
        std::get<datum_symbol>(arena.get(body.elements[1]).inner).name.view() ==
        "X");
    REQUIRE(std::get<datum_integer>(arena.get(body.elements[2]).inner).value ==
            1);
    REQUIRE(std::get<datum_integer>(arena.get(body.elements[3]).inner).value ==
            2);
}

// -- failure cases -----------------------------------------------------

TEST_CASE("ReadDatumTest - unterminated list is an error") {
    arena_t arena;
    auto r = read("(1 2 3", arena);
    REQUIRE(!r.has_value());
}

TEST_CASE("ReadDatumTest - unterminated nested list is an error") {
    arena_t arena;
    auto r = read("(defun f (x", arena);
    REQUIRE(!r.has_value());
}

TEST_CASE("ReadDatumTest - stray close paren is an error") {
    arena_t arena;
    auto r = read(")", arena);
    REQUIRE(!r.has_value());
}

TEST_CASE("ReadDatumTest - a trailing close paren after a complete datum is "
          "left unread, not an error") {
    // read_datum reads exactly one datum; it does not require the whole
    // input to be consumed.
    arena_t arena;
    auto r = read("1)", arena);
    REQUIRE(r.has_value());
    REQUIRE(r.value().rest.remaining() == ")");
}

TEST_CASE("ReadDatumTest - empty input is an error") {
    arena_t arena;
    auto r = read("", arena);
    REQUIRE(!r.has_value());
}

// -- ; comments are intertoken space, so they work inside forms -----------

TEST_CASE("ReadDatumTest - a ; comment inside a list is skipped") {
    arena_t arena;
    auto r = read("(1 ; a comment\n 2)", arena);
    REQUIRE(r.has_value());
    auto const &list = std::get<list_t>(r.value().value.inner);
    REQUIRE(list.elements.size() == 2);
    REQUIRE(std::get<datum_integer>(arena.get(list.elements[0]).inner).value ==
            1);
    REQUIRE(std::get<datum_integer>(arena.get(list.elements[1]).inner).value ==
            2);
}

} // namespace smd::smdlisp::reader
