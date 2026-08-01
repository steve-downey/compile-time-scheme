// src/smd/cl/symbol/symbol_table.test.cpp                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/symbol/symbol_table.hpp>
#include <smd/cl/symbol/symbol_table.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <optional>
#include <string_view>

namespace {

// Three deliberately distinct slot types, so a slot written into the wrong
// slot would fail to compile rather than silently pass.
struct value_slot {
    int payload{};
    friend constexpr auto operator==(value_slot, value_slot) -> bool = default;
};

struct function_slot {
    int payload{};
    friend constexpr auto operator==(function_slot, function_slot)
        -> bool = default;
};

struct macro_slot {
    int payload{};
    friend constexpr auto operator==(macro_slot, macro_slot) -> bool = default;
};

using table =
    smd::cl::symbol::symbol_table<value_slot, function_slot, macro_slot, 8, 64>;

} // namespace

TEST_CASE("SymbolTableTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("SymbolTableTest - EmptyTable") {
    table const t;
    CHECK(t.size() == 0);
    CHECK(t.capacity() == 8);
    CHECK(t.name_chars_used() == 0);
    CHECK(t.name_chars_capacity() == 64);
    CHECK(!t.find("nil").has_value());
}

TEST_CASE("SymbolTableTest - InterningTwiceYieldsSameId") {
    table t;
    auto const a = t.intern("cons");
    auto const b = t.intern("cons");
    CHECK(a == b);
    CHECK(t.size() == 1);
    CHECK(t.name_chars_used() == 4);
}

TEST_CASE("SymbolTableTest - DistinctNamesYieldDistinctIds") {
    table t;
    auto const a = t.intern("car");
    auto const b = t.intern("cdr");
    CHECK(a != b);
    CHECK(t.size() == 2);
}

TEST_CASE("SymbolTableTest - FindReturnsTheInternedId") {
    table t;
    auto const a = t.intern("lambda");
    auto const found = t.find("lambda");
    REQUIRE(found.has_value());
    CHECK(*found == a);
    CHECK(!t.find("lambd").has_value());
}

TEST_CASE("SymbolTableTest - NameRecoversTheInternedSpelling") {
    table t;
    auto const a = t.intern("quote");
    CHECK(t.name(a) == "quote");
    CHECK(t.is_interned(a));
}

TEST_CASE("SymbolTableTest - NamesAreCopiedNotViewed") {
    // D12's self-containedness: the table owns its characters, so mutating
    // the caller's buffer after interning must not change the symbol's name.
    std::array<char, 3> buf{'f', 'o', 'o'};
    table t;
    auto const id = t.intern(std::string_view{buf.data(), buf.size()});
    buf[0] = 'z';
    CHECK(t.name(id) == "foo");
}

TEST_CASE("SymbolTableTest - UninternedSymbolIsNeverFoundByName") {
    table t;
    auto const g = t.make_uninterned("g1");
    CHECK(!t.find("g1").has_value());
    CHECK(t.name(g) == "g1");
    CHECK(!t.is_interned(g));

    // Interning the same spelling afterwards yields a different symbol.
    auto const i = t.intern("g1");
    CHECK(i != g);
    REQUIRE(t.find("g1").has_value());
    CHECK(*t.find("g1") == i);

    // Two uninterned symbols with the same name are distinct.
    CHECK(t.make_uninterned("g1") != g);
}

TEST_CASE("SymbolTableTest - SlotsStartUnbound") {
    table t;
    auto const a = t.intern("x");
    CHECK(!t.value(a).has_value());
    CHECK(!t.function(a).has_value());
    CHECK(!t.macro(a).has_value());
}

TEST_CASE("SymbolTableTest - SlotsAreIndependentlyWritable") {
    table t;
    auto const a = t.intern("x");
    auto const b = t.intern("y");

    t.set_value(a, value_slot{1});
    CHECK(t.value(a) == std::optional{value_slot{1}});
    CHECK(!t.function(a).has_value());
    CHECK(!t.macro(a).has_value());
    CHECK(!t.value(b).has_value());

    t.set_function(a, function_slot{2});
    t.set_macro(a, macro_slot{3});
    CHECK(t.value(a) == std::optional{value_slot{1}});
    CHECK(t.function(a) == std::optional{function_slot{2}});
    CHECK(t.macro(a) == std::optional{macro_slot{3}});

    // Overwriting one slot leaves the others alone.
    t.set_value(a, value_slot{4});
    CHECK(t.value(a) == std::optional{value_slot{4}});
    CHECK(t.function(a) == std::optional{function_slot{2}});
    CHECK(t.macro(a) == std::optional{macro_slot{3}});
}

// Compile-time twins: interning, identity, lookup, self-containment, and the
// slots are all part of the constexpr contract.

static_assert([] {
    table t;
    return t.intern("cons") == t.intern("cons");
}());

static_assert([] {
    table t;
    return t.intern("car") != t.intern("cdr");
}());

static_assert([] {
    table t;
    auto const a = t.intern("lambda");
    return t.find("lambda") == std::optional{a} && !t.find("lambd").has_value();
}());

static_assert([] {
    std::array<char, 3> buf{'f', 'o', 'o'};
    table t;
    auto const id = t.intern(std::string_view{buf.data(), buf.size()});
    buf[0] = 'z';
    return t.name(id) == std::string_view{"foo"};
}());

static_assert([] {
    table t;
    auto const g = t.make_uninterned("g1");
    return !t.find("g1").has_value() && t.name(g) == std::string_view{"g1"} &&
           !t.is_interned(g) && t.intern("g1") != g;
}());

static_assert([] {
    table t;
    auto const a = t.intern("x");
    if (t.value(a).has_value() || t.function(a).has_value() ||
        t.macro(a).has_value()) {
        return false;
    }
    t.set_value(a, value_slot{1});
    t.set_function(a, function_slot{2});
    t.set_macro(a, macro_slot{3});
    return t.value(a) == std::optional{value_slot{1}} &&
           t.function(a) == std::optional{function_slot{2}} &&
           t.macro(a) == std::optional{macro_slot{3}};
}());
