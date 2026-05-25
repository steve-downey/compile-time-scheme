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
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"#t"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto env = default_env<core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(env,
                  [](value<core_type<32, 16>> v) constexpr
                      -> result<value<core_type<32, 16>>> { return v; });
    return r.has_value() && std::holds_alternative<bool>(r.value()) &&
           std::get<bool>(r.value()) == true;
}());

// "#f" -> value{false} via CPS
static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"#f"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto env = default_env<core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(env,
                  [](value<core_type<32, 16>> v) constexpr
                      -> result<value<core_type<32, 16>>> { return v; });
    return r.has_value() && std::holds_alternative<bool>(r.value()) &&
           std::get<bool>(r.value()) == false;
}());

// "(if #t 1 2)" -> value{1} via CPS (true branch taken)
static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(if #t 1 2)"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto env = default_env<core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(env,
                  [](value<core_type<32, 16>> v) constexpr
                      -> result<value<core_type<32, 16>>> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 1;
}());

// "(if #f 1 2)" -> value{2} via CPS (false branch taken)
static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(if #f 1 2)"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto env = default_env<core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(env,
                  [](value<core_type<32, 16>> v) constexpr
                      -> result<value<core_type<32, 16>>> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 2;
}());

// "(if #t (+ 1 2) 0)" -> value{3} via CPS (expression in branch)
static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(if #t (+ 1 2) 0)"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto env = default_env<core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(env,
                  [](value<core_type<32, 16>> v) constexpr
                      -> result<value<core_type<32, 16>>> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 3;
}());

// "(if x 1 2)" -> error (unbound variable in condition)
static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(if x 1 2)"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto env = default_env<core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(env,
                  [](value<core_type<32, 16>> v) constexpr
                      -> result<value<core_type<32, 16>>> { return v; });
    return !r.has_value();
}());

// cps_of with explicit identity cont produces the same result as compile_cps
static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(+ 2 3)"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto env = default_env<core_type<32, 16>, 16>();
    auto code = cps_of<32, 16>(
        ct, core_arena,
        [](value<core_type<32, 16>> v) -> result<value<core_type<32, 16>>> {
            return v;
        });
    auto r = code(env,
                  [](value<core_type<32, 16>> v) constexpr
                      -> result<value<core_type<32, 16>>> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 5;
}());

} // namespace

TEST_CASE("CpsTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("CpsTest - BooleanTrue") {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"#t"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](value<core_type<32, 16>> v) -> result<value<core_type<32, 16>>> {
            return v;
        });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<bool>(r.value()));
    REQUIRE(std::get<bool>(r.value()) == true);
}

TEST_CASE("CpsTest - BooleanFalse") {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"#f"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](value<core_type<32, 16>> v) -> result<value<core_type<32, 16>>> {
            return v;
        });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<bool>(r.value()));
    REQUIRE(std::get<bool>(r.value()) == false);
}

TEST_CASE("CpsTest - IfTrueBranch") {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(if #t 1 2)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](value<core_type<32, 16>> v) -> result<value<core_type<32, 16>>> {
            return v;
        });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 1);
}

TEST_CASE("CpsTest - IfFalseBranch") {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(if #f 1 2)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](value<core_type<32, 16>> v) -> result<value<core_type<32, 16>>> {
            return v;
        });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("CpsTest - IfWithExpressionInBranch") {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(if #t (+ 1 2) 0)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](value<core_type<32, 16>> v) -> result<value<core_type<32, 16>>> {
            return v;
        });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("CpsTest - IfErrorInCondition") {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(if x 1 2)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](value<core_type<32, 16>> v) -> result<value<core_type<32, 16>>> {
            return v;
        });
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("CpsTest - CpsOfDirectly") {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(+ 2 3)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto code = cps_of<32, 16>(
        ct, core_arena,
        [](value<core_type<32, 16>> v) -> result<value<core_type<32, 16>>> {
            return v;
        });
    auto r = code(
        env0,
        [](value<core_type<32, 16>> v) -> result<value<core_type<32, 16>>> {
            return v;
        });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 5);
}

TEST_CASE("CpsTest - LambdaEvaluatesToClosure") {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(lambda (x) x)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](value<core_type<32, 16>> v) -> result<value<core_type<32, 16>>> {
            return v;
        });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<closure<core_type<32, 16>>>(r.value()));
}

TEST_CASE("CpsTest - ApplicationOfLambda") {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr =
        read_datum<32, 16>(cursor{"((lambda (x) (+ x 1)) 41)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](value<core_type<32, 16>> v) -> result<value<core_type<32, 16>>> {
            return v;
        });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("CpsTest - QuoteEvaluatesToAtom") {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(quote abc)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = default_env<core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](value<core_type<32, 16>> v) -> result<value<core_type<32, 16>>> {
            return v;
        });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<symbol>(r.value()));
    REQUIRE(std::get<symbol>(r.value()).name == "abc");
}
