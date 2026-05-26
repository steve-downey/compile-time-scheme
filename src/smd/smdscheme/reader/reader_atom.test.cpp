// src/smd/schemepoc/reader_atom.test.cpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/reader/reader_atom.hpp>
#include <smd/smdscheme/reader/reader_atom.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

namespace smd::smdscheme::reader {

static_assert([] {
    auto r = integer_p()(parser::cursor{"42"});
    return r.has_value() && r.value().value.value == 42;
}());

static_assert([] {
    auto r = integer_p()(parser::cursor{"-7"});
    return r.has_value() && r.value().value.value == -7;
}());

static_assert([] {
    auto r = integer_p()(parser::cursor{"abc"});
    return !r.has_value();
}());

static_assert([] {
    auto r = symbol_p()(parser::cursor{"foo"});
    return r.has_value() && r.value().value.name == "foo";
}());

static_assert([] {
    auto r = symbol_p()(parser::cursor{"+"});
    return r.has_value() && r.value().value.name == "+";
}());

static_assert([] {
    auto r = symbol_p()(parser::cursor{"42"});
    return !r.has_value();
}());

TEST_CASE("AtomTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("AtomTest - integer_p parses positive") {
    auto r = integer_p()(parser::cursor{"123"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.value == 123);
}

TEST_CASE("AtomTest - integer_p parses negative") {
    auto r = integer_p()(parser::cursor{"-42"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.value == -42);
}

TEST_CASE("AtomTest - integer_p fails on non-digit") {
    auto r = integer_p()(parser::cursor{"abc"});
    REQUIRE(!r.has_value());
}

TEST_CASE("AtomTest - integer_p zero") {
    auto r = integer_p()(parser::cursor{"0"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.value == 0);
}

TEST_CASE("AtomTest - symbol_p parses alphabetic") {
    auto r = symbol_p()(parser::cursor{"foo"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name == "foo");
}

TEST_CASE("AtomTest - symbol_p parses operator") {
    auto r = symbol_p()(parser::cursor{"+"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name == "+");
}

TEST_CASE("AtomTest - symbol_p parses mixed") {
    auto r = symbol_p()(parser::cursor{"foo123"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name == "foo123");
}

TEST_CASE("AtomTest - symbol_p fails on digit start") {
    auto r = symbol_p()(parser::cursor{"42"});
    REQUIRE(!r.has_value());
}

TEST_CASE("AtomTest - integer_p stops at delimiter") {
    auto r = integer_p()(parser::cursor{"42 rest"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.value == 42);
    REQUIRE(r.value().rest.remaining() == " rest");
}

TEST_CASE("AtomTest - symbol_p stops at delimiter") {
    auto r = symbol_p()(parser::cursor{"foo bar"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name == "foo");
    REQUIRE(r.value().rest.remaining() == " bar");
}

} // namespace smd::smdscheme::reader
