// src/smd/schemepoc/reader_atom.test.cpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/reader_atom.hpp>
#include <smd/schemepoc/reader_atom.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

namespace smd::schemepoc {

static_assert([] {
    auto r = integer_p()(cursor{"42"});
    return r.has_value() && r.value().value.value == 42;
}());

static_assert([] {
    auto r = integer_p()(cursor{"-7"});
    return r.has_value() && r.value().value.value == -7;
}());

static_assert([] {
    auto r = integer_p()(cursor{"abc"});
    return !r.has_value();
}());

static_assert([] {
    auto r = symbol_p()(cursor{"foo"});
    return r.has_value() && r.value().value.name == "foo";
}());

static_assert([] {
    auto r = symbol_p()(cursor{"+"});
    return r.has_value() && r.value().value.name == "+";
}());

static_assert([] {
    auto r = symbol_p()(cursor{"42"});
    return !r.has_value();
}());

TEST_CASE("AtomTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("AtomTest - integer_p parses positive") {
    auto r = integer_p()(cursor{"123"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.value == 123);
}

TEST_CASE("AtomTest - integer_p parses negative") {
    auto r = integer_p()(cursor{"-42"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.value == -42);
}

TEST_CASE("AtomTest - integer_p fails on non-digit") {
    auto r = integer_p()(cursor{"abc"});
    REQUIRE(!r.has_value());
}

TEST_CASE("AtomTest - integer_p zero") {
    auto r = integer_p()(cursor{"0"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.value == 0);
}

TEST_CASE("AtomTest - symbol_p parses alphabetic") {
    auto r = symbol_p()(cursor{"foo"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name == "foo");
}

TEST_CASE("AtomTest - symbol_p parses operator") {
    auto r = symbol_p()(cursor{"+"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name == "+");
}

TEST_CASE("AtomTest - symbol_p parses mixed") {
    auto r = symbol_p()(cursor{"foo123"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name == "foo123");
}

TEST_CASE("AtomTest - symbol_p fails on digit start") {
    auto r = symbol_p()(cursor{"42"});
    REQUIRE(!r.has_value());
}

TEST_CASE("AtomTest - integer_p stops at delimiter") {
    auto r = integer_p()(cursor{"42 rest"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.value == 42);
    REQUIRE(r.value().rest.remaining() == " rest");
}

TEST_CASE("AtomTest - symbol_p stops at delimiter") {
    auto r = symbol_p()(cursor{"foo bar"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value.name == "foo");
    REQUIRE(r.value().rest.remaining() == " bar");
}

} // namespace smd::schemepoc
