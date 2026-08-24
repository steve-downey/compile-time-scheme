// src/smd/kit/foundation/source_span.test.cpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Moved in step R8 from src/smd/cl/foundation/source_span.test.cpp, itself
// the reviewed union of two prior copies of this test:
// src/smd/smdscheme/foundation/source_span.test.cpp (compile-time-scheme, at
// iteration/smdscheme-final) and src/smd/forth/foundation/source_span.test.cpp
// (compile-time-forth).

#include <smd/kit/foundation/source_span.hpp>
#include <smd/kit/foundation/source_span.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::kit::foundation::source_pos;
using smd::kit::foundation::source_span;

static_assert(source_span{} == source_span{});
static_assert(source_span{source_pos{0, 1, 1}, source_pos{3, 1, 4}} ==
              source_span{source_pos{0, 1, 1}, source_pos{3, 1, 4}});

TEST_CASE("SourceSpanTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("SourceSpanTest - DefaultConstruction") {
    constexpr source_span span{};
    CHECK(span.first == source_pos{});
    CHECK(span.last == source_pos{});
}

TEST_CASE("SourceSpanTest - Equality") {
    constexpr source_span s{source_pos{0, 1, 1}, source_pos{3, 1, 4}};
    constexpr source_span t{source_pos{0, 1, 1}, source_pos{3, 1, 4}};
    constexpr source_span u{source_pos{0, 1, 1}, source_pos{5, 1, 6}};
    CHECK(s == t);
    CHECK(!(s == u));
}
