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
using Store = lisp::closure::store<Core, lisp::closure::default_max_store>;

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

/// Like @ref run, but the environment is store-backed (step L12): required
/// for `setq` and for `defun` self-recursion (@ref
/// lisp::closure::env::set_value / @ref lisp::closure::env::patch_function
/// both need a mutable-mode environment -- see env.hpp's docs for why a
/// no-store environment cannot support either). A separate helper, rather
/// than adding a @p store parameter to @ref run, keeps every pre-L12 call
/// site of @ref run unchanged.
auto run_mut(std::string_view src, DatumArena &datum_arena,
             CoreArena &core_arena, Core &root, Store &store, Heap &heap,
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
        lisp::closure::default_env<Core, MaxBindings>(store, heap);
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

// -- merge criterion (docs/cl-pivot-plan.md, step L12) --------------------

static_assert([] {
    // (progn (defun twice (x) (+ x x)) (twice 4)) => 8.
    //
    // Top-level sequencing decision (see eval_direct.hpp's doc comment on
    // the `environment` parameter): the wrapping `progn` evaluates both
    // forms through the SAME mutable environment reference, so `defun`'s
    // in-place env::define_function/patch_function calls are visible to
    // the sibling `(twice 4)` call -- no separate top-level-sequence
    // driver was needed for this step.
    DatumArena da;
    CoreArena ca;
    Store store;
    Heap heap;
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
        lisp::closure::default_env<Core, MaxBindings>(store, heap);
    auto vr =
        lisp::closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca, environment, envs);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 8;
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

// -- setq (step L12) ------------------------------------------------------

TEST_CASE("EvalDirectTest - SetqMutatesLexicalBindingAndReturnsAssignedValue") {
    DatumArena da;
    CoreArena ca;
    Store store;
    Heap heap;
    Core root;
    Envs envs;
    // let binds x=1 (store-backed, since run_mut's environment has a
    // store); setq mutates it in place and its own result is the newly
    // assigned value; the body's final expression reads x back to prove
    // the mutation stuck, not just that setq's own return value was
    // right.
    auto r =
        run_mut("(let ((x 1)) (setq x 2) x)", da, ca, root, store, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("EvalDirectTest - SetqReturnsTheAssignedValue") {
    DatumArena da;
    CoreArena ca;
    Store store;
    Heap heap;
    Core root;
    Envs envs;
    auto r =
        run_mut("(let ((x 1)) (setq x 42))", da, ca, root, store, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("EvalDirectTest - SetqOnUnboundVariableIsError") {
    DatumArena da;
    CoreArena ca;
    Store store;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run_mut("(setq nope 1)", da, ca, root, store, heap, envs);
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("EvalDirectTest - SetqWithNoStoreEnvironmentIsError") {
    // Using the ordinary (functional-mode, no-store) run() helper: setq
    // errors rather than silently no-op'ing.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(let ((x 1)) (setq x 2))", da, ca, root, heap, envs);
    REQUIRE_FALSE(r.has_value());
}

// -- defun (step L12) ------------------------------------------------------

TEST_CASE("EvalDirectTest - DefunDefinesAndTwiceIsCallable") {
    DatumArena da;
    CoreArena ca;
    Store store;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run_mut("(progn (defun twice (x) (+ x x)) (twice 4))", da, ca,
                     root, store, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 8);
}

TEST_CASE("EvalDirectTest - DefunReturnsTheFunctionNameSymbol") {
    DatumArena da;
    CoreArena ca;
    Store store;
    Heap heap;
    Core root;
    Envs envs;
    auto r =
        run_mut("(defun twice (x) (+ x x))", da, ca, root, store, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::symbol>(r.value()));
    REQUIRE(std::get<lisp::closure::symbol>(r.value()).name == "TWICE");
}

TEST_CASE("EvalDirectTest - DefunSelfRecursionWorksWithStoreBackedEnv") {
    // A store-backed environment (env::patch_function) lets a defun-
    // defined function find its own name in the FUNCTION namespace of its
    // own captured environment -- see env.hpp's docs for the reserve
    // -then-patch mechanism this relies on.
    DatumArena da;
    CoreArena ca;
    Store store;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run_mut("(progn"
                     "  (defun fact (n) (if (eq n 0) 1 (* n (fact (+ n -1)))))"
                     "  (fact 5))",
                     da, ca, root, store, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 120);
}

TEST_CASE("EvalDirectTest - DefunDoesNotSupportSelfRecursionWithoutStore") {
    // Documented limitation (env::patch_function's docs): a functional
    // -mode (no-store) environment falls back to shadow-redefining the
    // OUTER environment object, but the closure had already captured a
    // COPY at the point of the (unpatchable) placeholder push, so its own
    // body still sees FACT bound to the nil placeholder -- a lookup
    // SUCCESS (unlike a name never defined at all), so calling it fails
    // with "attempted to call non-function", not "undefined function".
    // External calls to the function still work (see the companion
    // "DefunReturnsTheFunctionNameSymbol"-style non-recursive case,
    // "DefunDefinesAndTwiceIsCallable", which does not need a store).
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(progn"
                 "  (defun fact (n) (if (eq n 0) 1 (* n (fact (+ n -1)))))"
                 "  (fact 5))",
                 da, ca, root, heap, envs);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(std::string_view{r.error().message} ==
            "attempted to call non-function");
}

TEST_CASE("EvalDirectTest - DefunNonRecursiveWorksWithoutStore") {
    // The shadow-redefine fallback in env::patch_function keeps ordinary
    // (non-self-recursive) defun correct even without a store: this uses
    // the plain, no-store run() helper deliberately, unlike this file's
    // other defun tests.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run("(progn (defun twice (x) (+ x x)) (twice 4))", da, ca, root,
                 heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 8);
}

// -- defvar / defparameter (step L12; special-mark only, no dynamic
// -- binding behavior yet -- see env::is_special's docs) ------------------

TEST_CASE("EvalDirectTest - DefvarBindsWhenUnbound") {
    DatumArena da;
    CoreArena ca;
    Store store;
    Heap heap;
    Core root;
    Envs envs;
    auto r =
        run_mut("(progn (defvar *x* 10) *x*)", da, ca, root, store, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 10);
}

TEST_CASE("EvalDirectTest - DefvarDoesNotRebindIfAlreadyBound") {
    // ANSI CL: a second defvar for an already-bound name does not
    // re-evaluate or rebind the value form.
    DatumArena da;
    CoreArena ca;
    Store store;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run_mut("(progn (defvar *x* 10) (defvar *x* 999) *x*)", da, ca,
                     root, store, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 10);
}

TEST_CASE("EvalDirectTest - DefparameterAlwaysRebinds") {
    DatumArena da;
    CoreArena ca;
    Store store;
    Heap heap;
    Core root;
    Envs envs;
    auto r = run_mut("(progn (defparameter *x* 10) (defparameter *x* 999) *x*)",
                     da, ca, root, store, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 999);
}
