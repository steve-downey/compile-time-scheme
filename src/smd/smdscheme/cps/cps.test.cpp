// src/smd/schemepoc/cps.test.cpp                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/cps/cps.hpp>
#include <smd/smdscheme/cps/cps.hpp> // test 2nd include OK

#include <smd/smdscheme/reader/reader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
using namespace smd::smdscheme;
using namespace smd::smdscheme::cps;
using namespace std::string_view_literals;

// "#t" -> value{true} via CPS
static_assert([] {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr = reader::read_datum<32, 16>(parser::cursor{"#t"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto env = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env,
        [](closure::value<elaborator::core_type<32, 16>> v) constexpr
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    return r.has_value() && std::holds_alternative<bool>(r.value()) &&
           std::get<bool>(r.value()) == true;
}());

// "#f" -> value{false} via CPS
static_assert([] {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr = reader::read_datum<32, 16>(parser::cursor{"#f"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto env = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env,
        [](closure::value<elaborator::core_type<32, 16>> v) constexpr
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    return r.has_value() && std::holds_alternative<bool>(r.value()) &&
           std::get<bool>(r.value()) == false;
}());

// "(if #t 1 2)" -> value{1} via CPS (true branch taken)
static_assert([] {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr = reader::read_datum<32, 16>(parser::cursor{"(if #t 1 2)"sv},
                                         datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto env = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env,
        [](closure::value<elaborator::core_type<32, 16>> v) constexpr
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 1;
}());

// "(if #f 1 2)" -> value{2} via CPS (false branch taken)
static_assert([] {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr = reader::read_datum<32, 16>(parser::cursor{"(if #f 1 2)"sv},
                                         datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto env = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env,
        [](closure::value<elaborator::core_type<32, 16>> v) constexpr
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 2;
}());

// "(if #t (+ 1 2) 0)" -> value{3} via CPS (expression in branch)
static_assert([] {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr = reader::read_datum<32, 16>(parser::cursor{"(if #t (+ 1 2) 0)"sv},
                                         datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto env = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env,
        [](closure::value<elaborator::core_type<32, 16>> v) constexpr
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 3;
}());

// "(if x 1 2)" -> error (unbound variable in condition)
static_assert([] {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr =
        reader::read_datum<32, 16>(parser::cursor{"(if x 1 2)"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto env = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env,
        [](closure::value<elaborator::core_type<32, 16>> v) constexpr
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    return !r.has_value();
}());

// cps_of with explicit identity cont produces the same foundation::result as
// compile_cps
static_assert([] {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr =
        reader::read_datum<32, 16>(parser::cursor{"(+ 2 3)"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto env = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto code = cps_of<32, 16>(
        ct, core_arena,
        [](closure::value<elaborator::core_type<32, 16>> v)
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    auto r = code(
        env,
        [](closure::value<elaborator::core_type<32, 16>> v) constexpr
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 5;
}());

} // namespace

TEST_CASE("CpsTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("CpsTest - BooleanTrue") {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr = reader::read_datum<32, 16>(parser::cursor{"#t"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v)
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<bool>(r.value()));
    REQUIRE(std::get<bool>(r.value()) == true);
}

TEST_CASE("CpsTest - BooleanFalse") {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr = reader::read_datum<32, 16>(parser::cursor{"#f"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v)
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<bool>(r.value()));
    REQUIRE(std::get<bool>(r.value()) == false);
}

TEST_CASE("CpsTest - IfTrueBranch") {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr = reader::read_datum<32, 16>(parser::cursor{"(if #t 1 2)"sv},
                                         datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v)
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 1);
}

TEST_CASE("CpsTest - IfFalseBranch") {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr = reader::read_datum<32, 16>(parser::cursor{"(if #f 1 2)"sv},
                                         datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v)
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("CpsTest - IfWithExpressionInBranch") {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr = reader::read_datum<32, 16>(parser::cursor{"(if #t (+ 1 2) 0)"sv},
                                         datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v)
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("CpsTest - IfErrorInCondition") {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr =
        reader::read_datum<32, 16>(parser::cursor{"(if x 1 2)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v)
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("CpsTest - CpsOfDirectly") {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr =
        reader::read_datum<32, 16>(parser::cursor{"(+ 2 3)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto code = cps_of<32, 16>(
        ct, core_arena,
        [](closure::value<elaborator::core_type<32, 16>> v)
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v)
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 5);
}

TEST_CASE("CpsTest - LambdaEvaluatesToClosure") {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr = reader::read_datum<32, 16>(parser::cursor{"(lambda (x) x)"sv},
                                         datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v)
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(
        std::holds_alternative<closure::closure<elaborator::core_type<32, 16>>>(
            r.value()));
}

TEST_CASE("CpsTest - ApplicationOfLambda") {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr = reader::read_datum<32, 16>(
        parser::cursor{"((lambda (x) (+ x 1)) 41)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v)
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("CpsTest - QuoteEvaluatesToAtom") {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr = reader::read_datum<32, 16>(parser::cursor{"(quote abc)"sv},
                                         datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v)
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<closure::symbol>(r.value()));
    REQUIRE(std::get<closure::symbol>(r.value()).name == "abc");
}
