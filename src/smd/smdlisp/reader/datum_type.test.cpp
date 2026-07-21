// src/smd/smdlisp/reader/datum_type.test.cpp                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/reader/datum_type.hpp>
#include <smd/smdlisp/reader/datum_type.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <variant>

namespace smd::smdlisp::reader {

namespace {

constexpr int MaxNodes = 8;
constexpr int MaxList = 4;
using datum = datum_type<MaxNodes, MaxList>;
using datum_f =
    typename datum_f_factory<MaxNodes, MaxList>::template type<datum>;
using list_t = datum_list<datum, MaxNodes, MaxList>;
using quote_t = datum_quote<datum, MaxNodes>;
using function_t = datum_function<datum, MaxNodes>;
using backquote_t = datum_backquote<datum, MaxNodes>;
using unquote_t = datum_unquote<datum, MaxNodes>;
using unquote_splice_t = datum_unquote_splice<datum, MaxNodes>;

} // namespace

TEST_CASE("DatumTypeTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- shape: nine kinds (integer, symbol, keyword, list, quote, function,
// backquote, unquote, unquote-splice -- the last three added in step L18) --

static_assert(std::variant_size_v<datum_f> == 9);

static_assert([] {
    datum d{datum_f{datum_integer{42}}};
    return std::holds_alternative<datum_integer>(d.inner) &&
           std::get<datum_integer>(d.inner).value == 42;
}());

// -- symbol and keyword are distinct alternatives (decision D7) ------------

TEST_CASE("DatumTypeTest - symbol and keyword are distinct alternatives (D7)") {
    datum sym{datum_f{datum_symbol{}}};
    datum kw{datum_f{datum_keyword{}}};
    REQUIRE(sym.inner.index() != kw.inner.index());
    REQUIRE(std::holds_alternative<datum_symbol>(sym.inner));
    REQUIRE(std::holds_alternative<datum_keyword>(kw.inner));
}

// -- list holds arena_box handles into a tree_arena of the same datum type -

TEST_CASE("DatumTypeTest - list holds arena_box handles to elements") {
    smdscheme::foundation::tree_arena<datum, MaxNodes> arena;
    auto box = smdscheme::foundation::make_arena_box(
        arena, datum{datum_f{datum_integer{1}}});
    list_t list{};
    list.elements.push_back(box);
    datum d{datum_f{list}};
    REQUIRE(std::holds_alternative<list_t>(d.inner));
    auto const &l = std::get<list_t>(d.inner);
    REQUIRE(l.elements.size() == 1);
    REQUIRE(
        std::holds_alternative<datum_integer>(arena.get(l.elements[0]).inner));
    REQUIRE(std::get<datum_integer>(arena.get(l.elements[0]).inner).value == 1);
}

TEST_CASE("DatumTypeTest - empty list has zero elements") {
    list_t list{};
    datum d{datum_f{list}};
    REQUIRE(std::holds_alternative<list_t>(d.inner));
    REQUIRE(std::get<list_t>(d.inner).elements.size() == 0);
}

// -- quote wraps a handle to the quoted datum -------------------------------

TEST_CASE("DatumTypeTest - quote wraps a handle to the quoted datum") {
    smdscheme::foundation::tree_arena<datum, MaxNodes> arena;
    auto box = smdscheme::foundation::make_arena_box(
        arena, datum{datum_f{datum_integer{7}}});
    datum d{datum_f{quote_t{box}}};
    REQUIRE(std::holds_alternative<quote_t>(d.inner));
    auto const &quoted = arena.get(std::get<quote_t>(d.inner).quoted);
    REQUIRE(std::get<datum_integer>(quoted.inner).value == 7);
}

// -- function-quote is a distinct kind from quote and from list (L6 sketch) -

TEST_CASE("DatumTypeTest - function-quote wraps a handle, distinct from "
          "quote and list") {
    smdscheme::foundation::tree_arena<datum, MaxNodes> arena;
    auto box = smdscheme::foundation::make_arena_box(
        arena, datum{datum_f{datum_symbol{}}});
    datum d{datum_f{function_t{box}}};
    REQUIRE(std::holds_alternative<function_t>(d.inner));
    REQUIRE_FALSE(std::holds_alternative<quote_t>(d.inner));
    REQUIRE_FALSE(std::holds_alternative<list_t>(d.inner));
}

// -- backquote/unquote/unquote-splice are distinct kinds (step L18) --------

TEST_CASE("DatumTypeTest - backquote wraps a handle to its template") {
    smdscheme::foundation::tree_arena<datum, MaxNodes> arena;
    auto box = smdscheme::foundation::make_arena_box(
        arena, datum{datum_f{datum_integer{9}}});
    datum d{datum_f{backquote_t{box}}};
    REQUIRE(std::holds_alternative<backquote_t>(d.inner));
    auto const &templ = arena.get(std::get<backquote_t>(d.inner).templ);
    REQUIRE(std::get<datum_integer>(templ.inner).value == 9);
}

TEST_CASE("DatumTypeTest - unquote and unquote-splice are distinct from each "
          "other and from backquote") {
    smdscheme::foundation::tree_arena<datum, MaxNodes> arena;
    auto box = smdscheme::foundation::make_arena_box(
        arena, datum{datum_f{datum_symbol{}}});
    datum uq{datum_f{unquote_t{box}}};
    datum sp{datum_f{unquote_splice_t{box}}};
    REQUIRE(std::holds_alternative<unquote_t>(uq.inner));
    REQUIRE(std::holds_alternative<unquote_splice_t>(sp.inner));
    REQUIRE(uq.inner.index() != sp.inner.index());
    REQUIRE_FALSE(std::holds_alternative<backquote_t>(uq.inner));
}

} // namespace smd::smdlisp::reader
