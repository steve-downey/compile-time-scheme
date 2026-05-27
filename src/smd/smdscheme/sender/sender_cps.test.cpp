// src/smd/smdscheme/sender/sender_cps.test.cpp                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/sender_cps.hpp>
#include <smd/smdscheme/sender/sender_cps.hpp> // test 2nd include OK

#include <smd/smdscheme/reader/reader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
using namespace smd::smdscheme;
using namespace std::string_view_literals;

using Core = elaborator::core_type<32, 16>;
using Val  = closure::value<Core>;
using Res  = foundation::result<Val>;

auto eval(std::string_view src) -> Res {
    auto program_r = sender_cps::compile_sender_cps<32, 16>(src);
    if (!program_r.has_value())
        return Res{program_r.error()};
    auto env = closure::default_env<Core, 16>();
    return program_r.value()(env);
}

} // namespace

TEST_CASE("SenderCpsTest - IntegerLiteral") {
    auto r = eval("42");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("SenderCpsTest - BooleanTrue") {
    auto r = eval("#t");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<bool>(r.value()));
    REQUIRE(std::get<bool>(r.value()) == true);
}

TEST_CASE("SenderCpsTest - BooleanFalse") {
    auto r = eval("#f");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<bool>(r.value()));
    REQUIRE(std::get<bool>(r.value()) == false);
}

TEST_CASE("SenderCpsTest - Addition") {
    auto r = eval("(+ 3 4)");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("SenderCpsTest - Multiplication") {
    auto r = eval("(* 3 4)");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 12);
}

TEST_CASE("SenderCpsTest - NestedArithmetic") {
    auto r = eval("(+ 1 (* 2 3))");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("SenderCpsTest - IfTrue") {
    auto r = eval("(if #t 1 2)");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 1);
}

TEST_CASE("SenderCpsTest - IfFalse") {
    auto r = eval("(if #f 1 2)");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("SenderCpsTest - IfWithExpression") {
    auto r = eval("(if #t (+ 1 2) (* 3 4))");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("SenderCpsTest - LambdaApplication") {
    auto r = eval("((lambda (x) (+ x 1)) 41)");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("SenderCpsTest - Quote") {
    auto r = eval("(quote abc)");
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<closure::symbol>(r.value()));
    REQUIRE(std::get<closure::symbol>(r.value()).name == "abc");
}

TEST_CASE("SenderCpsTest - UnboundVariable") {
    auto r = eval("undefined_var");
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("SenderCpsTest - HeaderIsIdempotent") { REQUIRE(true); }
