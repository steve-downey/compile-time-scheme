// src/smd/schemepoc/cps.test.cpp                                   -*-C++-*-
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

// "#t" -> value{true} via CPS
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"#t"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct  = er.value();
    auto        env = default_env<16>();
    auto code = compile_cps(ct, ct.size() - 1);
    auto r = code(env, [](value v) constexpr -> result<value> { return v; });
    return r.has_value() && std::holds_alternative<bool>(r.value()) &&
           std::get<bool>(r.value()) == true;
}());

// "#f" -> value{false} via CPS
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"#f"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct  = er.value();
    auto        env = default_env<16>();
    auto code = compile_cps(ct, ct.size() - 1);
    auto r = code(env, [](value v) constexpr -> result<value> { return v; });
    return r.has_value() && std::holds_alternative<bool>(r.value()) &&
           std::get<bool>(r.value()) == false;
}());

// "(if #t 1 2)" -> value{1} via CPS (true branch taken)
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"(if #t 1 2)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct  = er.value();
    auto        env = default_env<16>();
    auto code = compile_cps(ct, ct.size() - 1);
    auto r = code(env, [](value v) constexpr -> result<value> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 1;
}());

// "(if #f 1 2)" -> value{2} via CPS (false branch taken)
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"(if #f 1 2)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct  = er.value();
    auto        env = default_env<16>();
    auto code = compile_cps(ct, ct.size() - 1);
    auto r = code(env, [](value v) constexpr -> result<value> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 2;
}());

// "(if #t (+ 1 2) 0)" -> value{3} via CPS (expression in branch)
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"(if #t (+ 1 2) 0)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct  = er.value();
    auto        env = default_env<16>();
    auto code = compile_cps(ct, ct.size() - 1);
    auto r = code(env, [](value v) constexpr -> result<value> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 3;
}());

// "(if x 1 2)" -> error (unbound variable in condition)
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"(if x 1 2)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct  = er.value();
    auto        env = default_env<16>();
    auto code = compile_cps(ct, ct.size() - 1);
    auto r = code(env, [](value v) constexpr -> result<value> { return v; });
    return !r.has_value();
}());

// cps_of with explicit identity cont produces the same result as compile_cps
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"(+ 2 3)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct  = er.value();
    auto        env = default_env<16>();
    auto code =
        cps_of(ct, ct.size() - 1, [](value v) -> result<value> { return v; });
    auto r = code(env, [](value v) constexpr -> result<value> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 5;
}());

} // namespace

TEST_CASE("CpsTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("CpsTest - BooleanTrue") {
    auto dr = read_datum<32, 16>(cursor{"#t"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct   = er.value();
    auto        env0 = default_env<16>();
    auto        code = compile_cps(ct, ct.size() - 1);
    auto        r    = code(env0, [](value v) -> result<value> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<bool>(r.value()));
    REQUIRE(std::get<bool>(r.value()) == true);
}

TEST_CASE("CpsTest - BooleanFalse") {
    auto dr = read_datum<32, 16>(cursor{"#f"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct   = er.value();
    auto        env0 = default_env<16>();
    auto        code = compile_cps(ct, ct.size() - 1);
    auto        r    = code(env0, [](value v) -> result<value> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<bool>(r.value()));
    REQUIRE(std::get<bool>(r.value()) == false);
}

TEST_CASE("CpsTest - IfTrueBranch") {
    auto dr = read_datum<32, 16>(cursor{"(if #t 1 2)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct   = er.value();
    auto        env0 = default_env<16>();
    auto        code = compile_cps(ct, ct.size() - 1);
    auto        r    = code(env0, [](value v) -> result<value> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 1);
}

TEST_CASE("CpsTest - IfFalseBranch") {
    auto dr = read_datum<32, 16>(cursor{"(if #f 1 2)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct   = er.value();
    auto        env0 = default_env<16>();
    auto        code = compile_cps(ct, ct.size() - 1);
    auto        r    = code(env0, [](value v) -> result<value> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("CpsTest - IfWithExpressionInBranch") {
    auto dr = read_datum<32, 16>(cursor{"(if #t (+ 1 2) 0)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct   = er.value();
    auto        env0 = default_env<16>();
    auto        code = compile_cps(ct, ct.size() - 1);
    auto        r    = code(env0, [](value v) -> result<value> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("CpsTest - IfErrorInCondition") {
    auto dr = read_datum<32, 16>(cursor{"(if x 1 2)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct   = er.value();
    auto        env0 = default_env<16>();
    auto        code = compile_cps(ct, ct.size() - 1);
    auto        r    = code(env0, [](value v) -> result<value> { return v; });
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("CpsTest - CpsOfDirectly") {
    auto dr = read_datum<32, 16>(cursor{"(+ 2 3)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct   = er.value();
    auto        env0 = default_env<16>();
    auto code = cps_of(ct, ct.size() - 1, [](value v) -> result<value> { return v; });
    auto r = code(env0, [](value v) -> result<value> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 5);
}
