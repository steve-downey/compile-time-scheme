// src/smd/schemepoc/eval_direct.test.cpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/eval_direct.hpp>
#include <smd/schemepoc/eval_direct.hpp> // test 2nd include OK

#include <smd/schemepoc/reader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
using namespace smd::schemepoc;
using namespace std::string_view_literals;

// Full pipeline: "(+ 1 (* 2 3))" -> 7
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"(+ 1 (* 2 3))"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 7;
}());

// Lambda yielding closure
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"(lambda (x) x)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    return vr.has_value() && std::holds_alternative<closure>(vr.value()) &&
           std::get<closure>(vr.value()).node == ct.size() - 1;
}());

// "42" -> value holding int{42}
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"42"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 42;
}());

// "#t" -> value holding bool{true}
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"#t"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    return vr.has_value() && std::holds_alternative<bool>(vr.value()) &&
           std::get<bool>(vr.value()) == true;
}());

// "(+ 1 2)" -> value holding int{3}
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"(+ 1 2)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 3;
}());

// "(* 3 4)" -> value holding int{12}
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"(* 3 4)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 12;
}());

// "(if #t 1 2)" -> value holding int{1}
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"(if #t 1 2)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 1;
}());

// "(if #f 1 2)" -> value holding int{2}
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"(if #f 1 2)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 2;
}());

// "()" -> error from elaboration (before eval is reached)
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"()"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    return !er.has_value();
}());

// Unbound variable "x" -> error from lookup
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"x"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    return !vr.has_value();
}());

// "((lambda (x) x) 42)" -> value holding int{42}
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"((lambda (x) x) 42)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 42;
}());

// "((lambda (x) (+ x 1)) 5)" -> value holding int{6}
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"((lambda (x) (+ x 1)) 5)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 6;
}());

// "((lambda (x y) (+ x y)) 3 4)" -> value holding int{7}
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"((lambda (x y) (+ x y)) 3 4)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 7;
}());

// "(((lambda (x) (lambda (y) (+ x y))) 3) 4)" -> value holding int{7}
static_assert([] {
    auto dr = read_datum<32, 16>(
        cursor{"(((lambda (x) (lambda (y) (+ x y))) 3) 4)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 7;
}());

// "((lambda (x) ((lambda (y) (+ x y)) 2)) 40)" -> value holding int{42}
static_assert([] {
    auto dr = read_datum<32, 16>(
        cursor{"((lambda (x) ((lambda (y) (+ x y)) 2)) 40)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 42;
}());

} // namespace

TEST_CASE("EvalDirectTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("EvalDirectTest - FullPipeline") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"(+ 1 (* 2 3))"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 7);
}

TEST_CASE("EvalDirectTest - Integer") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"42"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 42);
}

TEST_CASE("EvalDirectTest - Boolean") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"#t"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<bool>(vr.value()));
    REQUIRE(std::get<bool>(vr.value()) == true);
}

TEST_CASE("EvalDirectTest - Add") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"(+ 1 2)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 3);
}

TEST_CASE("EvalDirectTest - Multiply") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"(* 3 4)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 12);
}

TEST_CASE("EvalDirectTest - IfTrue") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"(if #t 1 2)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 1);
}

TEST_CASE("EvalDirectTest - IfFalse") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"(if #f 1 2)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 2);
}

TEST_CASE("EvalDirectTest - UnboundVariable") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"x"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    REQUIRE_FALSE(vr.has_value());
}

TEST_CASE("EvalDirectTest - EvalLambdaYieldsClosure") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"(lambda (x) x)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<closure>(vr.value()));
    auto closure_val = std::get<closure>(vr.value());
    REQUIRE(closure_val.node == ct.size() - 1);
}

TEST_CASE("EvalDirectTest - ApplicationIdentity") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"((lambda (x) x) 42)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 42);
}

TEST_CASE("EvalDirectTest - ApplicationAdd") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"((lambda (x) (+ x 1)) 5)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 6);
}

TEST_CASE("EvalDirectTest - ApplicationAddMultiArgs") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"((lambda (x y) (+ x y)) 3 4)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 7);
}

TEST_CASE("EvalDirectTest - ApplicationArityMismatch") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"((lambda (x y) x) 1)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    REQUIRE_FALSE(vr.has_value());
    REQUIRE(vr.error().message == "arity error");
}

TEST_CASE("EvalDirectTest - ApplicationNonFunction") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"(1 2 3)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    REQUIRE_FALSE(vr.has_value());
    REQUIRE(vr.error().message == "type error");
}

TEST_CASE("EvalDirectTest - LexicalCapture1") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(
        cursor{"(((lambda (x) (lambda (y) (+ x y))) 3) 4)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 7);
}

TEST_CASE("EvalDirectTest - LexicalCapture2") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(
        cursor{"((lambda (x) ((lambda (y) (+ x y)) 2)) 40)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 42);
}
