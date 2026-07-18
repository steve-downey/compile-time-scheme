// src/smd/smdlisp/reader/atom.test.cpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/reader/atom.hpp>
#include <smd/smdlisp/reader/atom.hpp> // test 2nd include OK

#include <smd/smdscheme/parser/cursor.hpp>

#include <catch2/catch_test_macros.hpp>

namespace smd::smdlisp::reader {

// -- integer_p ------------------------------------------------------------

static_assert([] {
    auto r = integer_p()(smdscheme::parser::cursor{"42"});
    return r.has_value() && r.value().value.value == 42;
}());

static_assert([] {
    auto r = integer_p()(smdscheme::parser::cursor{"-7"});
    return r.has_value() && r.value().value.value == -7;
}());

static_assert([] {
    auto r = integer_p()(smdscheme::parser::cursor{"abc"});
    return !r.has_value();
}());

// -- symbol_p: folding (decision D2) ---------------------------------------

static_assert([] {
    auto r = symbol_p()(smdscheme::parser::cursor{"foo"});
    return r.has_value() && r.value().value.name.view() == "FOO";
}());

static_assert([] {
    auto r = symbol_p()(smdscheme::parser::cursor{"Foo123"});
    return r.has_value() && r.value().value.name.view() == "FOO123";
}());

static_assert([] {
    auto r = symbol_p()(smdscheme::parser::cursor{"+"});
    return r.has_value() && r.value().value.name.view() == "+";
}());

// -- keyword_p: distinct kind, colon stripped, folded ----------------------

static_assert([] {
    auto r = keyword_p()(smdscheme::parser::cursor{":bar"});
    return r.has_value() && r.value().value.name.view() == "BAR";
}());

static_assert([] {
    auto r = keyword_p()(smdscheme::parser::cursor{":Baz"});
    return r.has_value() && r.value().value.name.view() == "BAZ";
}());

static_assert([] {
    // A bare colon with nothing after it is not a well-formed keyword.
    auto r = keyword_p()(smdscheme::parser::cursor{":"});
    return !r.has_value();
}());

static_assert([] {
    // symbol_p must not accept the keyword-shaped ":bar" itself as the
    // *symbol* "BAR"; a leading colon is consumed as part of the name (":"
    // is not a terminating macro character), so it reads as a different
    // symbol than the keyword.
    auto r = symbol_p()(smdscheme::parser::cursor{":bar"});
    return r.has_value() && r.value().value.name.view() == ":BAR";
}());

// -- atom_p / read_atom: dispatch and D7 kind distinction ------------------

static_assert([] {
    auto r = read_atom("foo");
    return r.has_value() && std::holds_alternative<atom_symbol>(r.value().value) &&
           std::get<atom_symbol>(r.value().value).name.view() == "FOO";
}());

static_assert([] {
    auto r = read_atom(":bar");
    return r.has_value() &&
           std::holds_alternative<atom_keyword>(r.value().value) &&
           std::get<atom_keyword>(r.value().value).name.view() == "BAR";
}());

static_assert([] {
    auto r = read_atom("42");
    return r.has_value() &&
           std::holds_alternative<atom_integer>(r.value().value) &&
           std::get<atom_integer>(r.value().value).value == 42;
}());

static_assert([] {
    // A digit-led spelling is claimed by the integer alternative, not read
    // as a symbol, when it is entirely digits.
    auto r = read_atom("-7");
    return r.has_value() &&
           std::holds_alternative<atom_integer>(r.value().value) &&
           std::get<atom_integer>(r.value().value).value == -7;
}());

static_assert([] {
    // A digit-led spelling that is not entirely digits reads as a symbol
    // (CL allows symbols like `1+`; ANSI's fuller "potential number" syntax
    // is out of scope for this reader, decision D10).
    auto r = read_atom("1+");
    return r.has_value() &&
           std::holds_alternative<atom_symbol>(r.value().value) &&
           std::get<atom_symbol>(r.value().value).name.view() == "1+";
}());

static_assert([] {
    // A lone minus sign is not "one or more digits"; it reads as the
    // symbol `-`, not a truncated/incomplete integer.
    auto r = read_atom("-");
    return r.has_value() &&
           std::holds_alternative<atom_symbol>(r.value().value) &&
           std::get<atom_symbol>(r.value().value).name.view() == "-";
}());

// -- t / nil read as ordinary symbols (no special-casing here) -------------

static_assert([] {
    auto r = read_atom("t");
    return r.has_value() &&
           std::holds_alternative<atom_symbol>(r.value().value) &&
           std::get<atom_symbol>(r.value().value).name.view() == "T";
}());

static_assert([] {
    auto r = read_atom("nil");
    return r.has_value() &&
           std::holds_alternative<atom_symbol>(r.value().value) &&
           std::get<atom_symbol>(r.value().value).name.view() == "NIL";
}());

// -- malformed / error cases, matching the Scheme atom reader's discipline -

static_assert([] {
    auto r = read_atom("");
    return !r.has_value();
}());

static_assert([] {
    // A lone terminating macro character is not an atom at all.
    auto r = read_atom("(");
    return !r.has_value();
}());

TEST_CASE("AtomTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("AtomTest - integer_p parses positive") {
    auto r = integer_p()(smdscheme::parser::cursor{"123"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.value == 123);
}

TEST_CASE("AtomTest - integer_p parses negative") {
    auto r = integer_p()(smdscheme::parser::cursor{"-42"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.value == -42);
}

TEST_CASE("AtomTest - integer_p fails on non-digit") {
    auto r = integer_p()(smdscheme::parser::cursor{"abc"});
    REQUIRE(!r.has_value());
}

TEST_CASE("AtomTest - integer_p zero") {
    auto r = integer_p()(smdscheme::parser::cursor{"0"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.value == 0);
}

TEST_CASE("AtomTest - integer_p stops at delimiter") {
    auto r = integer_p()(smdscheme::parser::cursor{"42 rest"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.value == 42);
    REQUIRE(r.value().rest.remaining() == " rest");
}

TEST_CASE("AtomTest - symbol_p folds lowercase to uppercase") {
    auto r = symbol_p()(smdscheme::parser::cursor{"foo"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name.view() == "FOO");
}

TEST_CASE("AtomTest - symbol_p folds mixed case") {
    auto r = symbol_p()(smdscheme::parser::cursor{"FooBar"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name.view() == "FOOBAR");
}

TEST_CASE("AtomTest - symbol_p is already-uppercase idempotent") {
    auto r = symbol_p()(smdscheme::parser::cursor{"FOO"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name.view() == "FOO");
}

TEST_CASE("AtomTest - symbol_p parses operator") {
    auto r = symbol_p()(smdscheme::parser::cursor{"+"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name.view() == "+");
}

TEST_CASE("AtomTest - symbol_p allows digit-led spelling like 1+") {
    auto r = symbol_p()(smdscheme::parser::cursor{"1+"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name.view() == "1+");
}

TEST_CASE("AtomTest - symbol_p stops at delimiter") {
    auto r = symbol_p()(smdscheme::parser::cursor{"foo bar"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name.view() == "FOO");
    REQUIRE(r.value().rest.remaining() == " bar");
}

TEST_CASE("AtomTest - symbol_p stops at open paren") {
    auto r = symbol_p()(smdscheme::parser::cursor{"foo(bar)"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name.view() == "FOO");
    REQUIRE(r.value().rest.remaining() == "(bar)");
}

TEST_CASE("AtomTest - keyword_p strips colon and folds") {
    auto r = keyword_p()(smdscheme::parser::cursor{":bar"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name.view() == "BAR");
}

TEST_CASE("AtomTest - keyword_p folds mixed case") {
    auto r = keyword_p()(smdscheme::parser::cursor{":FooBar"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name.view() == "FOOBAR");
}

TEST_CASE("AtomTest - keyword_p fails without a colon") {
    auto r = keyword_p()(smdscheme::parser::cursor{"bar"});
    REQUIRE(!r.has_value());
}

TEST_CASE("AtomTest - keyword_p fails on colon alone") {
    auto r = keyword_p()(smdscheme::parser::cursor{":"});
    REQUIRE(!r.has_value());
}

TEST_CASE("AtomTest - keyword_p stops at delimiter") {
    auto r = keyword_p()(smdscheme::parser::cursor{":bar baz"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name.view() == "BAR");
    REQUIRE(r.value().rest.remaining() == " baz");
}

TEST_CASE("AtomTest - read_atom yields a symbol for foo") {
    auto r = read_atom("foo");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<atom_symbol>(r.value().value));
    REQUIRE(std::get<atom_symbol>(r.value().value).name.view() == "FOO");
}

TEST_CASE("AtomTest - read_atom yields a keyword for :bar") {
    auto r = read_atom(":bar");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<atom_keyword>(r.value().value));
    REQUIRE(std::get<atom_keyword>(r.value().value).name.view() == "BAR");
}

TEST_CASE("AtomTest - read_atom yields an integer for 42") {
    auto r = read_atom("42");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<atom_integer>(r.value().value));
    REQUIRE(std::get<atom_integer>(r.value().value).value == 42);
}

TEST_CASE("AtomTest - read_atom yields a symbol for negative-looking non-number -foo") {
    auto r = read_atom("-foo");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<atom_symbol>(r.value().value));
    REQUIRE(std::get<atom_symbol>(r.value().value).name.view() == "-FOO");
}

TEST_CASE("AtomTest - a keyword and a symbol with the same letters are distinct kinds (D7)") {
    auto sym = read_atom("bar");
    auto kw = read_atom(":bar");
    REQUIRE(sym.has_value());
    REQUIRE(kw.has_value());
    REQUIRE(std::holds_alternative<atom_symbol>(sym.value().value));
    REQUIRE(std::holds_alternative<atom_keyword>(kw.value().value));
    REQUIRE_FALSE(sym.value().value.index() == kw.value().value.index());
}

TEST_CASE("AtomTest - t reads as an ordinary symbol") {
    auto r = read_atom("t");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<atom_symbol>(r.value().value));
    REQUIRE(std::get<atom_symbol>(r.value().value).name.view() == "T");
}

TEST_CASE("AtomTest - nil reads as an ordinary symbol") {
    auto r = read_atom("nil");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<atom_symbol>(r.value().value));
    REQUIRE(std::get<atom_symbol>(r.value().value).name.view() == "NIL");
}

TEST_CASE("AtomTest - read_atom fails on empty input") {
    auto r = read_atom("");
    REQUIRE(!r.has_value());
}

TEST_CASE("AtomTest - read_atom fails on a lone terminating macro character") {
    auto r = read_atom(")");
    REQUIRE(!r.has_value());
}

TEST_CASE("AtomTest - a lone minus sign reads as the symbol -, not an "
          "incomplete integer") {
    auto r = read_atom("-");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<atom_symbol>(r.value().value));
    REQUIRE(std::get<atom_symbol>(r.value().value).name.view() == "-");
}

} // namespace smd::smdlisp::reader
