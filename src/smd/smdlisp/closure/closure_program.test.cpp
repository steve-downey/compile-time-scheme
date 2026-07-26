// src/smd/smdlisp/closure/closure_program.test.cpp                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/closure/closure_program.hpp>
#include <smd/smdlisp/closure/closure_program.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
namespace lisp = smd::smdlisp;
using namespace std::string_view_literals;

constexpr int MaxNodes = 128;
constexpr int MaxList = 16;

using Core = lisp::elaborator::core_type<MaxNodes, MaxList>;
using Val = lisp::closure::value<Core>;
using Heap = lisp::closure::pair_heap<Core, lisp::closure::default_max_pairs>;
using Envs = lisp::closure::env_arena<Core, 16, 32>;
using DatumArena = smd::smdscheme::foundation::tree_arena<
    lisp::reader::datum_type<MaxNodes, MaxList>, MaxNodes>;

} // namespace

TEST_CASE("ClosureProgramTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- merge criteria (docs/cl-pivot-plan.md, step L13), via compile_to_closure
//
// DIV-0007: compile_to_closure takes datum_arena by caller-owned reference
// (see closure_program.hpp's doc comment) -- every call below declares its
// own arena, kept alive alongside the program it produces.

static_assert([] {
    DatumArena datum_arena;
    auto r = lisp::closure::compile_to_closure("(if nil 1 2)"sv, datum_arena);
    if (!r.has_value())
        return false;
    auto environment = lisp::closure::default_env<Core, 16>();
    Envs envs;
    auto vr = r.value()(environment, envs);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 2;
}());

static_assert([] {
    DatumArena datum_arena;
    auto r = lisp::closure::compile_to_closure(
        "((lambda (x) (car (cdr x))) '(1 2 3))"sv, datum_arena);
    if (!r.has_value())
        return false;
    Heap heap;
    auto environment = lisp::closure::default_env<Core, 16>(heap);
    Envs envs;
    auto vr = r.value()(environment, envs);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 2;
}());

// foundation::bad source -> compile_to_closure returns a runtime error
// (unbound variable), matching smdscheme::closure_program's identical check.
static_assert([] {
    DatumArena datum_arena;
    auto r = lisp::closure::compile_to_closure("(+ nope 1)"sv, datum_arena);
    if (!r.has_value())
        return true; // elaboration error is also acceptable
    auto environment = lisp::closure::default_env<Core, 16>();
    Envs envs;
    auto vr = r.value()(environment, envs);
    return !vr.has_value();
}());

// -- append (step L18 builtin) routes through the CPS backend --------------
//
// L18's `append` landed after L13's CPS backend, leaving cps_apply's
// builtin_op switch non-exhaustive: `append` was handled in eval_direct and
// fell through to "unknown builtin" here.  It is not an isolated gap --
// backquote lowers `,@` splicing to `append` calls, so every spliced template
// evaluated correctly under the direct evaluator and failed under CPS, which
// is exactly the kind of one-backend-only divergence the parity discipline
// exists to catch.  `(append '(1) '(2))` => (1 2).
static_assert([] {
    DatumArena datum_arena;
    auto r =
        lisp::closure::compile_to_closure("(append '(1) '(2))"sv, datum_arena);
    if (!r.has_value())
        return false;
    Heap heap;
    auto environment = lisp::closure::default_env<Core, 16>(heap);
    Envs envs;
    auto vr = r.value()(environment, envs);
    if (!vr.has_value() ||
        !std::holds_alternative<lisp::closure::pair_ref>(vr.value()))
        return false;
    auto const &c0 =
        heap.get(std::get<lisp::closure::pair_ref>(vr.value()).loc);
    if (!std::holds_alternative<int>(c0.car) || std::get<int>(c0.car) != 1 ||
        !std::holds_alternative<lisp::closure::pair_ref>(c0.cdr))
        return false;
    auto const &c1 = heap.get(std::get<lisp::closure::pair_ref>(c0.cdr).loc);
    return std::holds_alternative<int>(c1.car) && std::get<int>(c1.car) == 2 &&
           std::holds_alternative<lisp::closure::nil_t>(c1.cdr);
}());

// Runtime twin of the static_assert above.  Step L14 established that
// constexpr-green is not proof of runtime correctness, so every constexpr
// contract here gets a run-time witness too.
TEST_CASE("ClosureProgramTest - AppendRoutesThroughCpsBackend") {
    DatumArena datum_arena;
    auto r =
        lisp::closure::compile_to_closure("(append '(1) '(2))"sv, datum_arena);
    REQUIRE(r.has_value());
    Heap heap;
    auto environment = lisp::closure::default_env<Core, 16>(heap);
    Envs envs;
    auto vr = r.value()(environment, envs);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::pair_ref>(vr.value()));
    auto const &c0 =
        heap.get(std::get<lisp::closure::pair_ref>(vr.value()).loc);
    REQUIRE(std::holds_alternative<int>(c0.car));
    REQUIRE(std::get<int>(c0.car) == 1);
    REQUIRE(std::holds_alternative<lisp::closure::pair_ref>(c0.cdr));
    auto const &c1 = heap.get(std::get<lisp::closure::pair_ref>(c0.cdr).loc);
    REQUIRE(std::holds_alternative<int>(c1.car));
    REQUIRE(std::get<int>(c1.car) == 2);
    REQUIRE(std::holds_alternative<lisp::closure::nil_t>(c1.cdr));
}

TEST_CASE("ClosureProgramTest - IfNilTakesFalseBranch") {
    DatumArena datum_arena;
    auto r = lisp::closure::compile_to_closure("(if nil 1 2)"sv, datum_arena);
    REQUIRE(r.has_value());
    auto environment = lisp::closure::default_env<Core, 16>();
    Envs envs;
    auto vr = r.value()(environment, envs);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 2);
}

TEST_CASE("ClosureProgramTest - LambdaApplicationWithCarCdr") {
    DatumArena datum_arena;
    auto r = lisp::closure::compile_to_closure(
        "((lambda (x) (car (cdr x))) '(1 2 3))"sv, datum_arena);
    REQUIRE(r.has_value());
    Heap heap;
    auto environment = lisp::closure::default_env<Core, 16>(heap);
    Envs envs;
    auto vr = r.value()(environment, envs);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 2);
}

TEST_CASE("ClosureProgramTest - FuncallOfConsBuildsAPair") {
    DatumArena datum_arena;
    auto r = lisp::closure::compile_to_closure("(funcall #'cons 1 nil)"sv,
                                               datum_arena);
    REQUIRE(r.has_value());
    Heap heap;
    auto environment = lisp::closure::default_env<Core, 16>(heap);
    Envs envs;
    auto vr = r.value()(environment, envs);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::pair_ref>(vr.value()));
}

TEST_CASE(
    "ClosureProgramTest - CanBeCalledMultipleTimesWithFreshEnvironments") {
    // The returned program is a value that can be reused; each call gets
    // its own environment/env_arena, per this type's own doc comment.
    DatumArena datum_arena;
    auto r = lisp::closure::compile_to_closure("(+ 1 2)"sv, datum_arena);
    REQUIRE(r.has_value());

    auto env1 = lisp::closure::default_env<Core, 16>();
    Envs envs1;
    auto vr1 = r.value()(env1, envs1);
    REQUIRE(vr1.has_value());
    REQUIRE(std::get<int>(vr1.value()) == 3);

    auto env2 = lisp::closure::default_env<Core, 16>();
    Envs envs2;
    auto vr2 = r.value()(env2, envs2);
    REQUIRE(vr2.has_value());
    REQUIRE(std::get<int>(vr2.value()) == 3);
}

TEST_CASE("ClosureProgramTest - UnboundVariableError") {
    DatumArena datum_arena;
    auto r = lisp::closure::compile_to_closure("(+ nope 1)"sv, datum_arena);
    if (!r.has_value())
        return; // error at elaboration is also acceptable
    auto environment = lisp::closure::default_env<Core, 16>();
    Envs envs;
    auto vr = r.value()(environment, envs);
    REQUIRE_FALSE(vr.has_value());
}
