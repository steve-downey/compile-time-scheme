// src/smd/smdlisp/smdlisp.test.cpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/smdlisp.hpp>
#include <smd/smdlisp/smdlisp.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string_view>
#include <type_traits>
#include <variant>

namespace {
namespace lisp = smd::smdlisp;
using namespace std::string_view_literals;

// -- source_literal ------------------------------------------------------

constexpr lisp::source_literal lit{"(+ 1 2)"};

static_assert(lit.view() == "(+ 1 2)"sv);
static_assert(lit.view().size() == 7);
static_assert(lit.text[7] == '\0');

// -- compiled_lisp, arithmetic -------------------------------------------
//
// Every static_assert below is paired with a runtime TEST_CASE twin: step
// L14 established that constexpr-green is not proof of runtime correctness
// (a pointer-identity check passed under constant evaluation and failed at
// run time under Asan). The datum-arena-as-member aliasing that
// lisp_program relies on (DIV-0007) is precisely that kind of contract, so
// the runtime twins are load-bearing, not decoration.
//
// The twins are not symmetric, and deliberately so. DIV-0013: under
// `-fsanitize=undefined` (the project's default Asan config), a program
// that materializes a closure cannot be evaluated *through*
// `compiled_lisp<Source>` inside a constant expression -- the evaluator's
// `clo.node == nullptr` guard becomes a pointer comparison against the
// address of a subobject of a namespace-scope constexpr variable, which
// GCC 16 then refuses to fold. The constexpr twins for such programs
// therefore construct a function-local `lisp_program<Source>` instead;
// `compiled_lisp<Source>` itself is exercised at run time, and in constant
// evaluation for programs that build no closure.

using arith_t = lisp::lisp_program<"(+ 1 (* 2 3))">;
using Core = arith_t::core_type;

// No closure is materialized here, so the public variable template can be
// evaluated in a constant expression directly.
static_assert([] {
    arith_t::env_arena_type envs;
    auto environment = lisp::closure::default_env<Core, 16>();
    auto r = lisp::compiled_lisp<"(+ 1 (* 2 3))">(environment, envs);
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 7;
}());

// A program whose core nodes hold string_view fields viewing the datum
// arena: `x` is a core_lambda param. This is the case that dangles if the
// datum arena is not owned by the program object (DIV-0007).
constexpr auto lambda_src = "((lambda (x) (car (cdr x))) '(1 2 3))"sv;
using lambda_t = lisp::lisp_program<"((lambda (x) (car (cdr x))) '(1 2 3))">;

static_assert([] {
    lambda_t program{}; // not compiled_lisp<...>; see DIV-0013 above
    lambda_t::pair_heap_type heap;
    lambda_t::env_arena_type envs;
    auto environment = lisp::closure::default_env<Core, 16>(heap);
    auto r = program(environment, envs);
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 2;
}());

// The two namespaces (decision D4): `x` is bound as a variable and as a
// function at the same time, and both are visible in one form.
constexpr auto lisp2_src = "(progn (defvar x 10) (defun x (n) (* n 2)) "
                           "(+ x (x 3)))"sv;
using lisp2_t = lisp::lisp_program<
    "(progn (defvar x 10) (defun x (n) (* n 2)) (+ x (x 3)))">;

static_assert([] {
    lisp2_t program{}; // not compiled_lisp<...>; see DIV-0013 above
    lisp2_t::env_arena_type envs;
    auto environment = lisp::closure::default_env<Core, 16>();
    auto r = program(environment, envs);
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 16;
}());

// -- foreign_function ----------------------------------------------------

using Val = lisp::closure::value<Core>;
using Res = smd::smdscheme::foundation::result<Val>;

constexpr auto ffi_add_ten(std::span<Val const> args) -> Res {
    if (args.size() != 1)
        return smd::smdscheme::foundation::parse_error{{}, "ffi arity"};
    if (!std::holds_alternative<int>(args[0]))
        return smd::smdscheme::foundation::parse_error{{}, "ffi type"};
    return Val{std::get<int>(args[0]) + 10};
}

using ffi_t = lisp::lisp_program<"(add-ten 32)">;

static_assert([] {
    ffi_t::env_arena_type envs;
    auto environment = lisp::closure::default_env<Core, 16>();
    environment.define_function(
        lisp::closure::symbol{"ADD-TEN"},
        Val{lisp::closure::foreign_function<Core>{ffi_add_ten}});
    auto r = lisp::compiled_lisp<"(add-ten 32)">(environment, envs);
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 42;
}());

// -- lifetime contract ---------------------------------------------------
//
// lisp_program owns the datum arena its program's string_views point into,
// so copying it would leave those views aliasing the source object. The
// copy operations are deleted; guaranteed copy elision is what still lets
// `inline constexpr auto compiled_lisp = lisp_program<Source>{};` work.

static_assert(!std::is_copy_constructible_v<arith_t>);
static_assert(!std::is_copy_assignable_v<arith_t>);
static_assert(!std::is_move_constructible_v<arith_t>);
static_assert(std::is_default_constructible_v<arith_t>);

// The variable template caches: naming the same source twice yields one
// object, not two -- i.e. two occurrences of the literal produce equal
// `source_literal` template arguments and so the same specialization.
//
// Taken through named pointers rather than compared inline: written as
// `&compiled_lisp<S> == &compiled_lisp<S>` this is a syntactic
// self-comparison and GCC rejects it with -Wtautological-compare, which
// -Wall makes a diagnostic in this build.
constexpr auto const *arith_first = &lisp::compiled_lisp<"(+ 1 (* 2 3))">;
constexpr auto const *arith_second = &lisp::compiled_lisp<"(+ 1 (* 2 3))">;
static_assert(arith_first == arith_second);
static_assert(
    std::is_same_v<
        std::remove_cvref_t<decltype(lisp::compiled_lisp<"(+ 1 (* 2 3))">)>,
        arith_t>);

} // namespace

TEST_CASE("SmdlispTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("SmdlispTest - SourceLiteralViewExcludesNulTerminator") {
    REQUIRE(lit.view() == "(+ 1 2)"sv);
    REQUIRE(lit.view().size() == 7);
}

TEST_CASE("SmdlispTest - CompiledLispEvaluatesArithmetic") {
    arith_t::env_arena_type envs;
    auto environment = lisp::closure::default_env<Core, 16>();
    auto r = lisp::compiled_lisp<"(+ 1 (* 2 3))">(environment, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("SmdlispTest - CompiledLispLambdaParamNamesSurviveToRuntime") {
    INFO(lambda_src);
    lambda_t::pair_heap_type heap;
    lambda_t::env_arena_type envs;
    auto environment = lisp::closure::default_env<Core, 16>(heap);
    auto r = lisp::compiled_lisp<"((lambda (x) (car (cdr x))) '(1 2 3))">(
        environment, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("SmdlispTest - CompiledLispKeepsValueAndFunctionNamespacesApart") {
    INFO(lisp2_src);
    lisp2_t::env_arena_type envs;
    auto environment = lisp::closure::default_env<Core, 16>();
    auto r = lisp::compiled_lisp<
        "(progn (defvar x 10) (defun x (n) (* n 2)) (+ x (x 3)))">(environment,
                                                                   envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 16);
}

TEST_CASE("SmdlispTest - CompiledLispCallsForeignFunction") {
    ffi_t::env_arena_type envs;
    auto environment = lisp::closure::default_env<Core, 16>();
    environment.define_function(
        lisp::closure::symbol{"ADD-TEN"},
        Val{lisp::closure::foreign_function<Core>{ffi_add_ten}});
    auto r = lisp::compiled_lisp<"(add-ten 32)">(environment, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("SmdlispTest - CompiledLispProgramIsReusableAcrossCalls") {
    for (int i = 0; i < 3; ++i) {
        arith_t::env_arena_type envs;
        auto environment = lisp::closure::default_env<Core, 16>();
        auto r = lisp::compiled_lisp<"(+ 1 (* 2 3))">(environment, envs);
        REQUIRE(r.has_value());
        REQUIRE(std::get<int>(r.value()) == 7);
    }
}

TEST_CASE("SmdlispTest - CompiledLispReportsRuntimeErrorForUnboundName") {
    lisp::lisp_program<"(+ nope 1)">::env_arena_type envs;
    auto environment = lisp::closure::default_env<Core, 16>();
    auto r = lisp::compiled_lisp<"(+ nope 1)">(environment, envs);
    REQUIRE_FALSE(r.has_value());
}
