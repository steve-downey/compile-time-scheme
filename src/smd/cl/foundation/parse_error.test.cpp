// src/smd/cl/foundation/parse_error.test.cpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Reviewed union of the two prior copies of this test:
// src/smd/smdscheme/foundation/parse_error.test.cpp (compile-time-scheme) and
// src/smd/forth/foundation/parse_error.test.cpp (compile-time-forth).

#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/parse_error.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::parse_error;
using smd::cl::foundation::source_pos;

static_assert(parse_error{} == parse_error{});
static_assert(parse_error{source_pos{1, 1, 2}, "oops"} ==
              parse_error{source_pos{1, 1, 2}, "oops"});

TEST_CASE("ParseErrorTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ParseErrorTest - DefaultConstruction") {
    constexpr parse_error e{};
    CHECK(e.where == source_pos{});
    CHECK(e.message == nullptr);
}

TEST_CASE("ParseErrorTest - Equality") {
    constexpr parse_error e1{source_pos{0, 1, 1}, "test"};
    constexpr parse_error e2{source_pos{0, 1, 1}, "test"};
    constexpr parse_error e3{source_pos{1, 1, 2}, "other"};
    CHECK(e1 == e2);
    CHECK(!(e1 == e3));
}

TEST_CASE("ParseErrorTest - NullMessage") {
    constexpr parse_error e1{source_pos{}, nullptr};
    constexpr parse_error e2{source_pos{}, nullptr};
    CHECK(e1 == e2);
}

TEST_CASE("ParseErrorTest - DifferentMessages") {
    constexpr parse_error e1{source_pos{}, "abc"};
    constexpr parse_error e2{source_pos{}, "xyz"};
    CHECK(!(e1 == e2));
}
