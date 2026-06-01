// src/smd/smdscheme/closure/eval_direct.test.cpp               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/closure/eval_direct.hpp>
#include <smd/smdscheme/closure/eval_direct.hpp> // test 2nd include OK

#include <smd/smdscheme/parser/cursor.hpp>
#include <smd/smdscheme/reader/read_datum.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
using namespace std::string_view_literals;

static_assert([] {
    using Core = smd::smdscheme::elaborator::core_type<32, 16>;
    smd::smdscheme::foundation::tree_arena<
        smd::smdscheme::reader::datum_type<32, 16>, 32>
        datum_arena;
    smd::smdscheme::foundation::tree_arena<Core, 32> core_arena;

    auto dr = smd::smdscheme::reader::read_datum<32, 16>(
        smd::smdscheme::parser::cursor{"(+ 1 (* 2 3))"sv}, datum_arena);
    if (!dr.has_value()) {
        return false;
    }

    auto er = smd::smdscheme::elaborator::elaborate<32, 16>(
        dr.value().value, datum_arena, core_arena);
    if (!er.has_value()) {
        return false;
    }

    auto vr = smd::smdscheme::eval_direct<32, 16, 16>(
        er.value(), core_arena,
        smd::smdscheme::closure::default_env<Core, 16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 7;
}());

static_assert([] {
    using Core = smd::smdscheme::elaborator::core_type<32, 16>;
    smd::smdscheme::foundation::tree_arena<
        smd::smdscheme::reader::datum_type<32, 16>, 32>
        datum_arena;
    smd::smdscheme::foundation::tree_arena<Core, 32> core_arena;

    auto dr = smd::smdscheme::reader::read_datum<32, 16>(
        smd::smdscheme::parser::cursor{"(lambda (x) x)"sv}, datum_arena);
    if (!dr.has_value()) {
        return false;
    }

    auto er = smd::smdscheme::elaborator::elaborate<32, 16>(
        dr.value().value, datum_arena, core_arena);
    if (!er.has_value()) {
        return false;
    }

    auto vr = smd::smdscheme::eval_direct<32, 16, 16>(
        er.value(), core_arena,
        smd::smdscheme::closure::default_env<Core, 16>());
    return vr.has_value() &&
           std::holds_alternative<smd::smdscheme::closure::closure<Core>>(
               vr.value());
}());

// c8d39ca4-8d99-4406-bb40-50d38acd6761
static_assert([] {
    using Core = smd::smdscheme::elaborator::core_type<32, 16>;
    smd::smdscheme::foundation::tree_arena<
        smd::smdscheme::reader::datum_type<32, 16>, 32>
        datum_arena;
    smd::smdscheme::foundation::tree_arena<Core, 32> core_arena;

    // Bad syntax: 'if' requires 3 arguments (condition, consequent,
    // alternative)
    auto dr = smd::smdscheme::reader::read_datum<32, 16>(
        smd::smdscheme::parser::cursor{"(if #t 1)"sv}, datum_arena);
    if (!dr.has_value())
        return false;

    auto er = smd::smdscheme::elaborator::elaborate<32, 16>(
        dr.value().value, datum_arena, core_arena);

    // Elaboration should fail naturally here
    return !er.has_value();
}());
// c8d39ca4-8d99-4406-bb40-50d38acd6761 end

} // namespace

TEST_CASE("EvalDirectTest - HeaderIsIdempotent") { REQUIRE(true); }
