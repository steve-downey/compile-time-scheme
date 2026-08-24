// src/smd/kit/foundation/source_pos.test.cpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Moved in step R8 from src/smd/cl/foundation/source_pos.test.cpp, itself
// the reviewed union of two prior copies of this test:
// src/smd/smdscheme/foundation/source_pos.test.cpp (compile-time-scheme, at
// iteration/smdscheme-final) and src/smd/forth/foundation/source_pos.test.cpp
// (compile-time-forth).

#include <smd/kit/foundation/source_pos.hpp>
#include <smd/kit/foundation/source_pos.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::kit::foundation::source_pos;

static_assert(source_pos{} == source_pos{0, 1, 1});
static_assert(source_pos{5, 2, 3} == source_pos{5, 2, 3});
static_assert(!(source_pos{0, 1, 1} == source_pos{1, 1, 2}));

TEST_CASE("SourcePosTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("SourcePosTest - DefaultConstruction") {
    constexpr source_pos pos{};
    CHECK(pos.offset == 0);
    CHECK(pos.line == 1);
    CHECK(pos.column == 1);
}

TEST_CASE("SourcePosTest - Equality") {
    constexpr source_pos a{5, 2, 3};
    constexpr source_pos b{5, 2, 3};
    constexpr source_pos c{6, 2, 4};
    CHECK(a == b);
    CHECK(!(a == c));
}
