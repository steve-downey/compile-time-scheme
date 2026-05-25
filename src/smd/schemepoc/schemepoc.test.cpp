// schemepoc/schemepoc.test.cpp -*-C++-*- SPDX-License-Identifier: Apache-2.0
// WITH LLVM-exception

#include <smd/schemepoc/schemepoc.hpp>

#include <smd/schemepoc/schemepoc.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Test Testing", "") { REQUIRE(true); }

// 03013d1f-bcc1-4d3e-9701-3ed1a15c6370
TEST_CASE("Test Name Steve", "") { REQUIRE(schemepoc::schemepoc() == "Steve"); }
// 03013d1f-bcc1-4d3e-9701-3ed1a15c6370 end
