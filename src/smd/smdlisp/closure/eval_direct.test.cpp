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

using Store = lisp::closure::store<Core, lisp::closure::default_max_store>;

/// Like @ref run, but builds a *mutable* environment (a shared @ref Store,
/// step L12) so `setq`/`defun`/`defvar`/`defparameter` can be exercised.
/// Follows the identical caller-owns-the-root discipline @ref run does.
auto run_mut(std::string_view src, DatumArena &datum_arena,
             CoreArena &core_arena, Core &root, Heap &heap, Store &store,
             Envs &envs) -> Res {
    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{src}, datum_arena);
    if (!dr.has_value())
        return Res{dr.error()};
    auto er = lisp::elaborator::elaborate<MaxNodes, MaxList>(
        dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return Res{er.error()};
    root = er.value();
    auto environment =
        lisp::closure::default_env<Core, MaxBindings>(heap, store);
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

// -- merge criteria (docs/cl-pivot-plan.md, step L12) ----------------------

static_assert([] {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Envs envs;

    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{"(progn (defun twice (x) (+ x x)) (twice 4))"sv},
        da);
    if (!dr.has_value())
        return false;
    auto er = lisp::elaborator::elaborate<MaxNodes, MaxList>(dr.value().value,
                                                             da, ca);
    if (!er.has_value())
        return false;
    auto environment =
        lisp::closure::default_env<Core, MaxBindings>(heap, store);
    auto vr =
        lisp::closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca, environment, envs);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 8;
}());

TEST_CASE("EvalDirectTest - DefunThenCallInSameProgn") {
    // Runtime twin of the merge-criteria static_assert above.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("(progn (defun twice (x) (+ x x)) (twice 4))", da, ca,
                     root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 8);
}

TEST_CASE("EvalDirectTest - DefunReturnsTheFunctionName") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("(defun f (x) x)", da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::symbol>(r.value()));
    REQUIRE(std::get<lisp::closure::symbol>(r.value()).name == "F");
}

TEST_CASE("EvalDirectTest - DefunDefinedFunctionIsCallableViaFuncallAndApply") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("(progn (defun sq (x) (* x x)) (funcall #'sq 5))", da, ca,
                     root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 25);

    DatumArena da2;
    CoreArena ca2;
    Heap heap2;
    Store store2;
    Core root2;
    Envs envs2;
    auto r2 = run_mut("(progn (defun sq (x) (* x x)) (apply #'sq (list 6)))",
                      da2, ca2, root2, heap2, store2, envs2);
    REQUIRE(r2.has_value());
    REQUIRE(std::get<int>(r2.value()) == 36);
}

// -- setq -------------------------------------------------------------------

TEST_CASE("EvalDirectTest - SetqMutatesNearestLexicalBindingAndReturnsValue") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("((lambda (x) (setq x 2) x) 1)", da, ca, root, heap, store,
                     envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("EvalDirectTest - SetqExpressionValueIsTheAssignedValue") {
    // ANSI CL: setq's own value is the assigned value, not an unspecified
    // marker (the Scheme `set!` convention this step's machinery was
    // adapted from).
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("((lambda (x) (setq x 42)) 1)", da, ca, root, heap, store,
                     envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("EvalDirectTest - SetqOfUnboundIsDiagnosedError") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("(setq nope 1)", da, ca, root, heap, store, envs);
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("EvalDirectTest - SetqWithoutStoreIsDiagnosedError") {
    // The plain (store-less) default_env overload is functional-mode:
    // setq is a diagnosed error rather than a silent no-op.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("((lambda (x) (setq x 2) x) 1)", da, ca, root, heap, envs);
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("EvalDirectTest - SetqMultiplePairsAssignsLeftToRightReturnsLast") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("((lambda (x y) (setq x 10 y 20) (+ x y)) 1 2)", da, ca,
                     root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 30);
}

TEST_CASE("EvalDirectTest - SetqIsVisibleAcrossClosuresSharingTheBinding") {
    // The classic shared-mutable-closure idiom: a `store` is shared across
    // every env copy, including closure captures, so `setq` inside one
    // closure call is visible to a *later* call of a (different) closure
    // sharing the same binding -- not just within a single call's own
    // sequential evaluation. This is exactly why L12 adapts a store rather
    // than mutating a per-copy inline value: `inc`'s two calls must
    // observe each other's mutation of the shared `x`.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r =
        run_mut("(let ((x 1)) (progn (defun inc () (setq x (+ x 1))) (funcall "
                "#'inc) (funcall #'inc) x))",
                da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

// -- defvar / defparameter ---------------------------------------------------

TEST_CASE("EvalDirectTest - DefvarInitializesAndMarksSpecial") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r =
        run_mut("(progn (defvar *x* 1) *x*)", da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 1);
}

TEST_CASE("EvalDirectTest - DefvarDoesNotReinitializeIfAlreadyBound") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("(progn (defvar *x* 1) (setq *x* 2) (defvar *x* 99) *x*)",
                     da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("EvalDirectTest - DefparameterAlwaysReinitializes") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut(
        "(progn (defparameter *x* 1) (setq *x* 2) (defparameter *x* 99) "
        "*x*)",
        da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 99);
}

TEST_CASE("EvalDirectTest - DefvarReturnsTheVariableName") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("(defvar *x* 1)", da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::symbol>(r.value()));
    REQUIRE(std::get<lisp::closure::symbol>(r.value()).name == "*X*");
}

TEST_CASE("EvalDirectTest - DefvarWithoutInitOnlyMarksSpecial") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    // No init form: *x* stays unbound; referencing it is still an error.
    auto r =
        run_mut("(progn (defvar *x*) *x*)", da, ca, root, heap, store, envs);
    REQUIRE_FALSE(r.has_value());
}

// -- block / return-from (step L14) -----------------------------------------
// -- merge criteria (docs/cl-pivot-plan.md, step L14) ------------------

static_assert([] {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Envs envs;

    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{"(block b 1 (return-from b 42) 3)"sv}, da);
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
           std::get<int>(vr.value()) == 42;
}());

static_assert([] {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Envs envs;

    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{
            "(progn (defun f (x) (if x (return-from f 0) 1)) (f t))"sv},
        da);
    if (!dr.has_value())
        return false;
    auto er = lisp::elaborator::elaborate<MaxNodes, MaxList>(dr.value().value,
                                                             da, ca);
    if (!er.has_value())
        return false;
    auto environment =
        lisp::closure::default_env<Core, MaxBindings>(heap, store);
    auto vr =
        lisp::closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca, environment, envs);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 0;
}());

TEST_CASE("EvalDirectTest - BlockReturnFromSkipsRestOfBody") {
    // Runtime twin of the first L14 merge-criteria static_assert above.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(block b 1 (return-from b 42) 3)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("EvalDirectTest - BlockFallsOffEndReturnsLastValue") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(block b 1 2 3)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("EvalDirectTest - BlockWithNoFormsEvaluatesToNil") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(block b)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::nil_t>(r.value()));
}

TEST_CASE("EvalDirectTest - DefunImplicitBlockReturnFromExitsFunctionCall") {
    // Runtime twin of the second L14 merge-criteria static_assert above,
    // exercising both the true and false arms of (f x).
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("(progn (defun f (x) (if x (return-from f 0) 1)) (f t))",
                     da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 0);

    DatumArena da2;
    CoreArena ca2;
    Heap heap2;
    Store store2;
    Core root2;
    Envs envs2;
    auto r2 =
        run_mut("(progn (defun f (x) (if x (return-from f 0) 1)) (f nil))", da2,
                ca2, root2, heap2, store2, envs2);
    REQUIRE(r2.has_value());
    REQUIRE(std::get<int>(r2.value()) == 1);
}

TEST_CASE("EvalDirectTest - NestedBlockReturnFromSkipsInnerBlock") {
    // return-from targeting the OUTER block transfers control straight past
    // the still-live inner block, which never falls off its own end.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(block outer (block inner (return-from outer 5)) 99)", da, ca,
                 root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 5);
}

TEST_CASE("EvalDirectTest - SameNameNestedBlockReturnFromTargetsInnermost") {
    // Two simultaneously-live activations of the *same* block name `b`, each
    // with its own exit_record allocated fresh per activation (per env.hpp's
    // docs): the inner `return-from b` must target the innermost (its own)
    // activation, so the inner block resolves to 5 and the outer block --
    // still live -- falls off its end returning (+ 100 5) = 105. Nesting
    // depth must not confuse one activation's exit_record with another's.
    //
    // This is the non-recursive witness of the own-activation property. The
    // recursive form (a function calling itself across a `block`) is out of
    // reach because recursive `defun` is unsupported in this baseline
    // evaluator -- a closure captures a copy of the environment before its
    // own name is bound in the function namespace (see DIV-0009).
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(block b (+ 100 (block b (return-from b 5))))", da, ca, root,
                 heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 105);
}

TEST_CASE("EvalDirectTest - ReturnFromOfDeadBlockIsDiagnosedError") {
    // The diagnosed-error merge criterion (docs/cl-pivot-plan.md, step L14):
    // a closure smuggles a return-from out of its block's dynamic extent.
    // `make-escaper`'s block B returns the closure itself (its last body
    // form), so B's own evaluation -- and therefore B's exit_record -- has
    // already completed by the time the closure is later invoked.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut(
        "(progn (defun make-escaper () (block b (lambda () (return-from b "
        "42)))) (funcall (make-escaper)))",
        da, ca, root, heap, store, envs);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(std::string_view{r.error().message} ==
            "return-from: block has already exited");
}

TEST_CASE("EvalDirectTest - ReturnFromWithoutValueReturnsNil") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(block b (return-from b))", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::nil_t>(r.value()));
}

// -- catch / throw / unwind-protect (step L15) -------------------------------
// -- merge criteria (docs/cl-pivot-plan.md, step L15) ------------------

/// The cleanup-ordering witness, used by both the constexpr and the runtime
/// halves of the first merge criterion below.
///
/// `log` accumulates base-10 digits in the order the cleanups run, so
/// innermost-first yields 12 and outermost-first would yield 21 -- the two
/// orders are distinguishable, not merely "both cleanups ran".
constexpr auto cleanup_order_src =
    "(let ((log 0))"
    "  (catch 'c"
    "    (unwind-protect"
    "      (unwind-protect (throw 'c 99) (setq log (+ (* log 10) 1)))"
    "      (setq log (+ (* log 10) 2))))"
    "  log)"sv;

/// The throw-through-nested-unwind-protect witness: the thrown value must
/// reach the `catch` (99) *and* both cleanups must have already run by the
/// time it does (log = 1), which `+` observes by evaluating its arguments
/// left to right.
constexpr auto throw_through_unwind_src =
    "(let ((log 0))"
    "  (+ (catch 'c (unwind-protect (throw 'c 99) (setq log 1))) log))"sv;

static_assert([] {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Envs envs;

    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{cleanup_order_src}, da);
    if (!dr.has_value())
        return false;
    auto er = lisp::elaborator::elaborate<MaxNodes, MaxList>(dr.value().value,
                                                             da, ca);
    if (!er.has_value())
        return false;
    auto environment =
        lisp::closure::default_env<Core, MaxBindings>(heap, store);
    auto vr =
        lisp::closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca, environment, envs);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 12;
}());

static_assert([] {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Envs envs;

    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{throw_through_unwind_src}, da);
    if (!dr.has_value())
        return false;
    auto er = lisp::elaborator::elaborate<MaxNodes, MaxList>(dr.value().value,
                                                             da, ca);
    if (!er.has_value())
        return false;
    auto environment =
        lisp::closure::default_env<Core, MaxBindings>(heap, store);
    auto vr =
        lisp::closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca, environment, envs);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 100;
}());

static_assert([] {
    // Uncaught throw: a diagnosed error, not UB and not a silent no-op.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Envs envs;

    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{"(catch 'a (throw 'b 1))"sv}, da);
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
    return !vr.has_value();
}());

TEST_CASE("EvalDirectTest - UnwindProtectCleanupsRunInnermostFirst") {
    // Runtime twin of the first L15 merge-criteria static_assert above. The
    // twin is not redundant: L14 proved a constexpr-green unwind mechanism
    // can still fail at run time (env.hpp's marker-identity note).
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut(cleanup_order_src, da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 12);
}

TEST_CASE("EvalDirectTest - ThrowThroughNestedUnwindProtectRunsCleanups") {
    // Runtime twin of the second L15 merge-criteria static_assert above.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut(throw_through_unwind_src, da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 100);
}

TEST_CASE("EvalDirectTest - UncaughtThrowIsDiagnosedError") {
    // Runtime twin of the third L15 merge-criteria static_assert above,
    // in both the no-catch-at-all and the wrong-tag shapes.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(throw 'c 1)", da, ca, root, heap, envs);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(std::string_view{r.error().message} == "throw: no catch for tag");

    DatumArena da2;
    CoreArena ca2;
    Heap heap2;
    Core root2;
    Envs envs2;
    auto r2 = run("(catch 'a (throw 'b 1))", da2, ca2, root2, heap2, envs2);
    REQUIRE_FALSE(r2.has_value());
    REQUIRE(std::string_view{r2.error().message} == "throw: no catch for tag");
}

// -- catch / throw behaviour ------------------------------------------------

TEST_CASE("EvalDirectTest - CatchReturnsThrownValue") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(catch 'c 1 (throw 'c 42) 3)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("EvalDirectTest - CatchFallsOffEndReturnsLastValue") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(catch 'c 1 2 3)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("EvalDirectTest - CatchWithNoFormsEvaluatesToNil") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(catch 'c)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::nil_t>(r.value()));
}

TEST_CASE("EvalDirectTest - ThrowSkipsCatchWithNonMatchingTag") {
    // The dynamic search is by `eq` on the evaluated tag, innermost-first:
    // the inner `(catch 'b ...)` is passed straight through.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r =
        run("(catch 'a (catch 'b (throw 'a 7)) 99)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("EvalDirectTest - SameTagNestedCatchTargetsInnermost") {
    // The dynamic-exit counterpart of L14's
    // SameNameNestedBlockReturnFromTargetsInnermost: two simultaneously
    // active frames carrying the SAME tag, so the throw must reach the
    // innermost one and the outer frame must fall off its own end.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(catch 'c (+ 100 (catch 'c (throw 'c 5))))", da, ca, root,
                 heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 105);
}

TEST_CASE("EvalDirectTest - CatchTagIsAnEvaluatedValue") {
    // Not a name: a variable holding the tag works, and matching is `eq`.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("(let ((tag 'c)) (catch tag (throw tag 8)))", da, ca, root,
                     heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 8);
}

// -- unwind-protect behaviour -----------------------------------------------

TEST_CASE("EvalDirectTest - UnwindProtectReturnsProtectedFormValue") {
    // Cleanup values are discarded, even when a cleanup is the last thing
    // evaluated.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(unwind-protect 7 8 9)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("EvalDirectTest - UnwindProtectCleanupRunsOnNormalCompletion") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("(let ((log 0)) (unwind-protect 7 (setq log 1)) log)", da,
                     ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 1);
}

TEST_CASE("EvalDirectTest - UnwindProtectCleanupRunsOnReturnFromPath") {
    // The third exit path (after normal completion and `throw`): a lexical
    // `return-from` unwind passing through the protected extent.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r =
        run_mut("(let ((log 0)) (block b (unwind-protect (return-from b 7) "
                "(setq log 1))) log)",
                da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 1);

    // ...and the return-from still delivers its own value to the block.
    DatumArena da2;
    CoreArena ca2;
    Heap heap2;
    Core root2;
    Envs envs2;
    auto r2 = run("(block b (unwind-protect (return-from b 7) 0))", da2, ca2,
                  root2, heap2, envs2);
    REQUIRE(r2.has_value());
    REQUIRE(std::get<int>(r2.value()) == 7);
}

TEST_CASE("EvalDirectTest - ReturnFromUnwindsThroughCatchFrame") {
    // A `catch` must let a `return-from` unwind pass through it untouched
    // (the two markers are distinct objects precisely so neither mechanism
    // intercepts the other), and must still pop its own dynamic frame on
    // that path.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(block b (catch 'c (return-from b 3)) 99)", da, ca, root,
                 heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
    REQUIRE(envs.catch_depth() == 0);
}

TEST_CASE("EvalDirectTest - EveryCatchExitPathPopsTheDynamicStack") {
    // Normal completion, a caught throw, and an uncaught throw must each
    // leave the dynamic stack balanced -- otherwise a later `throw` could
    // find a frame whose extent has already ended.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    REQUIRE(run("(catch 'c 1)", da, ca, root, heap, envs).has_value());
    REQUIRE(envs.catch_depth() == 0);

    DatumArena da2;
    CoreArena ca2;
    Heap heap2;
    Core root2;
    Envs envs2;
    REQUIRE(run("(catch 'c (throw 'c 1))", da2, ca2, root2, heap2, envs2)
                .has_value());
    REQUIRE(envs2.catch_depth() == 0);

    DatumArena da3;
    CoreArena ca3;
    Heap heap3;
    Core root3;
    Envs envs3;
    REQUIRE_FALSE(run("(catch 'a (throw 'b 1))", da3, ca3, root3, heap3, envs3)
                      .has_value());
    REQUIRE(envs3.catch_depth() == 0);
}

// -- special variables / dynamic binding (step L16) --------------------------
// -- merge criteria (docs/cl-pivot-plan.md, step L16) ------------------

/// The classic dynamic-vs-lexical witness, verbatim from the plan: `get-x`
/// closed over `*x*` long before the `let` ran, and `let` is lowered to a
/// lambda application, so under purely lexical scoping `(get-x)` could only
/// ever answer 1. It answers 2 exactly because `*x*` was marked special.
constexpr auto dynamic_binding_src = "(progn"
                                     "  (defvar *x* 1)"
                                     "  (defun get-x () *x*)"
                                     "  (let ((*x* 2)) (get-x)))"sv;

/// The control for the witness above: the identical program shape over a
/// name that was never marked special still binds lexically, so the inner
/// `let` is invisible to `get-y` and the answer is the outer 1. Without this
/// half, "returns 2" would be equally consistent with having broken `let`
/// into a global assignment.
constexpr auto lexical_contrast_src = "(let ((y 1))"
                                      "  (defun get-y () y)"
                                      "  (let ((y 2)) (get-y)))"sv;

/// The restore-on-nonlocal-exit witness: `throw` leaves the dynamic extent
/// without ever reaching the end of the `let` body, and `+` (which evaluates
/// its arguments left to right) then observes that `*x*` is 1 again. 2 + 1.
constexpr auto throw_past_dynamic_src =
    "(progn"
    "  (defvar *x* 1)"
    "  (defun get-x () *x*)"
    "  (+ (catch 'c (let ((*x* 2)) (throw 'c (get-x)))) (get-x)))"sv;

static_assert([] {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Envs envs;

    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{dynamic_binding_src}, da);
    if (!dr.has_value())
        return false;
    auto er = lisp::elaborator::elaborate<MaxNodes, MaxList>(dr.value().value,
                                                             da, ca);
    if (!er.has_value())
        return false;
    auto environment =
        lisp::closure::default_env<Core, MaxBindings>(heap, store);
    auto vr =
        lisp::closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca, environment, envs);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 2 && envs.dynamic_depth() == 0;
}());

static_assert([] {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Envs envs;

    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{lexical_contrast_src}, da);
    if (!dr.has_value())
        return false;
    auto er = lisp::elaborator::elaborate<MaxNodes, MaxList>(dr.value().value,
                                                             da, ca);
    if (!er.has_value())
        return false;
    auto environment =
        lisp::closure::default_env<Core, MaxBindings>(heap, store);
    auto vr =
        lisp::closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca, environment, envs);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 1 && envs.dynamic_depth() == 0;
}());

static_assert([] {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Envs envs;

    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{throw_past_dynamic_src}, da);
    if (!dr.has_value())
        return false;
    auto er = lisp::elaborator::elaborate<MaxNodes, MaxList>(dr.value().value,
                                                             da, ca);
    if (!er.has_value())
        return false;
    auto environment =
        lisp::closure::default_env<Core, MaxBindings>(heap, store);
    auto vr =
        lisp::closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca, environment, envs);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 3 && envs.dynamic_depth() == 0;
}());

TEST_CASE("EvalDirectTest - SpecialLetBindsDynamically") {
    // Runtime twin of the first L16 merge-criteria static_assert above. The
    // twin is not optional: L14 proved a constexpr-green mechanism can still
    // be broken at run time (env.hpp's marker-identity note).
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut(dynamic_binding_src, da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
    REQUIRE(envs.dynamic_depth() == 0);
}

TEST_CASE("EvalDirectTest - NonSpecialLetStillBindsLexically") {
    // Runtime twin of the second L16 merge-criteria static_assert above.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut(lexical_contrast_src, da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 1);
    REQUIRE(envs.dynamic_depth() == 0);
}

TEST_CASE("EvalDirectTest - ThrowPastDynamicBindingRestoresOuterValue") {
    // Runtime twin of the third L16 merge-criteria static_assert above.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut(throw_past_dynamic_src, da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
    REQUIRE(envs.dynamic_depth() == 0);
}

// -- dynamic binding behaviour ----------------------------------------------

TEST_CASE("EvalDirectTest - ReturnFromPastDynamicBindingRestoresOuterValue") {
    // The `block`/`return-from` twin of the throw criterion: the other
    // nonlocal exit L14/L15 built must restore too. 2 + 1.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("(progn (defvar *x* 1) (defun get-x () *x*)"
                     "  (+ (block b (let ((*x* 2)) (return-from b (get-x))))"
                     "     (get-x)))",
                     da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
    REQUIRE(envs.dynamic_depth() == 0);
}

TEST_CASE("EvalDirectTest - SetqOfSpecialAssignsInnermostDynamicBinding") {
    // `setq` inside the extent must hit the dynamic binding, not the outer
    // value: 3 inside, 1 again outside. 3 + 1.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("(progn (defvar *x* 1) (defun get-x () *x*)"
                     "  (+ (let ((*x* 2)) (setq *x* 3) (get-x)) (get-x)))",
                     da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 4);
    REQUIRE(envs.dynamic_depth() == 0);
}

TEST_CASE("EvalDirectTest - NestedDynamicBindingsRestoreInnermostFirst") {
    // (3 + 2) + 1: each extent restores exactly the value it displaced.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("(progn (defvar *x* 1) (defun get-x () *x*)"
                     "  (+ (let ((*x* 2)) (+ (let ((*x* 3)) (get-x)) (get-x)))"
                     "     (get-x)))",
                     da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 6);
    REQUIRE(envs.dynamic_depth() == 0);
}

TEST_CASE("EvalDirectTest - SpecialLambdaParameterBindsDynamically") {
    // The mark applies to every binding of the name, not just `let`: a
    // `defun` parameter spelled like a special binds dynamically too. 7 + 1.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("(progn (defvar *x* 1) (defun get-x () *x*)"
                     "  (defun call-x (*x*) (get-x))"
                     "  (+ (call-x 7) (get-x)))",
                     da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 8);
    REQUIRE(envs.dynamic_depth() == 0);
}

TEST_CASE("EvalDirectTest - DynamicBindingOfUnboundSpecialIsDiagnosedError") {
    // DIV-0014: shallow binding has no cell to save, so this is diagnosed
    // rather than establishing a fresh binding as ANSI CL would.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("(progn (defvar *x*) (let ((*x* 2)) *x*))", da, ca, root,
                     heap, store, envs);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(std::string_view{r.error().message} ==
            "dynamic binding: special variable is unbound");
    REQUIRE(envs.dynamic_depth() == 0);
}

TEST_CASE("EvalDirectTest - EveryDynamicExitPathRestoresTheBindingStack") {
    // Normal completion, a nonlocal exit through the extent, and an error
    // raised inside it must each leave the stack balanced -- otherwise a
    // later restore would write a stale value into a live cell.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    REQUIRE(run_mut("(progn (defvar *x* 1) (let ((*x* 2)) *x*))", da, ca, root,
                    heap, store, envs)
                .has_value());
    REQUIRE(envs.dynamic_depth() == 0);

    DatumArena da2;
    CoreArena ca2;
    Heap heap2;
    Store store2;
    Core root2;
    Envs envs2;
    REQUIRE(run_mut("(progn (defvar *x* 1)"
                    "  (catch 'c (let ((*x* 2)) (throw 'c 9))))",
                    da2, ca2, root2, heap2, store2, envs2)
                .has_value());
    REQUIRE(envs2.dynamic_depth() == 0);

    DatumArena da3;
    CoreArena ca3;
    Heap heap3;
    Store store3;
    Core root3;
    Envs envs3;
    REQUIRE_FALSE(run_mut("(progn (defvar *x* 1) (let ((*x* 2)) (nope)))", da3,
                          ca3, root3, heap3, store3, envs3)
                      .has_value());
    REQUIRE(envs3.dynamic_depth() == 0);
}
