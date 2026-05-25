#if 0
// src/smd/schemepoc/closure_backend.test.cpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/closure_backend.hpp>
#include <smd/schemepoc/closure_backend.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
using namespace smd::schemepoc;
using namespace std::string_view_literals;

// "(+ 1 (* 2 3))" -> value<core_type<32, 16>>{7}
static_assert([] {
    auto r = compile_to_closure("(+ 1 (* 2 3))"sv);
    if (!r.has_value())
        return false;
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto vr = r.value()(env0);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 7;
}());

// "42" -> value<core_type<32, 16>>{42}
static_assert([] {
    auto r = compile_to_closure("42"sv);
    if (!r.has_value())
        return false;
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto vr = r.value()(env0);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 42;
}());

// "(if #t (+ 1 2) 0)" -> value<core_type<32, 16>>{3}
static_assert([] {
    auto r = compile_to_closure("(if #t (+ 1 2) 0)"sv);
    if (!r.has_value())
        return false;
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto vr = r.value()(env0);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 3;
}());

// "#t" -> value<core_type<32, 16>>{true}
static_assert([] {
    auto r = compile_to_closure("#t"sv);
    if (!r.has_value())
        return false;
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto vr = r.value()(env0);
    return vr.has_value() && std::holds_alternative<bool>(vr.value()) &&
           std::get<bool>(vr.value()) == true;
}());

// bad source -> compile_to_closure returns error
static_assert([] {
    auto r = compile_to_closure("(+ x 1)"sv);
    if (!r.has_value())
        return true; // elaboration or eval error
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto vr = r.value()(env0);
    return !vr.has_value(); // unbound variable error at runtime
}());

} // namespace

TEST_CASE("ClosureBackendTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ClosureBackendTest - AddMultiply") {
    auto r = compile_to_closure("(+ 1 (* 2 3))"sv);
    REQUIRE(r.has_value());
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto vr = r.value()(env0);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 7);
}

TEST_CASE("ClosureBackendTest - Integer") {
    auto r = compile_to_closure("42"sv);
    REQUIRE(r.has_value());
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto vr = r.value()(env0);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 42);
}

TEST_CASE("ClosureBackendTest - IfExpression") {
    auto r = compile_to_closure("(if #t (+ 1 2) 0)"sv);
    REQUIRE(r.has_value());
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto vr = r.value()(env0);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 3);
}

TEST_CASE("ClosureBackendTest - Boolean") {
    auto r = compile_to_closure("#t"sv);
    REQUIRE(r.has_value());
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto vr = r.value()(env0);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<bool>(vr.value()));
    REQUIRE(std::get<bool>(vr.value()) == true);
}

TEST_CASE("ClosureBackendTest - UnboundVariableError") {
    auto r = compile_to_closure("(+ x 1)"sv);
    if (!r.has_value())
        return; // error at elaboration
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto vr = r.value()(env0);
    REQUIRE_FALSE(vr.has_value());
}

#endif
