// src/smd/schemepoc/result.test.cpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/result.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::smdscheme::foundation::parse_error;
using smd::smdscheme::foundation::result;
using smd::smdscheme::foundation::source_pos;

static_assert(result<int>{42}.has_value());
static_assert(result<int>{42}.value() == 42);
static_assert(!result<int>{parse_error{}}.has_value());

TEST_CASE("ResultTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ResultTest - ConstructFromValue") {
    constexpr result<int> r{7};
    CHECK(r.has_value());
    CHECK(r.value() == 7);
}

TEST_CASE("ResultTest - ConstructFromError") {
    constexpr parse_error err{source_pos{3, 1, 4}, "bad"};
    constexpr result<int> r{err};
    CHECK(!r.has_value());
    CHECK(r.error() == err);
}

TEST_CASE("ResultTest - ValueTypePreservation") {
    constexpr result<bool> r{true};
    CHECK(r.has_value());
    CHECK(r.value() == true);
}
