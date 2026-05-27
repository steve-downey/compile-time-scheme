// src/smd/smdscheme/closure/ffi.test.cpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <catch2/catch_test_macros.hpp>
#include <smd/smdscheme/closure/closure_backend.hpp>
#include <smd/smdscheme/elaborator/elaborator.hpp>
#include <smd/smdscheme/closure/eval_direct.hpp>
#include <smd/smdscheme/reader/reader.hpp>
#include <smd/smdscheme/parser/cursor.hpp>
#include <smd/smdscheme/smdscheme.hpp>
#include <span>
#include <string_view>

using namespace smd::smdscheme;
using namespace std::string_view_literals;

constexpr auto
my_ffi_func(std::span<
            smd::smdscheme::closure::value<elaborator::core_type<32, 16>> const>
                args)
    -> foundation::result<
        smd::smdscheme::closure::value<elaborator::core_type<32, 16>>> {
    if (args.size() != 1)
        return foundation::result<
            smd::smdscheme::closure::value<elaborator::core_type<32, 16>>>{
            foundation::parse_error{{}, "ffi arity mismatch"}};
    if (!std::holds_alternative<int>(args[0]))
        return foundation::result<
            smd::smdscheme::closure::value<elaborator::core_type<32, 16>>>{
            foundation::parse_error{{}, "ffi type error"}};
    return smd::smdscheme::closure::value<elaborator::core_type<32, 16>>{
        std::get<int>(args[0]) + 100};
}

TEST_CASE("FFITest - Basic") {

    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  16>();
    e.define("add-100",
             smd::smdscheme::closure::value<elaborator::core_type<32, 16>>{
                 smd::smdscheme::closure::foreign_function<
                     elaborator::core_type<32, 16>>{my_ffi_func}});

    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;

    auto dr = reader::read_datum<32, 16>(parser::cursor{"(add-100 42)"sv},
                                         datum_arena);
    REQUIRE(dr.has_value());

    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    REQUIRE(er.has_value());

    auto const &ct = er.value();
    auto vr = smd::smdscheme::eval_direct<32, 16, 16>(ct, core_arena, e);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 142);
}

static_assert([] {
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  16>();
    e.define("add-100",
             smd::smdscheme::closure::value<elaborator::core_type<32, 16>>{
                 smd::smdscheme::closure::foreign_function<
                     elaborator::core_type<32, 16>>{my_ffi_func}});

    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;

    auto dr = reader::read_datum<32, 16>(parser::cursor{"(add-100 42)"sv},
                                         datum_arena);
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    auto const &ct = er.value();
    auto vr = smd::smdscheme::eval_direct<32, 16, 16>(ct, core_arena, e);
    return vr.has_value() && std::get<int>(vr.value()) == 142;
}());

#include <smd/smdscheme/closure/cps.hpp>

TEST_CASE("FFITest - CPS") {
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  16>();
    e.define("add-100",
             smd::smdscheme::closure::value<elaborator::core_type<32, 16>>{
                 smd::smdscheme::closure::foreign_function<
                     elaborator::core_type<32, 16>>{my_ffi_func}});

    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;

    auto dr = reader::read_datum<32, 16>(parser::cursor{"(add-100 42)"sv},
                                         datum_arena);
    REQUIRE(dr.has_value());

    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    REQUIRE(er.has_value());

    auto const &ct = er.value();

    auto code = smd::smdscheme::cps::compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        e,
        [](smd::smdscheme::closure::value<elaborator::core_type<32, 16>>
               v) constexpr
            -> foundation::result<
                smd::smdscheme::closure::value<elaborator::core_type<32, 16>>> {
            return v;
        });

    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 142);
}

static_assert([] {
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  16>();
    e.define("add-100",
             smd::smdscheme::closure::value<elaborator::core_type<32, 16>>{
                 smd::smdscheme::closure::foreign_function<
                     elaborator::core_type<32, 16>>{my_ffi_func}});

    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;

    auto dr = reader::read_datum<32, 16>(parser::cursor{"(add-100 42)"sv},
                                         datum_arena);
    auto er = elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                            core_arena);
    auto const &ct = er.value();

    auto code = smd::smdscheme::cps::compile_cps<32, 16>(ct, core_arena);
    auto r = code(
        e,
        [](smd::smdscheme::closure::value<elaborator::core_type<32, 16>>
               v) constexpr
            -> foundation::result<
                smd::smdscheme::closure::value<elaborator::core_type<32, 16>>> {
            return v;
        });

    return r.has_value() && std::get<int>(r.value()) == 142;
}());
