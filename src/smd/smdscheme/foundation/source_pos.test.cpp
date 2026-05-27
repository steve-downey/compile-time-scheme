// src/smd/smdscheme/foundation/source_pos.test.cpp              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/foundation/source_pos.hpp>
#include <smd/smdscheme/foundation/source_pos.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::smdscheme::foundation::source_pos;

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
