// src/smd/schemepoc/value.test.cpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/value.hpp>
#include <smd/schemepoc/value.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
using namespace smd::schemepoc;
using namespace std::string_view_literals;

static_assert([] {
    auto e = default_env<8>();
    auto r = e.lookup("+"sv);
    return r.has_value() && std::holds_alternative<builtin>(r.value()) &&
           std::get<builtin>(r.value()).op == builtin_op::add;
}());

static_assert([] {
    auto e = default_env<8>();
    auto r = e.lookup("*"sv);
    return r.has_value() && std::holds_alternative<builtin>(r.value()) &&
           std::get<builtin>(r.value()).op == builtin_op::multiply;
}());

static_assert([] {
    auto e = default_env<8>();
    auto r = e.lookup("x"sv);
    return !r.has_value();
}());

static_assert([] {
    auto e = default_env<8>();
    e.define("x"sv, value{42});
    auto r = e.lookup("x"sv);
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 42;
}());

static_assert([] {
    auto e = default_env<8>();
    e.define("x"sv, value{1});
    e.define("x"sv, value{2});
    auto r = e.lookup("x"sv);
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 2;
}());

} // namespace

TEST_CASE("ValueTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ValueTest - DefaultEnvHasAdd") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto e = default_env<8>();
    auto r = e.lookup("+"sv);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<builtin>(r.value()));
    REQUIRE(std::get<builtin>(r.value()).op == builtin_op::add);
}

TEST_CASE("ValueTest - DefaultEnvHasMultiply") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto e = default_env<8>();
    auto r = e.lookup("*"sv);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<builtin>(r.value()));
    REQUIRE(std::get<builtin>(r.value()).op == builtin_op::multiply);
}

TEST_CASE("ValueTest - UnboundVariableIsError") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto e = default_env<8>();
    auto r = e.lookup("x"sv);
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("ValueTest - DefineAndLookup") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto e = default_env<8>();
    e.define("x"sv, value{42});
    auto r = e.lookup("x"sv);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("ValueTest - DefineShadows") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto e = default_env<8>();
    e.define("x"sv, value{1});
    e.define("x"sv, value{2});
    auto r = e.lookup("x"sv);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}
