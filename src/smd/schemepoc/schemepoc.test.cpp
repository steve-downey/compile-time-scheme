// src/smd/schemepoc/schemepoc.test.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/schemepoc.hpp>

#include <smd/schemepoc/schemepoc.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

// 03013d1f-bcc1-4d3e-9701-3ed1a15c6370
TEST_CASE("SchemepocTest - HeaderIsIdempotent") { REQUIRE(true); }

static_assert([] {
    auto env0 =
        smd::schemepoc::default_env<smd::schemepoc::core_type<32, 16>, 16>();
    auto r = smd::schemepoc::compiled_closure<"(+ 1 (* 2 3))">(env0);
    return r.has_value() && std::get<int>(r.value()) == 7;
}());
// 03013d1f-bcc1-4d3e-9701-3ed1a15c6370 end
