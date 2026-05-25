// src/smd/schemepoc/version.test.cpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/version.hpp>
#include <smd/schemepoc/version.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

TEST_CASE("VersionTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("VersionTest - VersionIsAvailable") {
    CHECK(smd::schemepoc::version_major == 0);
    CHECK(smd::schemepoc::version_minor == 1);
    CHECK(smd::schemepoc::version_patch == 0);
}

static_assert(smd::schemepoc::version_major == 0);
static_assert(smd::schemepoc::version_minor == 1);
static_assert(smd::schemepoc::version_patch == 0);
