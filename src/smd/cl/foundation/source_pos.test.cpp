// src/smd/cl/foundation/source_pos.test.cpp                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test (step R8): the substantive source_pos tests moved to
// src/smd/kit/foundation/source_pos.test.cpp with the definition. This file
// only verifies that the forwarding shim compiles and that the forwarded
// name is usable from smd::cl::foundation exactly as before.

#include <smd/cl/foundation/source_pos.hpp>
#include <smd/cl/foundation/source_pos.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::source_pos;

static_assert(source_pos{} == source_pos{0, 1, 1});

TEST_CASE("SourcePosShimTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("SourcePosShimTest - EqualityThroughShim") {
    constexpr source_pos a{5, 2, 3};
    constexpr source_pos b{5, 2, 3};
    CHECK(a == b);
}
