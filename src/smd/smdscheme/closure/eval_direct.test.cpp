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

namespace scm = smd::smdscheme;

using Core = scm::elaborator::core_type<64, 16>;
using Val = scm::closure::value<Core>;
using Res = scm::foundation::result<Val>;

/// Reads, elaborates and directly evaluates @p src under a store-backed
/// (mutable) default environment.  The result is a value type (or an error);
/// callers only inspect scalar/unspecified results, which do not reference the
/// arenas or store destroyed on return.
auto run(std::string_view src) -> Res {
    scm::foundation::tree_arena<scm::reader::datum_type<64, 16>, 64>
        datum_arena;
    scm::foundation::tree_arena<Core, 64> core_arena;

    auto dr =
        scm::reader::read_datum<64, 16>(scm::parser::cursor{src}, datum_arena);
    if (!dr.has_value())
        return Res{dr.error()};
    auto er = scm::elaborator::elaborate<64, 16>(dr.value().value, datum_arena,
                                                 core_arena);
    if (!er.has_value())
        return Res{er.error()};

    scm::closure::store<Core, scm::closure::default_max_store> st;
    auto env = scm::closure::default_env<Core, 16>(st);
    return scm::eval_direct<64, 16, 16>(er.value(), core_arena, env);
}

} // namespace

TEST_CASE("EvalDirectTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("EvalDirectTest - BeginSequences") {
    auto r = run("(begin 1 2 3)");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("EvalDirectTest - SetMutatesLocalBinding") {
    auto r = run("(let ((x 1)) (begin (set! x 2) x))");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("EvalDirectTest - SetReturnsUnspecified") {
    auto r = run("(let ((x 1)) (set! x 2))");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<scm::closure::unspecified>(r.value()));
}

TEST_CASE("EvalDirectTest - SetUnboundIsError") {
    auto r = run("(set! nope 1)");
    REQUIRE(!r.has_value());
}

TEST_CASE("EvalDirectTest - SetSharedAcrossClosureCalls") {
    // A counter closure captures `c`; two calls must observe the shared,
    // mutated cell even though the environment is deep-copied on capture.
    auto r = run("(let ((c 0))"
                 "  (let ((f (lambda () (begin (set! c (+ c 1)) c))))"
                 "    (begin (f) (f))))");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 2);
}
