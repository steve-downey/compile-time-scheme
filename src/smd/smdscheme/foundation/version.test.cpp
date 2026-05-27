// src/smd/smdscheme/foundation/version.test.cpp                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/foundation/version.hpp>
#include <smd/smdscheme/foundation/version.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

TEST_CASE("VersionTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("VersionTest - VersionIsAvailable") {
    CHECK(smd::smdscheme::foundation::version_major == 0);
    CHECK(smd::smdscheme::foundation::version_minor == 1);
    CHECK(smd::smdscheme::foundation::version_patch == 0);
}

static_assert(smd::smdscheme::foundation::version_major == 0);
static_assert(smd::smdscheme::foundation::version_minor == 1);
static_assert(smd::smdscheme::foundation::version_patch == 0);
