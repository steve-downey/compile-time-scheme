// src/smd/cl/foundation/parse_error.test.cpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test (step R8): the substantive parse_error equality tests
// moved to src/smd/kit/foundation/parse_error.test.cpp with the
// definition. This file only verifies that the forwarding shim compiles
// and that the forwarded name is usable from smd::cl::foundation exactly
// as before.

#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/parse_error.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::parse_error;
using smd::cl::foundation::source_pos;

static_assert(parse_error{} == parse_error{});

TEST_CASE("ParseErrorShimTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ParseErrorShimTest - EqualityThroughShim") {
    constexpr parse_error e1{source_pos{0, 1, 1}, "test"};
    constexpr parse_error e2{source_pos{0, 1, 1}, "test"};
    constexpr parse_error e3{source_pos{1, 1, 2}, "other"};
    CHECK(e1 == e2);
    CHECK(!(e1 == e3));
}
