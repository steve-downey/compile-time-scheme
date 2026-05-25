// src/smd/schemepoc/schemepoc.test.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/schemepoc.hpp>

#include <smd/schemepoc/schemepoc.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <variant>

// 03013d1f-bcc1-4d3e-9701-3ed1a15c6370
TEST_CASE("SchemepocTest - HeaderIsIdempotent") { REQUIRE(true); }
// 03013d1f-bcc1-4d3e-9701-3ed1a15c6370 end

// 7a2f1d3e-9b4c-4a8d-8e2f-1c3d5b7a9f2e
static_assert([] {
    auto env0 = smd::schemepoc::default_env<16>();
    auto r = smd::schemepoc::compiled_closure<"(+ 1 2)">(env0);
    return r.has_value() && std::get<int>(r.value()) == 3;
}());

static_assert([] {
    auto env0 = smd::schemepoc::default_env<16>();
    auto r = smd::schemepoc::compiled_closure<"(+ 1 (* 2 3))">(env0);
    return r.has_value() && std::get<int>(r.value()) == 7;
}());

static_assert([] {
    auto env0 = smd::schemepoc::default_env<16>();
    auto r = smd::schemepoc::compiled_closure<"(if #t 42 0)">(env0);
    return r.has_value() && std::get<int>(r.value()) == 42;
}());
// 7a2f1d3e-9b4c-4a8d-8e2f-1c3d5b7a9f2e end

TEST_CASE("SchemepocTest - CompiledClosureAddition") {
    auto env0 = smd::schemepoc::default_env<16>();
    auto r = smd::schemepoc::compiled_closure<"(+ 1 2)">(env0);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("SchemepocTest - CompiledClosureNestedArithmetic") {
    auto env0 = smd::schemepoc::default_env<16>();
    auto r = smd::schemepoc::compiled_closure<"(+ 1 (* 2 3))">(env0);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("SchemepocTest - CompiledClosureIf") {
    auto env0 = smd::schemepoc::default_env<16>();
    auto r = smd::schemepoc::compiled_closure<"(if #t 42 0)">(env0);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 42);
}
