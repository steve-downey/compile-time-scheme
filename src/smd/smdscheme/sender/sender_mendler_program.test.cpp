// src/smd/smdscheme/sender/sender_mendler_program.test.cpp        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/sender_mendler_program.hpp>
#include <smd/smdscheme/sender/sender_mendler_program.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
using namespace smd::smdscheme;
using namespace std::string_view_literals;

using CompT = fixpoint_eval::Comp<16>;
using Val   = closure::value<CompT>;
using Res   = foundation::result<Val>;

auto eval(std::string_view src) -> Res {
    auto program_r = sender_mendler::compile_sender_mendler<32, 16>(src);
    if (!program_r.has_value())
        return Res{program_r.error()};
    auto env = closure::default_env<CompT, 16>();
    return program_r.value()(env);
}

} // namespace

TEST_CASE("SenderMendlerTest - IntegerLiteral") {
    auto r = eval("42");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("SenderMendlerTest - BooleanTrue") {
    auto r = eval("#t");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<bool>(r.value()));
    REQUIRE(std::get<bool>(r.value()) == true);
}

TEST_CASE("SenderMendlerTest - BooleanFalse") {
    auto r = eval("#f");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<bool>(r.value()));
    REQUIRE(std::get<bool>(r.value()) == false);
}

TEST_CASE("SenderMendlerTest - Addition") {
    auto r = eval("(+ 3 4)");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("SenderMendlerTest - Multiplication") {
    auto r = eval("(* 3 4)");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 12);
}

TEST_CASE("SenderMendlerTest - NestedArithmetic") {
    auto r = eval("(+ 1 (* 2 3))");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("SenderMendlerTest - IfTrue") {
    auto r = eval("(if #t 1 2)");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 1);
}

TEST_CASE("SenderMendlerTest - IfFalse") {
    auto r = eval("(if #f 1 2)");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("SenderMendlerTest - IfWithExpression") {
    auto r = eval("(if #t (+ 1 2) (* 3 4))");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("SenderMendlerTest - LambdaApplication") {
    auto r = eval("((lambda (x) (+ x 1)) 41)");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("SenderMendlerTest - Quote") {
    auto r = eval("(quote abc)");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<closure::symbol>(r.value()));
    REQUIRE(std::get<closure::symbol>(r.value()).name == "abc");
}

TEST_CASE("SenderMendlerTest - UnboundVariable") {
    auto r = eval("undefined_var");
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("SenderMendlerTest - ClosureCapture") {
    auto r = eval("(((lambda (x) (lambda (y) (+ x y))) 10) 5)");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 15);
}

TEST_CASE("SenderMendlerTest - HeaderIsIdempotent") { REQUIRE(true); }
