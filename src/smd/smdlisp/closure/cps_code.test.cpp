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
/// Step L20 made CPS continuations n-ary: a continuation is handed the
/// form's whole value list, not one value.  These two aliases and the
/// @ref primary helper are all this file needs to keep the pre-L20 tests
/// reading exactly as they did.
using Vals = lisp::closure::value_list<Core, lisp::closure::default_max_values>;
using ResV = scm::foundation::result<Vals>;

/// Runs @p code to completion and narrows the result to its primary value.
constexpr auto primary(ResV const &r) -> Res {
    if (!r.has_value())
        return Res{r.error()};
    return lisp::closure::primary_value(r.value());
}
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
    return primary(code(environment, envs, [](Vals v) -> ResV { return v; }));
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
    return primary(code(environment, envs, [](Vals v) -> ResV { return v; }));
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
    auto vr =
        primary(code(environment, envs, [](Vals v) -> ResV { return v; }));
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
    auto vr =
        primary(code(environment, envs, [](Vals v) -> ResV { return v; }));
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
    auto vr =
        primary(code(environment, envs, [](Vals v) -> ResV { return v; }));
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
    auto vr =
        primary(code(environment, envs, [](Vals v) -> ResV { return v; }));
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
    auto vr =
        primary(code(environment, envs, [](Vals v) -> ResV { return v; }));
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
    auto vr =
        primary(code(environment, envs, [](Vals v) -> ResV { return v; }));
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

// -- catch / throw / unwind-protect (step L15) -------------------------------
// -- merge criteria (docs/cl-pivot-plan.md, step L15) ------------------
// The CPS twins of eval_direct.test.cpp's L15 merge criteria: same source,
// same expected values, the other evaluator. Parity between the two
// backends is the standing requirement, not an optional extra.

/// See eval_direct.test.cpp's identically-named constant for what this
/// program witnesses (innermost cleanup first => 12, not 21).
constexpr auto cleanup_order_src =
    "(let ((log 0))"
    "  (catch 'c"
    "    (unwind-protect"
    "      (unwind-protect (throw 'c 99) (setq log (+ (* log 10) 1)))"
    "      (setq log (+ (* log 10) 2))))"
    "  log)"sv;

/// See eval_direct.test.cpp's identically-named constant: the thrown value
/// reaches the catch (99) with both cleanups already run (log = 1).
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
    auto code =
        lisp::closure::compile_cps<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca);
    auto vr = primary(
        code(environment, envs, lisp::closure::detail::identity_k<Core>{}));
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
    auto code =
        lisp::closure::compile_cps<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca);
    auto vr = primary(
        code(environment, envs, lisp::closure::detail::identity_k<Core>{}));
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 100;
}());

static_assert([] {
    // Uncaught throw: a diagnosed error under CPS too -- neither cont nor k
    // runs, because there is no value to hand onward.
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
    auto code =
        lisp::closure::compile_cps<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca);
    auto vr = primary(
        code(environment, envs, lisp::closure::detail::identity_k<Core>{}));
    return !vr.has_value();
}());

TEST_CASE("CpsCodeTest - UnwindProtectCleanupsRunInnermostFirst") {
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

TEST_CASE("CpsCodeTest - ThrowThroughNestedUnwindProtectRunsCleanups") {
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

TEST_CASE("CpsCodeTest - UncaughtThrowIsDiagnosedError") {
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

TEST_CASE("CpsCodeTest - CatchReturnsThrownValue") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(catch 'c 1 (throw 'c 42) 3)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("CpsCodeTest - CatchFallsOffEndReturnsLastValue") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(catch 'c 1 2 3)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("CpsCodeTest - CatchWithNoFormsEvaluatesToNil") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(catch 'c)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::nil_t>(r.value()));
}

TEST_CASE("CpsCodeTest - ThrowSkipsCatchWithNonMatchingTag") {
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

TEST_CASE("CpsCodeTest - SameTagNestedCatchTargetsInnermost") {
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

TEST_CASE("CpsCodeTest - CatchTagIsAnEvaluatedValue") {
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

TEST_CASE("CpsCodeTest - UnwindProtectReturnsProtectedFormValue") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(unwind-protect 7 8 9)", da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("CpsCodeTest - UnwindProtectCleanupRunsOnNormalCompletion") {
    // The CPS-specific half of the parity claim: the protected form is a
    // continuation barrier, so the cleanup runs before cont/k see the value
    // on the NORMAL path too, not only on an escape path.
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

TEST_CASE("CpsCodeTest - UnwindProtectCleanupRunsOnReturnFromPath") {
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

TEST_CASE("CpsCodeTest - ReturnFromUnwindsThroughCatchFrame") {
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

TEST_CASE("CpsCodeTest - EveryCatchExitPathPopsTheDynamicStack") {
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
// Parity halves of `eval_direct.test.cpp`'s L16 merge criteria: the same
// three sources, the same three answers, through the CPS backend. The
// interesting difference is invisible from here -- applying a closure is a
// continuation barrier under CPS (see cps_apply_function_value), because a
// tail-passed continuation would otherwise run while the dynamic bindings
// were still in effect.

/// See `eval_direct.test.cpp` for what each of these three witnesses.
constexpr auto dynamic_binding_src = "(progn"
                                     "  (defvar *x* 1)"
                                     "  (defun get-x () *x*)"
                                     "  (let ((*x* 2)) (get-x)))"sv;

constexpr auto lexical_contrast_src = "(let ((y 1))"
                                      "  (defun get-y () y)"
                                      "  (let ((y 2)) (get-y)))"sv;

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
    auto code =
        lisp::closure::compile_cps<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca);
    auto vr =
        primary(code(environment, envs, [](Vals v) -> ResV { return v; }));
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
    auto code =
        lisp::closure::compile_cps<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca);
    auto vr =
        primary(code(environment, envs, [](Vals v) -> ResV { return v; }));
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
    auto code =
        lisp::closure::compile_cps<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca);
    auto vr =
        primary(code(environment, envs, [](Vals v) -> ResV { return v; }));
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 3 && envs.dynamic_depth() == 0;
}());

TEST_CASE("CpsCodeTest - SpecialLetBindsDynamically") {
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

TEST_CASE("CpsCodeTest - NonSpecialLetStillBindsLexically") {
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

TEST_CASE("CpsCodeTest - ThrowPastDynamicBindingRestoresOuterValue") {
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

TEST_CASE("CpsCodeTest - ReturnFromPastDynamicBindingRestoresOuterValue") {
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

TEST_CASE("CpsCodeTest - SetqOfSpecialAssignsInnermostDynamicBinding") {
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

TEST_CASE("CpsCodeTest - NestedDynamicBindingsRestoreInnermostFirst") {
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

TEST_CASE("CpsCodeTest - SpecialLambdaParameterBindsDynamically") {
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

TEST_CASE("CpsCodeTest - DynamicBindingOfUnboundSpecialIsDiagnosedError") {
    // DIV-0014, under the CPS backend too.
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

TEST_CASE("CpsCodeTest - EveryDynamicExitPathRestoresTheBindingStack") {
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

// -- multiple values (step L20) ---------------------------------------------
// -- merge criteria (docs/cl-pivot-plan.md, step L20) ------------------
//
// The same set eval_direct.test.cpp asserts, run through compile_cps -- the
// standing L13 parity criterion, extended to the n-ary continuation.

namespace {

/// Runs @p src through the CPS backend and returns the terminal
/// continuation's whole value list, not just its primary value.
auto run_values(std::string_view src, DatumArena &datum_arena,
                CoreArena &core_arena, Core &root, Heap &heap, Store &store,
                Envs &envs) -> ResV {
    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{src}, datum_arena);
    if (!dr.has_value())
        return ResV{dr.error()};
    auto er = lisp::elaborator::elaborate<MaxNodes, MaxList>(
        dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return ResV{er.error()};
    root = er.value();
    auto environment =
        lisp::closure::default_env<Core, MaxBindings>(heap, store);
    auto code =
        lisp::closure::compile_cps<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            root, core_arena);
    return code(environment, envs, lisp::closure::detail::identity_k<Core>{});
}

constexpr auto mvb_sum_src =
    "(multiple-value-bind (a b) (values 1 2) (+ a b))"sv;
constexpr auto values_primary_src = "(+ (values 1 2) 10)"sv;

} // namespace

static_assert([] {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Envs envs;

    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{mvb_sum_src}, da);
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
    auto vr = primary(
        code(environment, envs, lisp::closure::detail::identity_k<Core>{}));
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 3;
}());

static_assert([] {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Envs envs;

    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{values_primary_src}, da);
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
    auto vr = primary(
        code(environment, envs, lisp::closure::detail::identity_k<Core>{}));
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 11;
}());

TEST_CASE("CpsCodeTest - MultipleValueBindSumsTwoValues") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run(mvb_sum_src, da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("CpsCodeTest - ValuesInSingleValueContextYieldsPrimary") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run(values_primary_src, da, ca, root, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 11);
}

TEST_CASE("CpsCodeTest - ContinuationReceivesEveryValue") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_values("(values 1 2 3)", da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(r.value().size() == 3);
    REQUIRE(std::get<int>(r.value()[2]) == 3);
}

TEST_CASE("CpsCodeTest - ZeroValuesReadAsNilInSingleValueContext") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto n = run_values("(values)", da, ca, root, heap, store, envs);
    REQUIRE(n.has_value());
    REQUIRE(n.value().empty());

    DatumArena da2;
    CoreArena ca2;
    Heap heap2;
    Core root2;
    Envs envs2;
    auto r = run("(if (values) 1 2)", da2, ca2, root2, heap2, envs2);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("CpsCodeTest - MissingValuesPadWithNilAndSurplusIsDiscarded") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto pad = run("(multiple-value-bind (a b) (values 7) (if b 1 a))", da, ca,
                   root, heap, envs);
    REQUIRE(pad.has_value());
    REQUIRE(std::get<int>(pad.value()) == 7);

    DatumArena da2;
    CoreArena ca2;
    Heap heap2;
    Core root2;
    Envs envs2;
    auto drop = run("(multiple-value-bind (a) (values 4 5 6) a)", da2, ca2,
                    root2, heap2, envs2);
    REQUIRE(drop.has_value());
    REQUIRE(std::get<int>(drop.value()) == 4);
}

TEST_CASE("CpsCodeTest - ValuesPropagateThroughIfAndPrognTailPositions") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto i = run("(multiple-value-bind (a b) (if t (values 1 2) nil) (+ a b))",
                 da, ca, root, heap, envs);
    REQUIRE(i.has_value());
    REQUIRE(std::get<int>(i.value()) == 3);

    DatumArena da2;
    CoreArena ca2;
    Heap heap2;
    Core root2;
    Envs envs2;
    auto p = run("(multiple-value-bind (a b) (progn 0 (values 5 6)) (+ a b))",
                 da2, ca2, root2, heap2, envs2);
    REQUIRE(p.has_value());
    REQUIRE(std::get<int>(p.value()) == 11);
}

TEST_CASE("CpsCodeTest - ValuesPropagateThroughFunctionReturn") {
    // A call is a continuation barrier under CPS (see
    // cps_apply_function_value), so this is the test that says the barrier
    // does not swallow the secondary values on the way back out.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("(progn (defun two () (values 3 4))"
                     "  (multiple-value-bind (a b) (two) (+ a b)))",
                     da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("CpsCodeTest - ValuesPropagateThroughUnwindProtect") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("(let ((log 0))"
                     "  (+ (multiple-value-bind (a b)"
                     "         (unwind-protect (values 1 2) (setq log 10))"
                     "       (+ a b))"
                     "     log))",
                     da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 13);
}

TEST_CASE("CpsCodeTest - ValuesIsAnOrdinaryFunctionUnderFuncallAndApply") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto f = run("(multiple-value-bind (a b) (funcall #'values 1 2) (+ a b))",
                 da, ca, root, heap, envs);
    REQUIRE(f.has_value());
    REQUIRE(std::get<int>(f.value()) == 3);

    DatumArena da2;
    CoreArena ca2;
    Heap heap2;
    Core root2;
    Envs envs2;
    auto a =
        run("(multiple-value-bind (a b) (apply #'values (list 1 2)) (+ a b))",
            da2, ca2, root2, heap2, envs2);
    REQUIRE(a.has_value());
    REQUIRE(std::get<int>(a.value()) == 3);
}

TEST_CASE("CpsCodeTest - MultipleValueBindBindsSpecialsDynamically") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Store store;
    Core root;
    Envs envs;
    auto r = run_mut("(progn"
                     "  (defvar *x* 1)"
                     "  (defun get-x () *x*)"
                     "  (+ (multiple-value-bind (*x*) (values 2) (get-x))"
                     "     (get-x)))",
                     da, ca, root, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
    REQUIRE(envs.dynamic_depth() == 0);
}
