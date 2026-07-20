// src/smd/smdlisp/closure/eval_direct.test.cpp                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/closure/eval_direct.hpp>
#include <smd/smdlisp/closure/eval_direct.hpp> // test 2nd include OK

#include <smd/smdlisp/reader/read_datum.hpp>
#include <smd/smdscheme/parser/cursor.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
namespace lisp = smd::smdlisp;
namespace scm = smd::smdscheme;
using namespace std::string_view_literals;

constexpr int MaxNodes = 128;
constexpr int MaxList = 16;
constexpr int MaxBindings = 16;
constexpr int MaxEnvs = 32;

using Core = lisp::elaborator::core_type<MaxNodes, MaxList>;
using Val = lisp::closure::value<Core>;
using Res = scm::foundation::result<Val>;
using DatumArena =
    scm::foundation::tree_arena<lisp::reader::datum_type<MaxNodes, MaxList>,
                                MaxNodes>;
using CoreArena = scm::foundation::tree_arena<Core, MaxNodes>;
using Heap = lisp::closure::pair_heap<Core, lisp::closure::default_max_pairs>;
using Envs = lisp::closure::env_arena<Core, MaxBindings, MaxEnvs>;

/// Reads, elaborates and directly evaluates @p src under a default
/// environment sharing @p heap and @p envs.  Mirrors the elaborator's own
/// `elab()` test helper: arenas are caller-owned (passed in by reference),
/// not function-local, per L10's stack-use-after-return lesson -- extended
/// one hop further here, per that same lesson applied to the evaluator:
/// @p root receives the elaborated ROOT node (by copy) into CALLER-owned
/// storage, because a @ref lisp::closure::value can itself view into
/// whatever core node produced it (e.g. a bare keyword/symbol atom, when
/// that atom is the program's entire root and therefore not
/// @p core_arena-backed).  Returning through a function-local root (as an
/// earlier draft of this helper did) dangled exactly the way a
/// function-local datum root dangled in L10 -- caught the same way, by
/// AddressSanitizer failing `EvalDirectTest - KeywordSelfEvaluates`.
auto run(std::string_view src, DatumArena &datum_arena, CoreArena &core_arena,
         Core &root, Heap &heap, Envs &envs) -> Res {
    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{src}, datum_arena);
    if (!dr.has_value())
        return Res{dr.error()};
    auto er = lisp::elaborator::elaborate<MaxNodes, MaxList>(
        dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return Res{er.error()};
    root = er.value();
    auto environment = lisp::closure::default_env<Core, MaxBindings>(heap);
    return lisp::closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
        root, core_arena, environment, envs);
}

} // namespace

TEST_CASE("EvalDirectTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- merge criteria (docs/cl-pivot-plan.md, step L11) --------------------

static_assert([] {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Envs envs;

    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{"(if nil 1 2)"sv}, da);
    if (!dr.has_value())
        return false;
    auto er = lisp::elaborator::elaborate<MaxNodes, MaxList>(dr.value().value,
                                                             da, ca);
    if (!er.has_value())
        return false;
    auto environment = lisp::closure::default_env<Core, MaxBindings>(heap);
    auto vr =
        lisp::closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca, environment, envs);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 2;
}());

static_assert([] {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Envs envs;

    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{"((lambda (x) (car (cdr x))) '(1 2 3))"sv}, da);
    if (!dr.has_value())
        return false;
    auto er = lisp::elaborator::elaborate<MaxNodes, MaxList>(dr.value().value,
                                                             da, ca);
    if (!er.has_value())
        return false;
    auto environment = lisp::closure::default_env<Core, MaxBindings>(heap);
    auto vr =
        lisp::closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca, environment, envs);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 2;
}());

static_assert([] {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Envs envs;

    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{"(funcall #'cons 1 nil)"sv}, da);
    if (!dr.has_value())
        return false;
    auto er = lisp::elaborator::elaborate<MaxNodes, MaxList>(dr.value().value,
                                                             da, ca);
    if (!er.has_value())
        return false;
    auto environment = lisp::closure::default_env<Core, MaxBindings>(heap);
    auto vr =
        lisp::closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca, environment, envs);
    if (!vr.has_value() ||
        !std::holds_alternative<lisp::closure::pair_ref>(vr.value()))
        return false;
    auto const &cell =
        heap.get(std::get<lisp::closure::pair_ref>(vr.value()).loc);
    return std::holds_alternative<int>(cell.car) &&
           std::get<int>(cell.car) == 1 &&
           std::holds_alternative<lisp::closure::nil_t>(cell.cdr);
}());

// -- keyword self-evaluation ----------------------------------------------

TEST_CASE("EvalDirectTest - KeywordSelfEvaluates") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run(":foo", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::keyword>(r.value()));
    REQUIRE(std::get<lisp::closure::keyword>(r.value()).name == "FOO");
}

// -- nil / t truthiness -----------------------------------------------------

TEST_CASE("EvalDirectTest - NilIsFalse") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(if nil 1 2)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("EvalDirectTest - ZeroIsTrue") {
    // D3: nil is the SOLE false value; 0 is true, unlike Scheme's #f/0
    // distinction not even being a distinction (Scheme has no 0-is-false
    // convention either, but this pins the CL rule explicitly for smdlisp).
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(if 0 1 2)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 1);
}

TEST_CASE("EvalDirectTest - TEvaluatesToCanonicalTrueSymbol") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("t", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::symbol>(r.value()));
    REQUIRE(std::get<lisp::closure::symbol>(r.value()).name == "T");
}

// -- let / let* --------------------------------------------------------

TEST_CASE("EvalDirectTest - LetBindsInParallel") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(let ((x 1) (y 2)) (+ x y))", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("EvalDirectTest - LetStarSeesEarlierBindings") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(let* ((x 1) (y (+ x 1))) y)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

// -- implicit progn --------------------------------------------------------

TEST_CASE("EvalDirectTest - LambdaBodyIsImplicitProgn") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("((lambda (x) x 1 2) 99)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

// -- closures capture both namespaces ---------------------------------

TEST_CASE("EvalDirectTest - ClosureCapturesVariableNamespace") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(let ((x 10)) ((lambda (y) (+ x y)) 5))", da, ca, root, heap,
                 envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 15);
}

TEST_CASE("EvalDirectTest - ClosureCapturesFunctionNamespace") {
    // A nested lambda calling a name resolved in the FUNCTION namespace of
    // its capturing scope: CONS is a default-environment builtin reached
    // through the captured environment's function namespace, not the
    // variable namespace `x` lives in.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(let ((x 1)) ((lambda () (cons x nil))))", da, ca, root, heap,
                 envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::pair_ref>(r.value()));
    auto const &cell =
        heap.get(std::get<lisp::closure::pair_ref>(r.value()).loc);
    REQUIRE(std::get<int>(cell.car) == 1);
}

// -- apply spreads the final list argument ---------------------------------

TEST_CASE("EvalDirectTest - ApplySpreadsFinalListArgument") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(apply #'+ 1 (list 2 3))", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 6);
}

TEST_CASE("EvalDirectTest - ApplyWithJustTheListArgument") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(apply #'+ (list 1 2))", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

// -- error paths: distinct unbound-variable vs undefined-function --------

TEST_CASE("EvalDirectTest - UnboundVariableIsError") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("nope", da, ca, root, heap, envs);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(std::string_view{r.error().message} == "unbound variable");
}

TEST_CASE("EvalDirectTest - UndefinedFunctionIsError") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(nope 1 2)", da, ca, root, heap, envs);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(std::string_view{r.error().message} == "undefined function");
}

TEST_CASE("EvalDirectTest - CallingNonFunctionIsError") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(funcall 5)", da, ca, root, heap, envs);
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("EvalDirectTest - ArityMismatchIsError") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("((lambda (x y) (+ x y)) 1)", da, ca, root, heap, envs);
    REQUIRE_FALSE(r.has_value());
}

// -- funcall / #' at the evaluator level -----------------------------------

TEST_CASE("EvalDirectTest - FuncallOfNamedFunction") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(funcall #'+ 3 4)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("EvalDirectTest - FuncallOfLambdaExpression") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(funcall (lambda (x) (* x x)) 5)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 25);
}
