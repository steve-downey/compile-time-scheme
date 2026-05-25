// src/smd/schemepoc/cps_arithmetic.test.cpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/cps.hpp>
#include <smd/schemepoc/cps.hpp> // test 2nd include OK

#include <smd/schemepoc/reader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
using namespace smd::schemepoc;
using namespace std::string_view_literals;

// "(+ 3 4)" -> value{7} via CPS
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"(+ 3 4)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const& ct   = er.value();
    auto         code = compile_cps(ct, ct.size() - 1);
    auto         env0 = default_env<16>();
    auto r = code(env0, [](value v) constexpr -> result<value> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 7;
}());

// "42" -> value{42} via CPS
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"42"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const& ct   = er.value();
    auto         code = compile_cps(ct, ct.size() - 1);
    auto         env0 = default_env<16>();
    auto r = code(env0, [](value v) constexpr -> result<value> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 42;
}());

// "(* 3 4)" -> value{12} via CPS
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"(* 3 4)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const& ct   = er.value();
    auto         code = compile_cps(ct, ct.size() - 1);
    auto         env0 = default_env<16>();
    auto r = code(env0, [](value v) constexpr -> result<value> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 12;
}());

// error propagates: unbound variable
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"x"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const& ct   = er.value();
    auto         code = compile_cps(ct, ct.size() - 1);
    auto         env0 = default_env<16>();
    auto r = code(env0, [](value v) constexpr -> result<value> { return v; });
    return !r.has_value();
}());

} // namespace

TEST_CASE("CpsArithmeticTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("CpsArithmeticTest - AddExpression") {
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"(+ 3 4)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const& ct   = er.value();
    auto         code = compile_cps(ct, ct.size() - 1);
    auto         env0 = default_env<16>();
    auto r = code(env0, [](value v) -> result<value> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("CpsArithmeticTest - Integer") {
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"42"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const& ct   = er.value();
    auto         code = compile_cps(ct, ct.size() - 1);
    auto         env0 = default_env<16>();
    auto r = code(env0, [](value v) -> result<value> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("CpsArithmeticTest - Multiply") {
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"(* 3 4)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const& ct   = er.value();
    auto         code = compile_cps(ct, ct.size() - 1);
    auto         env0 = default_env<16>();
    auto r = code(env0, [](value v) -> result<value> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 12);
}

TEST_CASE("CpsArithmeticTest - ErrorPropagates") {
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"x"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const& ct   = er.value();
    auto         code = compile_cps(ct, ct.size() - 1);
    auto         env0 = default_env<16>();
    auto r = code(env0, [](value v) -> result<value> { return v; });
    REQUIRE_FALSE(r.has_value());
}
