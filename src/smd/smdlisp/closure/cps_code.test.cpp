// src/smd/smdlisp/closure/cps_code.test.cpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/closure/cps_code.hpp>
#include <smd/smdlisp/closure/cps_code.hpp> // test 2nd include OK

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
using Store = lisp::closure::store<Core, lisp::closure::default_max_store>;
using Envs = lisp::closure::env_arena<Core, MaxBindings, MaxEnvs>;

/// Reads, elaborates and compiles @p src to CPS, then runs it under a
/// default (store-less, functional) environment sharing @p heap and @p
/// envs. Mirrors `eval_direct.test.cpp`'s `run` helper exactly, but through
/// @ref lisp::closure::compile_cps / the resulting @ref lisp::closure::cps_code
/// instead of @ref lisp::closure::eval_direct -- this is the L13 merge
/// criterion made concrete: the same source, the same arenas-owned-by-the-
/// caller discipline, a different evaluator.
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
    auto code =
        lisp::closure::compile_cps<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            root, core_arena);
    return code(environment, envs, [](Val v) -> Res { return v; });
}

/// Like @ref run, but builds a *mutable* environment (a shared @ref Store,
/// step L12) so `setq`/`defun`/`defvar`/`defparameter` can be exercised.
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
    auto code =
        lisp::closure::compile_cps<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            root, core_arena);
    return code(environment, envs, [](Val v) -> Res { return v; });
}

} // namespace

TEST_CASE("CpsCodeTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- merge criteria (docs/cl-pivot-plan.md, step L13) ------------------
// Every one of these mirrors an L11/L12 eval_direct merge-criteria
// static_assert in eval_direct.test.cpp, run through compile_cps instead.

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
    auto code =
        lisp::closure::compile_cps<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca);
    auto vr = code(environment, envs, [](Val v) -> Res { return v; });
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
    auto code =
        lisp::closure::compile_cps<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca);
    auto vr = code(environment, envs, [](Val v) -> Res { return v; });
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
    auto code =
        lisp::closure::compile_cps<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca);
    auto vr = code(environment, envs, [](Val v) -> Res { return v; });
    if (!vr.has_value() ||
        !std::holds_alternative<lisp::closure::pair_ref>(vr.value()))
        return false;
    auto const &cell =
        heap.get(std::get<lisp::closure::pair_ref>(vr.value()).loc);
    return std::holds_alternative<int>(cell.car) &&
           std::get<int>(cell.car) == 1 &&
           std::holds_alternative<lisp::closure::nil_t>(cell.cdr);
}());

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
    auto code =
        lisp::closure::compile_cps<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca);
    auto vr = code(environment, envs, [](Val v) -> Res { return v; });
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 8;
}());

// -- runtime twins of the merge criteria ---------------------------------

TEST_CASE("CpsCodeTest - IfNilTakesFalseBranch") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(if nil 1 2)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("CpsCodeTest - LambdaApplicationWithCarCdr") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r =
        run("((lambda (x) (car (cdr x))) '(1 2 3))", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("CpsCodeTest - DefunThenCallInSameProgn") {
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

// -- closures capture both namespaces (EvalDirectTest parity) -----------

TEST_CASE("CpsCodeTest - ClosureCapturesVariableNamespace") {
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

TEST_CASE("CpsCodeTest - ClosureCapturesFunctionNamespace") {
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

// -- apply / funcall (EvalDirectTest parity) -----------------------------

TEST_CASE("CpsCodeTest - ApplySpreadsFinalListArgument") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(apply #'+ 1 (list 2 3))", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 6);
}

TEST_CASE("CpsCodeTest - FuncallOfNamedFunction") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(funcall #'+ 3 4)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("CpsCodeTest - FuncallOfLambdaExpression") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(funcall (lambda (x) (* x x)) 5)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 25);
}

// -- setq shared-store-across-closures (EvalDirectTest parity) ----------

TEST_CASE("CpsCodeTest - SetqIsVisibleAcrossClosuresSharingTheBinding") {
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

TEST_CASE("CpsCodeTest - SetqExpressionValueIsTheAssignedValue") {
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

TEST_CASE("CpsCodeTest - SetqWithoutStoreIsDiagnosedError") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("((lambda (x) (setq x 2) x) 1)", da, ca, root, heap, envs);
    REQUIRE_FALSE(r.has_value());
}

// -- defvar / defparameter init-vs-no-reinit (EvalDirectTest parity) ----

TEST_CASE("CpsCodeTest - DefvarDoesNotReinitializeIfAlreadyBound") {
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

TEST_CASE("CpsCodeTest - DefparameterAlwaysReinitializes") {
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

TEST_CASE("CpsCodeTest - DefvarReturnsTheVariableName") {
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

TEST_CASE("CpsCodeTest - DefunReturnsTheFunctionName") {
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

// -- error paths ----------------------------------------------------------

TEST_CASE("CpsCodeTest - UnboundVariableIsError") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("nope", da, ca, root, heap, envs);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(std::string_view{r.error().message} == "unbound variable");
}

TEST_CASE("CpsCodeTest - UndefinedFunctionIsError") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(nope 1 2)", da, ca, root, heap, envs);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(std::string_view{r.error().message} == "undefined function");
}

TEST_CASE("CpsCodeTest - ArityMismatchIsError") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("((lambda (x y) (+ x y)) 1)", da, ca, root, heap, envs);
    REQUIRE_FALSE(r.has_value());
}

// -- block / return-from (step L14) -----------------------------------------
// -- merge criteria (docs/cl-pivot-plan.md, step L14) ------------------
// Every one of these mirrors an L14 eval_direct merge-criteria
// static_assert in eval_direct.test.cpp, run through compile_cps instead.

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
    auto code =
        lisp::closure::compile_cps<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca);
    auto vr = code(environment, envs, [](Val v) -> Res { return v; });
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
    auto code =
        lisp::closure::compile_cps<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca);
    auto vr = code(environment, envs, [](Val v) -> Res { return v; });
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 0;
}());

TEST_CASE("CpsCodeTest - BlockReturnFromSkipsRestOfBody") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(block b 1 (return-from b 42) 3)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("CpsCodeTest - BlockFallsOffEndReturnsLastValue") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(block b 1 2 3)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("CpsCodeTest - DefunImplicitBlockReturnFromExitsFunctionCall") {
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

TEST_CASE("CpsCodeTest - NestedBlockReturnFromSkipsInnerBlock") {
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

TEST_CASE("CpsCodeTest - SameNameNestedBlockReturnFromTargetsInnermost") {
    // CPS twin of eval_direct.test.cpp's same-named-nested-block test: two
    // simultaneously-live activations of block name `b`, inner return-from
    // targets its own (innermost) activation; outer falls off end at
    // (+ 100 5) = 105. The recursive form is out of reach (recursive defun
    // unsupported in this baseline evaluator; see DIV-0009).
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

TEST_CASE("CpsCodeTest - ReturnFromOfDeadBlockIsDiagnosedError") {
    // CPS twin of eval_direct.test.cpp's diagnosed dead-exit merge
    // criterion: a closure smuggles a return-from out of its block's
    // dynamic extent.
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

TEST_CASE("CpsCodeTest - ReturnFromWithoutValueReturnsNil") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(block b (return-from b))", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::nil_t>(r.value()));
}
