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
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(+ 1 (* 2 3))"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 7;
}());

// Lambda yielding closure
static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(lambda (x) x)"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    return vr.has_value() &&
           std::holds_alternative<closure<core_type<32, 16>>>(vr.value()) &&
           std::get<closure<core_type<32, 16>>>(vr.value()).node != nullptr;
}());

// "42" -> value holding int{42}
static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"42"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 42;
}());

// "#t" -> value holding bool{true}
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
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    return vr.has_value() && std::holds_alternative<bool>(vr.value()) &&
           std::get<bool>(vr.value()) == true;
}());

// "(+ 1 2)" -> value holding int{3}
static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(+ 1 2)"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 3;
}());

// "(* 3 4)" -> value holding int{12}
static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(* 3 4)"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 12;
}());

// "(if #t 1 2)" -> value holding int{1}
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
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 1;
}());

// "(if #f 1 2)" -> value holding int{2}
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
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 2;
}());

// "()" -> error from elaboration (before eval is reached)
static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"()"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    return !er.has_value();
}());

// Unbound variable "x" -> error from lookup
static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"x"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    return !vr.has_value();
}());

// "((lambda (x) x) 42)" -> value holding int{42}
static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"((lambda (x) x) 42)"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 42;
}());

// "((lambda (x) (+ x 1)) 5)" -> value holding int{6}
static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr =
        read_datum<32, 16>(cursor{"((lambda (x) (+ x 1)) 5)"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 6;
}());

// "((lambda (x y) (+ x y)) 3 4)" -> value holding int{7}
static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"((lambda (x y) (+ x y)) 3 4)"sv},
                                 datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 7;
}());

// "(((lambda (x) (lambda (y) (+ x y))) 3) 4)" -> value holding int{7}
static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(
        cursor{"(((lambda (x) (lambda (y) (+ x y))) 3) 4)"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 7;
}());

// "((lambda (x) ((lambda (y) (+ x y)) 2)) 40)" -> value holding int{42}
static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(
        cursor{"((lambda (x) ((lambda (y) (+ x y)) 2)) 40)"sv}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 42;
}());

} // namespace

TEST_CASE("EvalDirectTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("EvalDirectTest - FullPipeline") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(+ 1 (* 2 3))"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 7);
}

TEST_CASE("EvalDirectTest - Integer") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"42"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 42);
}

TEST_CASE("EvalDirectTest - Boolean") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"#t"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<bool>(vr.value()));
    REQUIRE(std::get<bool>(vr.value()) == true);
}

TEST_CASE("EvalDirectTest - Add") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(+ 1 2)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 3);
}

TEST_CASE("EvalDirectTest - Multiply") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(* 3 4)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 12);
}

TEST_CASE("EvalDirectTest - IfTrue") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(if #t 1 2)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 1);
}

TEST_CASE("EvalDirectTest - IfFalse") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(if #f 1 2)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 2);
}

TEST_CASE("EvalDirectTest - UnboundVariable") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"x"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    REQUIRE_FALSE(vr.has_value());
}

TEST_CASE("EvalDirectTest - EvalLambdaYieldsClosure") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(lambda (x) x)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<closure<core_type<32, 16>>>(vr.value()));
    auto closure_val = std::get<closure<core_type<32, 16>>>(vr.value());
    REQUIRE(closure_val.node != nullptr);
}

TEST_CASE("EvalDirectTest - ApplicationIdentity") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"((lambda (x) x) 42)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 42);
}

TEST_CASE("EvalDirectTest - ApplicationAdd") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr =
        read_datum<32, 16>(cursor{"((lambda (x) (+ x 1)) 5)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 6);
}

TEST_CASE("EvalDirectTest - ApplicationAddMultiArgs") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"((lambda (x y) (+ x y)) 3 4)"sv},
                                 datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 7);
}

TEST_CASE("EvalDirectTest - ApplicationArityMismatch") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"((lambda (x y) x) 1)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    REQUIRE_FALSE(vr.has_value());
    REQUIRE(std::string_view(vr.error().message) == "arity mismatch");
}

TEST_CASE("EvalDirectTest - ApplicationNonFunction") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(1 2 3)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    REQUIRE_FALSE(vr.has_value());
    REQUIRE(std::string_view(vr.error().message) ==
            "attempted to call non-function");
}

TEST_CASE("EvalDirectTest - LexicalCapture1") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(
        cursor{"(((lambda (x) (lambda (y) (+ x y))) 3) 4)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 7);
}

TEST_CASE("EvalDirectTest - LexicalCapture2") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(
        cursor{"((lambda (x) ((lambda (y) (+ x y)) 2)) 40)"sv}, datum_arena);
    REQUIRE(dr.has_value());
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 42);
}

TEST_CASE("EvalDirectTest - QuoteAtom") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;

    auto test_eval_quote = [](std::string_view src) {
        tree_arena<datum_type<32, 16>, 32> datum_arena;
        tree_arena<core_type<32, 16>, 32> core_arena;
        auto dr = read_datum<32, 16>(cursor{src}, datum_arena);
        REQUIRE(dr.has_value());
        auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
        REQUIRE(er.has_value());
        auto const &ct = er.value();
        auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                          default_env<core_type<32, 16>, 16>());
        REQUIRE(vr.has_value());
        return vr.value();
    };

    auto sym = test_eval_quote("'x"sv);
    REQUIRE(std::holds_alternative<symbol>(sym));
    REQUIRE(std::get<symbol>(sym).name == "x"sv);

    auto num = test_eval_quote("'123"sv);
    REQUIRE(std::holds_alternative<int>(num));
    REQUIRE(std::get<int>(num) == 123);

    auto bln = test_eval_quote("'#t"sv);
    REQUIRE(std::holds_alternative<bool>(bln));
    REQUIRE(std::get<bool>(bln) == true);
}

static_assert([] {
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"'x"sv}, datum_arena);
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    return std::get<symbol>(vr.value()).name == "x"sv;
}());

TEST_CASE("EvalDirectTest - AddBadType") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    tree_arena<datum_type<32, 16>, 32> datum_arena;
    tree_arena<core_type<32, 16>, 32> core_arena;
    auto dr = read_datum<32, 16>(cursor{"(+ #t 1)"sv}, datum_arena);
    auto er = elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    auto const &ct = er.value();
    auto vr = eval_direct<32, 16, 16>(ct, core_arena,
                                      default_env<core_type<32, 16>, 16>());
    REQUIRE_FALSE(vr.has_value());
    REQUIRE(std::string_view(vr.error().message) == "type error");
}
