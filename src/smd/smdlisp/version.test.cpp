// src/smd/smdlisp/version.test.cpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/version.hpp>
#include <smd/smdlisp/version.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

TEST_CASE("VersionTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("VersionTest - VersionIsAvailable") {
    CHECK(smd::smdlisp::version_major == 0);
    CHECK(smd::smdlisp::version_minor == 1);
    CHECK(smd::smdlisp::version_patch == 0);
}

TEST_CASE("VersionTest - LinksSmdschemeFoundationAndParser") {
    CHECK(smd::smdlisp::links_smdscheme_foundation());
    CHECK(smd::smdlisp::links_smdscheme_parser());
}

static_assert(smd::smdlisp::version_major == 0);
static_assert(smd::smdlisp::version_minor == 1);
static_assert(smd::smdlisp::version_patch == 0);
static_assert(smd::smdlisp::links_smdscheme_foundation());
static_assert(smd::smdlisp::links_smdscheme_parser());
