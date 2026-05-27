// src/smd/smdscheme/closure/cps_arithmetic.test.cpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/closure/cps.hpp>
#include <smd/smdscheme/closure/cps.hpp> // test 2nd include OK

#include <smd/smdscheme/reader/reader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
using namespace smd::smdscheme;
using namespace smd::smdscheme::cps;
using namespace std::string_view_literals;

// "(+ 3 4)" -> value{7} via CPS
static_assert([] {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr =
        reader::read_datum<32, 16>(parser::cursor{"(+ 3 4)"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v) constexpr
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 7;
}());

// "42" -> value{42} via CPS
static_assert([] {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr = reader::read_datum<32, 16>(parser::cursor{"42"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v) constexpr
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 42;
}());

// "(* 3 4)" -> value{12} via CPS
static_assert([] {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr =
        reader::read_datum<32, 16>(parser::cursor{"(* 3 4)"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v) constexpr
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 12;
}());

// error propagates: unbound variable
static_assert([] {
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr = reader::read_datum<32, 16>(parser::cursor{"x"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v) constexpr
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    return !r.has_value();
}());

} // namespace

TEST_CASE("CpsArithmeticTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("CpsArithmeticTest - AddExpression") {
    using namespace std::string_view_literals;
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr =
        reader::read_datum<32, 16>(parser::cursor{"(+ 3 4)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v)
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("CpsArithmeticTest - Integer") {
    using namespace std::string_view_literals;
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr = reader::read_datum<32, 16>(parser::cursor{"42"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v)
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("CpsArithmeticTest - Multiply") {
    using namespace std::string_view_literals;
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr =
        reader::read_datum<32, 16>(parser::cursor{"(* 3 4)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v)
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 12);
}

TEST_CASE("CpsArithmeticTest - ErrorPropagates") {
    using namespace std::string_view_literals;
    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;
    auto dr = reader::read_datum<32, 16>(parser::cursor{"x"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto env0 = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto r = code(
        env0,
        [](closure::value<elaborator::core_type<32, 16>> v)
            -> foundation::result<
                closure::value<elaborator::core_type<32, 16>>> { return v; });
    REQUIRE_FALSE(r.has_value());
}
