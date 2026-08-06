// src/smd/cl/symbol/symbol_id.test.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/symbol/symbol_id.hpp>
#include <smd/cl/symbol/symbol_id.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::cl::symbol::symbol_id;

TEST_CASE("SymbolIdTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("SymbolIdTest - DefaultConstructedIsInvalid") {
    symbol_id const id{};
    CHECK(!id.valid());
    CHECK(id.index() == -1);
}

TEST_CASE("SymbolIdTest - ConstructedFromIndexIsValid") {
    symbol_id const id{3};
    CHECK(id.valid());
    CHECK(id.index() == 3);
}

TEST_CASE("SymbolIdTest - EqualityIsIdentity") {
    CHECK(symbol_id{2} == symbol_id{2});
    CHECK(symbol_id{2} != symbol_id{3});
    CHECK(symbol_id{} == symbol_id{});
    CHECK(symbol_id{} != symbol_id{0});
}

// Compile-time twins: the constexpr contract is part of the interface.
static_assert(!symbol_id{}.valid());
static_assert(symbol_id{}.index() == -1);
static_assert(symbol_id{3}.valid());
static_assert(symbol_id{3}.index() == 3);
static_assert(symbol_id{2} == symbol_id{2});
static_assert(symbol_id{2} != symbol_id{3});
static_assert(symbol_id{} != symbol_id{0});
