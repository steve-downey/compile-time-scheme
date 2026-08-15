// src/smd/cl/foundation/source_span.test.cpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test (step R8): the substantive source_span tests moved to
// src/smd/kit/foundation/source_span.test.cpp with the definition. This
// file only verifies that the forwarding shim compiles and that the
// forwarded name is usable from smd::cl::foundation exactly as before.

#include <smd/cl/foundation/source_span.hpp>
#include <smd/cl/foundation/source_span.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::source_pos;
using smd::cl::foundation::source_span;

static_assert(source_span{} == source_span{});

TEST_CASE("SourceSpanShimTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("SourceSpanShimTest - EqualityThroughShim") {
    constexpr source_span s{source_pos{0, 1, 1}, source_pos{3, 1, 4}};
    constexpr source_span t{source_pos{0, 1, 1}, source_pos{3, 1, 4}};
    CHECK(s == t);
}
