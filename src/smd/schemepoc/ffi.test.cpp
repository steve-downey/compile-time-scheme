// src/smd/schemepoc/ffi.test.cpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <catch2/catch_test_macros.hpp>
#include <smd/schemepoc/eval_direct.hpp>
#include <smd/schemepoc/reader.hpp>
#include <smd/schemepoc/elaborator.hpp>
#include <smd/schemepoc/closure_backend.hpp>
#include <smd/schemepoc/schemepoc.hpp>
#include <span>
#include <string_view>

using namespace smd::schemepoc;
using namespace std::string_view_literals;

constexpr auto my_ffi_func(std::span<value<core_type<32, 16>> const> args) -> result<value<core_type<32, 16>>> {
    if (args.size() != 1) return result<value<core_type<32, 16>>>{parse_error{{}, "ffi arity mismatch"}};
    if (!std::holds_alternative<int>(args[0])) return result<value<core_type<32, 16>>>{parse_error{{}, "ffi type error"}};
    return value<core_type<32, 16>>{std::get<int>(args[0]) + 100};
}

TEST_CASE("FFITest - Basic") {
    
    auto e = default_env<core_type<32, 16>, 16>();
    e.define("add-100", value<core_type<32, 16>>{foreign_function<core_type<32, 16>>{my_ffi_func}});
    
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    
    auto dr = read_datum<32, 16>(cursor{"(add-100 42)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena, e);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 142);
}

static_assert([] {
    auto e = default_env<core_type<32, 16>, 16>();
    e.define("add-100", value<core_type<32, 16>>{foreign_function<core_type<32, 16>>{my_ffi_func}});
    
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    
    auto dr = read_datum<32, 16>(cursor{"(add-100 42)"sv}, datum_arena);
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena, e);
    return vr.has_value() && std::get<int>(vr.value()) == 142;
}());

#include <smd/schemepoc/cps.hpp>

TEST_CASE("FFITest - CPS") {
    auto e = default_env<core_type<32, 16>, 16>();
    e.define("add-100", value<core_type<32, 16>>{foreign_function<core_type<32, 16>>{my_ffi_func}});
    
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    
    auto dr = read_datum<32, 16>(cursor{"(add-100 42)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    
    auto const &ct = er.value();
    
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(e, [](value<core_type<32, 16>> v) constexpr -> result<value<core_type<32, 16>>> { return v; });
    
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 142);
}

static_assert([] {
    auto e = default_env<core_type<32, 16>, 16>();
    e.define("add-100", value<core_type<32, 16>>{foreign_function<core_type<32, 16>>{my_ffi_func}});
    
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    
    auto dr = read_datum<32, 16>(cursor{"(add-100 42)"sv}, datum_arena);
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    auto const &ct = er.value();
    
    auto code = compile_cps<32, 16>(ct, core_arena);
    auto r = code(e, [](value<core_type<32, 16>> v) constexpr -> result<value<core_type<32, 16>>> { return v; });
    
    return r.has_value() && std::get<int>(r.value()) == 142;
}());

