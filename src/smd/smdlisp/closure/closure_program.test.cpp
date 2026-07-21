// src/smd/smdlisp/closure/closure_program.test.cpp                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Merge criterion (docs/cl-pivot-plan.md, step L13): every L11/L12
// end-to-end eval_direct.test.cpp scenario also passes through
// compile_to_closure. This file re-expresses that whole scenario set
// (rather than adding one narrow test), driving every program through
// compile_to_closure instead of eval_direct, and checking the SAME
// observable behavior -- same values, same error conditions -- the
// direct evaluator's own test file (eval_direct.test.cpp) already pins.
//
// **Lifetime discipline, read before adding more tests here (see
// handoff-next.md Part 1's warning, and the L10/L11 handoff entries it
// points at):** `pr`, the `result<closure_program<...>>` returned by
// @ref lisp::closure::compile_to_closure, must be declared directly in
// each TEST_CASE/static_assert body and stay alive for as long as an
// evaluated `value<Core>`'s `symbol`/`keyword` (a bare `std::string_view`,
// not an owned spelling) might be inspected -- the compiled program's
// `CpsCode` captures the elaborated core root (and the core arena) BY
// VALUE, and a `core_symbol`/`core_keyword`'s owned `folded_name` storage
// lives inside that captured copy. An earlier draft of this file hid
// `compile_to_closure`'s call behind a `run()` helper that returned a
// plain `Res` (dropping `pr` when the helper returned) -- caught
// immediately by AddressSanitizer failing
// `ClosureProgramTest - KeywordSelfEvaluates` with a stack-use-after-return,
// the identical failure mode L10/L11 hit one layer down. The fix here:
// `compile_to_closure` is always called directly in the test/static_assert
// body; only the "evaluate an already-compiled, already-successful
// program" step (which needs no new arena/root of its own) is factored
// into the @ref eval / @ref eval_mut helpers below.

#include <smd/smdlisp/closure/closure_program.hpp>
#include <smd/smdlisp/closure/closure_program.hpp> // test 2nd include OK

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
using Heap = lisp::closure::pair_heap<Core, lisp::closure::default_max_pairs>;
using Envs = lisp::closure::env_arena<Core, MaxBindings, MaxEnvs>;
using Store = lisp::closure::store<Core, lisp::closure::default_max_store>;

/// Evaluates an already-compiled, already-successful @p program under a
/// default (functional-mode, no store) environment sharing @p heap and
/// @p envs.
template <class Program>
constexpr auto eval(Program &program, Heap &heap, Envs &envs) -> Res {
    auto environment = lisp::closure::default_env<Core, MaxBindings>(heap);
    return program(environment, envs);
}

/// Like @ref eval, but the environment is store-backed (step L12):
/// required for `setq` and for `defun` self-recursion.
template <class Program>
constexpr auto eval_mut(Program &program, Store &store, Heap &heap, Envs &envs)
    -> Res {
    auto environment =
        lisp::closure::default_env<Core, MaxBindings>(store, heap);
    return program(environment, envs);
}

} // namespace

TEST_CASE("ClosureProgramTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- merge criteria (docs/cl-pivot-plan.md, step L11), through
// -- compile_to_closure instead of eval_direct ----------------------------

static_assert([] {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(if nil 1 2)"sv, da);
    if (!pr.has_value())
        return false;
    auto vr = eval(pr.value(), heap, envs);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 2;
}());

static_assert([] {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "((lambda (x) (car (cdr x))) '(1 2 3))"sv, da);
    if (!pr.has_value())
        return false;
    auto vr = eval(pr.value(), heap, envs);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 2;
}());

static_assert([] {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(funcall #'cons 1 nil)"sv, da);
    if (!pr.has_value())
        return false;
    auto vr = eval(pr.value(), heap, envs);
    if (!vr.has_value() ||
        !std::holds_alternative<lisp::closure::pair_ref>(vr.value()))
        return false;
    auto const &cell =
        heap.get(std::get<lisp::closure::pair_ref>(vr.value()).loc);
    return std::holds_alternative<int>(cell.car) &&
           std::get<int>(cell.car) == 1 &&
           std::holds_alternative<lisp::closure::nil_t>(cell.cdr);
}());

// -- append (step L18 builtin) routes through the CPS backend -------------
// The L18 `append` builtin landed after L13's CPS backend; this pins that
// `append` is handled in cps_apply's list-op dispatch (via to_list_op), not
// just in eval_direct.  `(append '(1) '(2))` => (1 2).
static_assert([] {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(append '(1) '(2))"sv, da);
    if (!pr.has_value())
        return false;
    auto vr = eval(pr.value(), heap, envs);
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

// -- merge criterion (docs/cl-pivot-plan.md, step L12), through
// -- compile_to_closure ----------------------------------------------------

static_assert([] {
    // (progn (defun twice (x) (+ x x)) (twice 4)) => 8, through CPS: the
    // SAME reserve-then-patch defun mechanism (env::patch_function) and
    // the SAME mutable-environment progn-sibling-visibility mechanism
    // eval_direct uses, threaded through cps_dispatch/cps_apply instead.
    DatumArena da;
    Store store;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(progn (defun twice (x) (+ x x)) (twice 4))"sv, da);
    if (!pr.has_value())
        return false;
    auto vr = eval_mut(pr.value(), store, heap, envs);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 8;
}());

// -- keyword self-evaluation ----------------------------------------------

TEST_CASE("ClosureProgramTest - KeywordSelfEvaluates") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr =
        lisp::closure::compile_to_closure<MaxNodes, MaxList>(":foo"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::keyword>(r.value()));
    REQUIRE(std::get<lisp::closure::keyword>(r.value()).name == "FOO");
}

// -- nil / t truthiness -----------------------------------------------------

TEST_CASE("ClosureProgramTest - NilIsFalse") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(if nil 1 2)"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("ClosureProgramTest - ZeroIsTrue") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(if 0 1 2)"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 1);
}

TEST_CASE("ClosureProgramTest - TEvaluatesToCanonicalTrueSymbol") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>("t"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::symbol>(r.value()));
    REQUIRE(std::get<lisp::closure::symbol>(r.value()).name == "T");
}

// -- let / let* --------------------------------------------------------

TEST_CASE("ClosureProgramTest - LetBindsInParallel") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(let ((x 1) (y 2)) (+ x y))"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("ClosureProgramTest - LetStarSeesEarlierBindings") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(let* ((x 1) (y (+ x 1))) y)"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

// -- implicit progn --------------------------------------------------------

TEST_CASE("ClosureProgramTest - LambdaBodyIsImplicitProgn") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "((lambda (x) x 1 2) 99)"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

// -- closures capture both namespaces ---------------------------------

TEST_CASE("ClosureProgramTest - ClosureCapturesVariableNamespace") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(let ((x 10)) ((lambda (y) (+ x y)) 5))"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 15);
}

TEST_CASE("ClosureProgramTest - ClosureCapturesFunctionNamespace") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(let ((x 1)) ((lambda () (cons x nil))))"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::pair_ref>(r.value()));
    auto const &cell =
        heap.get(std::get<lisp::closure::pair_ref>(r.value()).loc);
    REQUIRE(std::get<int>(cell.car) == 1);
}

// -- apply spreads the final list argument ---------------------------------

TEST_CASE("ClosureProgramTest - ApplySpreadsFinalListArgument") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(apply #'+ 1 (list 2 3))"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 6);
}

TEST_CASE("ClosureProgramTest - ApplyWithJustTheListArgument") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(apply #'+ (list 1 2))"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

// -- error paths: distinct unbound-variable vs undefined-function --------

TEST_CASE("ClosureProgramTest - UnboundVariableIsError") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr =
        lisp::closure::compile_to_closure<MaxNodes, MaxList>("nope"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(std::string_view{r.error().message} == "unbound variable");
}

TEST_CASE("ClosureProgramTest - UndefinedFunctionIsError") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(nope 1 2)"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(std::string_view{r.error().message} == "undefined function");
}

TEST_CASE("ClosureProgramTest - CallingNonFunctionIsError") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(funcall 5)"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("ClosureProgramTest - ArityMismatchIsError") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "((lambda (x y) (+ x y)) 1)"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE_FALSE(r.has_value());
}

// -- funcall / #' at the evaluator level -----------------------------------

TEST_CASE("ClosureProgramTest - FuncallOfNamedFunction") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(funcall #'+ 3 4)"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("ClosureProgramTest - FuncallOfLambdaExpression") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(funcall (lambda (x) (* x x)) 5)"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 25);
}

// -- setq (step L12) ------------------------------------------------------

TEST_CASE(
    "ClosureProgramTest - SetqMutatesLexicalBindingAndReturnsAssignedValue") {
    DatumArena da;
    Store store;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(let ((x 1)) (setq x 2) x)"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval_mut(pr.value(), store, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("ClosureProgramTest - SetqReturnsTheAssignedValue") {
    DatumArena da;
    Store store;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(let ((x 1)) (setq x 42))"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval_mut(pr.value(), store, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("ClosureProgramTest - SetqOnUnboundVariableIsError") {
    DatumArena da;
    Store store;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(setq nope 1)"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval_mut(pr.value(), store, heap, envs);
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("ClosureProgramTest - SetqWithNoStoreEnvironmentIsError") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(let ((x 1)) (setq x 2))"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE_FALSE(r.has_value());
}

// -- defun (step L12) ------------------------------------------------------

TEST_CASE("ClosureProgramTest - DefunDefinesAndTwiceIsCallable") {
    DatumArena da;
    Store store;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(progn (defun twice (x) (+ x x)) (twice 4))"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval_mut(pr.value(), store, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 8);
}

TEST_CASE("ClosureProgramTest - DefunReturnsTheFunctionNameSymbol") {
    DatumArena da;
    Store store;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(defun twice (x) (+ x x))"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval_mut(pr.value(), store, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::symbol>(r.value()));
    REQUIRE(std::get<lisp::closure::symbol>(r.value()).name == "TWICE");
}

TEST_CASE("ClosureProgramTest - DefunSelfRecursionWorksWithStoreBackedEnv") {
    // The decisive CPS design-question test: a store-backed environment
    // (env::patch_function) lets a defun-defined function find its own
    // name in the FUNCTION namespace of its own captured environment,
    // through cps_dispatch/cps_apply exactly as it does through
    // eval_direct.
    DatumArena da;
    Store store;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(progn"
        "  (defun fact (n) (if (eq n 0) 1 (* n (fact (+ n -1)))))"
        "  (fact 5))"sv,
        da);
    REQUIRE(pr.has_value());
    auto r = eval_mut(pr.value(), store, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 120);
}

TEST_CASE("ClosureProgramTest - DefunDoesNotSupportSelfRecursionWithoutStore") {
    // Documented limitation (env::patch_function's docs, shared by every
    // backend): a functional-mode (no-store) environment falls back to
    // shadow-redefining the OUTER environment object, but the closure had
    // already captured a COPY at the point of the (unpatchable)
    // placeholder push, so its own body still sees FACT bound to the nil
    // placeholder -- a lookup SUCCESS (unlike a name never defined at
    // all), so calling it fails with "attempted to call non-function",
    // not "undefined function".
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(progn"
        "  (defun fact (n) (if (eq n 0) 1 (* n (fact (+ n -1)))))"
        "  (fact 5))"sv,
        da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(std::string_view{r.error().message} ==
            "attempted to call non-function");
}

TEST_CASE("ClosureProgramTest - DefunNonRecursiveWorksWithoutStore") {
    DatumArena da;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(progn (defun twice (x) (+ x x)) (twice 4))"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval(pr.value(), heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 8);
}

// -- defvar / defparameter (step L12) --------------------------------------

TEST_CASE("ClosureProgramTest - DefvarBindsWhenUnbound") {
    DatumArena da;
    Store store;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(progn (defvar *x* 10) *x*)"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval_mut(pr.value(), store, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 10);
}

TEST_CASE("ClosureProgramTest - DefvarDoesNotRebindIfAlreadyBound") {
    DatumArena da;
    Store store;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(progn (defvar *x* 10) (defvar *x* 999) *x*)"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval_mut(pr.value(), store, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 10);
}

TEST_CASE("ClosureProgramTest - DefparameterAlwaysRebinds") {
    DatumArena da;
    Store store;
    Heap heap;
    Envs envs;
    auto pr = lisp::closure::compile_to_closure<MaxNodes, MaxList>(
        "(progn (defparameter *x* 10) (defparameter *x* 999) *x*)"sv, da);
    REQUIRE(pr.has_value());
    auto r = eval_mut(pr.value(), store, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 999);
}
