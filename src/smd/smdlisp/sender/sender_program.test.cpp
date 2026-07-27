// src/smd/smdlisp/sender/sender_program.test.cpp                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/sender/sender_program.hpp>
#include <smd/smdlisp/sender/sender_program.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
namespace lisp = smd::smdlisp;
namespace scm = smd::smdscheme;
namespace snd = smd::smdlisp::sender;
using namespace std::string_view_literals;

constexpr int MaxNodes = 128;
constexpr int MaxList = 16;
constexpr int MaxBindings = 16;
constexpr int MaxEnvs = 32;

using Core = lisp::elaborator::core_type<MaxNodes, MaxList>;
using Val = lisp::closure::value<Core>;
using Res = scm::foundation::result<Val>;
using Out = snd::outcome<Core>;
using DatumArena =
    scm::foundation::tree_arena<lisp::reader::datum_type<MaxNodes, MaxList>,
                                MaxNodes>;
using Heap = lisp::closure::pair_heap<Core, lisp::closure::default_max_pairs>;
using Store = lisp::closure::store<Core, lisp::closure::default_max_store>;
using Envs = lisp::closure::env_arena<Core, MaxBindings, MaxEnvs>;
using Program = snd::sender_program<MaxNodes, MaxList, MaxBindings, MaxEnvs>;

/// Compiles @p src into CALLER-owned @p prog and runs it under a default
/// (store-less) environment sharing @p heap and @p envs.
///
/// Mirrors `cps_code.test.cpp`'s `run` helper, which mirrors
/// `eval_direct.test.cpp`'s: the same source, the same caller-owns-the-storage
/// discipline, a third evaluator.  That is the whole parity claim, and having
/// the helpers be transcriptions of each other is what makes it checkable.
///
/// Two things must outlive the returned value, for the same reason and by the
/// same rule the other two backends' helpers already obey:
///
///  - @p datum_arena, because core nodes hold `string_view`s into the datum
///    atom data (DIV-0007);
///  - @p prog, because a @ref Val can view into the core node that produced
///    it -- a bare keyword or symbol atom that is the program's entire root
///    owns its name inside `prog.root`, and a closure value holds a raw
///    pointer to its lambda node inside `prog.arena`.
///
/// `eval_direct.test.cpp` learned the second one the hard way (its `Core
/// &root` parameter, and the L10 stack-use-after-return note above it).  The
/// sender backend moved the root *into* the program object, so here it is the
/// whole program that has to be caller-owned rather than just the root --
/// same lesson, one level out.  AddressSanitizer caught the function-local
/// version of this helper immediately, on `KeywordSelfEvaluates`, exactly as
/// it caught the original.
auto run(std::string_view src, DatumArena &datum_arena, Program &prog,
         Heap &heap, Envs &envs) -> Res {
    auto pr = snd::compile_to_sender<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
        src, datum_arena);
    if (!pr.has_value())
        return Res{pr.error()};
    prog = pr.value();
    auto environment = lisp::closure::default_env<Core, MaxBindings>(heap);
    return prog(environment, envs);
}

/// Like @ref run, but builds a *mutable* environment (a shared @ref Store,
/// step L12) so `setq`/`defun`/`defvar`/`defparameter` can be exercised.
auto run_mut(std::string_view src, DatumArena &datum_arena, Program &prog,
             Heap &heap, Store &store, Envs &envs) -> Res {
    auto pr = snd::compile_to_sender<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
        src, datum_arena);
    if (!pr.has_value())
        return Res{pr.error()};
    prog = pr.value();
    auto environment =
        lisp::closure::default_env<Core, MaxBindings>(heap, store);
    return prog(environment, envs);
}

/// Like @ref run_mut, but returns the raw three-channel completion rather
/// than projecting it onto a `result` -- for the tests that are about
/// *which channel* fired.
auto run_channel(std::string_view src, DatumArena &datum_arena, Program &prog,
                 Heap &heap, Store &store, Envs &envs) -> Out {
    auto pr = snd::compile_to_sender<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
        src, datum_arena);
    if (!pr.has_value())
        return snd::make_error<Core>(pr.error());
    prog = pr.value();
    auto environment =
        lisp::closure::default_env<Core, MaxBindings>(heap, store);
    return prog.run(environment, envs);
}

// Sources shared with `eval_direct.test.cpp` / `cps_code.test.cpp` verbatim,
// so the three backends are demonstrably running the same programs.
constexpr auto cleanup_order_src =
    "(let ((log 0))"
    "  (catch 'c"
    "    (unwind-protect"
    "      (unwind-protect (throw 'c 99) (setq log (+ (* log 10) 1)))"
    "      (setq log (+ (* log 10) 2))))"
    "  log)"sv;

constexpr auto throw_through_unwind_src =
    "(let ((log 0))"
    "  (+ (catch 'c (unwind-protect (throw 'c 99) (setq log 1))) log))"sv;

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

} // namespace

TEST_CASE("SenderProgramTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- merge criteria (docs/cl-pivot-plan.md, step L21) ------------------------
// Sender-backend parity for the L11-L15 test set. Every TEST_CASE below is a
// transcription of an identically-named one in `cps_code.test.cpp` or
// `eval_direct.test.cpp`, run through `compile_to_sender` instead.
//
// These are runtime cases only, with no `static_assert` twins. That is not an
// oversight and not a weakening of the standing "every static_assert needs a
// runtime twin" rule -- it is the reverse situation: Beman's connect/start
// machinery is not constant-evaluable, so the sender backend has runtime
// evidence ONLY. See DIV-0015.

// -- L11: the direct-evaluator surface --------------------------------------

TEST_CASE("SenderProgramTest - KeywordSelfEvaluates") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run(":foo", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::keyword>(r.value()));
    REQUIRE(std::get<lisp::closure::keyword>(r.value()).name == "FOO");
}

TEST_CASE("SenderProgramTest - NilIsFalse") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(if nil 1 2)", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("SenderProgramTest - ZeroIsTrue") {
    // D3: nil is the SOLE false value.
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(if 0 1 2)", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 1);
}

TEST_CASE("SenderProgramTest - TEvaluatesToCanonicalTrueSymbol") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("t", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<lisp::closure::symbol>(r.value()).name == "T");
}

TEST_CASE("SenderProgramTest - Arithmetic") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(+ 1 (* 2 3))", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("SenderProgramTest - LetBindsInParallel") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(let ((x 1) (y 2)) (+ x y))", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("SenderProgramTest - LetStarSeesEarlierBindings") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(let* ((x 1) (y (+ x 1))) y)", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("SenderProgramTest - LambdaBodyIsImplicitProgn") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("((lambda (x) x 1 2) 99)", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("SenderProgramTest - ClosureCapturesVariableNamespace") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r =
        run("(let ((x 10)) ((lambda (y) (+ x y)) 5))", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 15);
}

TEST_CASE("SenderProgramTest - ClosureCapturesFunctionNamespace") {
    // D4: CONS resolves through the captured environment's FUNCTION
    // namespace, not the variable namespace `x` lives in.
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r =
        run("(let ((x 1)) ((lambda () (cons x nil))))", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::pair_ref>(r.value()));
    auto const &cell =
        heap.get(std::get<lisp::closure::pair_ref>(r.value()).loc);
    REQUIRE(std::get<int>(cell.car) == 1);
}

TEST_CASE("SenderProgramTest - LambdaApplicationWithCarCdr") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("((lambda (x) (car x)) (list 1 2))", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 1);
}

TEST_CASE("SenderProgramTest - ApplySpreadsFinalListArgument") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(apply #'+ 1 (list 2 3))", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 6);
}

TEST_CASE("SenderProgramTest - ApplyWithJustTheListArgument") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(apply #'+ (list 1 2))", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("SenderProgramTest - FuncallOfNamedFunction") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(funcall #'+ 3 4)", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("SenderProgramTest - FuncallOfLambdaExpression") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(funcall (lambda (x) (* x x)) 5)", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 25);
}

TEST_CASE("SenderProgramTest - UnboundVariableIsError") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(let ((x 1)) y)", da, prog, heap, envs);
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("SenderProgramTest - UndefinedFunctionIsError") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(nope 1 2)", da, prog, heap, envs);
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("SenderProgramTest - CallingNonFunctionIsError") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(funcall 5)", da, prog, heap, envs);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(std::string_view{r.error().message} ==
            "attempted to call non-function");
}

TEST_CASE("SenderProgramTest - ArityMismatchIsError") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("((lambda (x y) (+ x y)) 1)", da, prog, heap, envs);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(std::string_view{r.error().message} == "arity mismatch");
}

// -- L12: setq / defun / defvar / defparameter ------------------------------

TEST_CASE("SenderProgramTest - DefunThenCallInSameProgn") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(progn (defun twice (x) (+ x x)) (twice 4))", da, prog,
                     heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 8);
}

TEST_CASE("SenderProgramTest - DefunReturnsTheFunctionName") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(defun f (x) x)", da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<lisp::closure::symbol>(r.value()).name == "F");
}

TEST_CASE("SenderProgramTest - DefunDefinedFunctionIsCallableViaFuncall") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(progn (defun sq (x) (* x x)) (funcall #'sq 5))", da,
                     prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 25);

    DatumArena da2;

    Program prog2;
    Heap heap2;
    Store store2;
    Envs envs2;
    auto r2 = run_mut("(progn (defun sq (x) (* x x)) (apply #'sq (list 6)))",
                      da2, prog2, heap2, store2, envs2);
    REQUIRE(r2.has_value());
    REQUIRE(std::get<int>(r2.value()) == 36);
}

TEST_CASE("SenderProgramTest - SetqExpressionValueIsTheAssignedValue") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r =
        run_mut("((lambda (x) (setq x 42)) 1)", da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("SenderProgramTest - SetqMutatesNearestLexicalBinding") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r =
        run_mut("((lambda (x) (setq x 2) x) 1)", da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE(
    "SenderProgramTest - SetqMultiplePairsAssignsLeftToRightReturnsLast") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("((lambda (x y) (setq x 10 y 20) (+ x y)) 1 2)", da, prog,
                     heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 30);
}

TEST_CASE("SenderProgramTest - SetqOfUnboundIsDiagnosedError") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(setq nope 1)", da, prog, heap, store, envs);
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("SenderProgramTest - SetqWithoutStoreIsDiagnosedError") {
    // The store-less `default_env` overload: `setq` has nowhere to write.
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("((lambda (x) (setq x 2) x) 1)", da, prog, heap, envs);
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("SenderProgramTest - SetqIsVisibleAcrossClosuresSharingTheBinding") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(let ((x 1)) (progn (defun inc () (setq x (+ x 1)))"
                     " (funcall #'inc) (funcall #'inc) x))",
                     da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("SenderProgramTest - DefvarInitializesAndMarksSpecial") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(progn (defvar *x* 1) *x*)", da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 1);
}

TEST_CASE("SenderProgramTest - DefvarDoesNotReinitializeIfAlreadyBound") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(progn (defvar *x* 1) (setq *x* 2) (defvar *x* 99) *x*)",
                     da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("SenderProgramTest - DefparameterAlwaysReinitializes") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r =
        run_mut("(progn (defvar *x* 1) (setq *x* 2) (defparameter *x* 99) *x*)",
                da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 99);
}

TEST_CASE("SenderProgramTest - DefvarReturnsTheVariableName") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(defvar *x* 1)", da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<lisp::closure::symbol>(r.value()).name == "*X*");
}

// -- L14: block / return-from -----------------------------------------------

TEST_CASE("SenderProgramTest - BlockReturnFromSkipsRestOfBody") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(block b 1 (return-from b 42) 3)", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("SenderProgramTest - BlockFallsOffEndReturnsLastValue") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(block b 1 2 3)", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("SenderProgramTest - BlockWithNoFormsEvaluatesToNil") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(block b)", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::nil_t>(r.value()));
}

TEST_CASE("SenderProgramTest - DefunImplicitBlockReturnFromExitsFunctionCall") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(progn (defun f (x) (if x (return-from f 0) 1)) (f t))",
                     da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 0);

    DatumArena da2;

    Program prog2;
    Heap heap2;
    Store store2;
    Envs envs2;
    auto r2 =
        run_mut("(progn (defun f (x) (if x (return-from f 0) 1)) (f nil))", da2,
                prog2, heap2, store2, envs2);
    REQUIRE(r2.has_value());
    REQUIRE(std::get<int>(r2.value()) == 1);
}

TEST_CASE("SenderProgramTest - NestedBlockReturnFromSkipsInnerBlock") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(block outer (block inner (return-from outer 5)) 99)", da,
                 prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 5);
}

TEST_CASE("SenderProgramTest - SameNameNestedBlockReturnFromTargetsInnermost") {
    // Two simultaneously-live activations of block name `b`; the inner
    // return-from targets its own. This is the case `live` exists for, and
    // the one where the sender backend's dropped sentinel has to still be
    // enough: the inner block's record is dead, the outer's is not.
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(block b (+ 100 (block b (return-from b 5))))", da, prog,
                 heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 105);
}

TEST_CASE("SenderProgramTest - ReturnFromOfDeadBlockIsDiagnosedError") {
    // D5: using a dead exit is diagnosed, never UB. Note the channel: this
    // is an ERROR, not a stopped -- it is not an unwind, it is the refusal
    // to start one.
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto o = run_channel(
        "(progn (defun make-escaper () (block b (lambda () (return-from b "
        "42)))) (funcall (make-escaper)))",
        da, prog, heap, store, envs);
    REQUIRE(o.is_error());
    REQUIRE(std::string_view{o.err.message} ==
            "return-from: block has already exited");
}

TEST_CASE("SenderProgramTest - ReturnFromWithoutValueReturnsNil") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(block b (return-from b))", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::nil_t>(r.value()));
}

// -- L15: catch / throw / unwind-protect ------------------------------------

TEST_CASE("SenderProgramTest - CatchReturnsThrownValue") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(catch 'c 1 (throw 'c 42) 3)", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 42);
    REQUIRE(envs.catch_depth() == 0);
}

TEST_CASE("SenderProgramTest - CatchFallsOffEndReturnsLastValue") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(catch 'c 1 2 3)", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("SenderProgramTest - CatchWithNoFormsEvaluatesToNil") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(catch 'c)", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::nil_t>(r.value()));
}

TEST_CASE("SenderProgramTest - ThrowSkipsCatchWithNonMatchingTag") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(catch 'a (catch 'b (throw 'a 7)) 99)", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 7);
    REQUIRE(envs.catch_depth() == 0);
}

TEST_CASE("SenderProgramTest - SameTagNestedCatchTargetsInnermost") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r =
        run("(catch 'c (+ 100 (catch 'c (throw 'c 5))))", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 105);
}

TEST_CASE("SenderProgramTest - CatchTagIsAnEvaluatedValue") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(let ((tag 'c)) (catch tag (throw tag 8)))", da, prog,
                     heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 8);
}

TEST_CASE("SenderProgramTest - UncaughtThrowIsDiagnosedError") {
    // D5, spelled out for this backend: an uncaught throw surfaces on the
    // ERROR channel. It is not an unwind with nowhere to go; it is the
    // diagnosis that there is nowhere to go.
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto o =
        run_channel("(catch 'a (throw 'b 1))", da, prog, heap, store, envs);
    REQUIRE(o.is_error());
    REQUIRE(std::string_view{o.err.message} == "throw: no catch for tag");
    REQUIRE(envs.catch_depth() == 0);

    DatumArena da2;

    Program prog2;
    Heap heap2;
    Store store2;
    Envs envs2;
    auto o2 = run_channel("(throw 'c 1)", da2, prog2, heap2, store2, envs2);
    REQUIRE(o2.is_error());
}

TEST_CASE("SenderProgramTest - UnwindProtectReturnsProtectedFormValue") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(unwind-protect 7 8 9)", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 7);
}

TEST_CASE("SenderProgramTest - UnwindProtectCleanupRunsOnNormalCompletion") {
    // The set_value arm of the adapter, reached through a real program.
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(let ((log 0)) (unwind-protect 7 (setq log 1)) log)", da,
                     prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 1);
}

TEST_CASE("SenderProgramTest - UnwindProtectCleanupRunsOnReturnFromPath") {
    // The set_stopped arm, via `return-from`.
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(let ((log 0)) (block b (unwind-protect (return-from b 7)"
                     " (setq log 1))) log)",
                     da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 1);

    DatumArena da2;

    Program prog2;
    Heap heap2;
    Envs envs2;
    auto r2 = run("(block b (unwind-protect (return-from b 7) 0))", da2, prog2,
                  heap2, envs2);
    REQUIRE(r2.has_value());
    REQUIRE(std::get<int>(r2.value()) == 7);
}

TEST_CASE("SenderProgramTest - UnwindProtectCleanupRunsOnErrorPath") {
    // The set_error arm, at program level.  Note the limit of what these two
    // cases can witness: `smdlisp` has no way to catch a diagnosed error, so
    // the cleanup's own side effect is unobservable from inside a program
    // that errors -- the program has no value to inspect it with.  What is
    // witnessed here is that the error still propagates with an
    // `unwind-protect` wrapped around it.  That the cleanup actually RAN on
    // that path is witnessed directly, by counting, in
    // `unwind_protect.test.cpp`'s `CleanupRunsOnSetError`, which is why that
    // file tests the adapter in isolation.
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(let ((log 0)) (unwind-protect (nope) (setq log 1)))", da,
                     prog, heap, store, envs);
    REQUIRE_FALSE(r.has_value());

    DatumArena da2;

    Program prog2;
    Heap heap2;
    Store store2;
    Envs envs2;
    auto r2 = run_mut("(let ((log 0)) (catch 'c (unwind-protect (throw 'nope 1)"
                      " (setq log 1))) log)",
                      da2, prog2, heap2, store2, envs2);
    // The uncaught throw is diagnosed, and the cleanup still ran on the way
    // out -- but the error supersedes, so the program as a whole fails.
    REQUIRE_FALSE(r2.has_value());
}

TEST_CASE("SenderProgramTest - UnwindProtectCleanupsRunInnermostFirst") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut(cleanup_order_src, da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 12);
}

TEST_CASE("SenderProgramTest - ThrowThroughNestedUnwindProtectRunsCleanups") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut(throw_through_unwind_src, da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 100);
}

TEST_CASE("SenderProgramTest - ReturnFromUnwindsThroughCatchFrame") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r =
        run("(block b (catch 'c (return-from b 3)) 99)", da, prog, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
    REQUIRE(envs.catch_depth() == 0);
}

TEST_CASE("SenderProgramTest - EveryCatchExitPathPopsTheDynamicStack") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    REQUIRE(run("(catch 'c 1)", da, prog, heap, envs).has_value());
    REQUIRE(envs.catch_depth() == 0);

    DatumArena da2;

    Program prog2;
    Heap heap2;
    Envs envs2;
    REQUIRE(
        run("(catch 'c (throw 'c 1))", da2, prog2, heap2, envs2).has_value());
    REQUIRE(envs2.catch_depth() == 0);

    DatumArena da3;

    Program prog3;
    Heap heap3;
    Envs envs3;
    REQUIRE_FALSE(
        run("(catch 'a (throw 'b 1))", da3, prog3, heap3, envs3).has_value());
    REQUIRE(envs3.catch_depth() == 0);

    DatumArena da4;

    Program prog4;
    Heap heap4;
    Envs envs4;
    REQUIRE(
        run("(block b (catch 'c (return-from b 3)))", da4, prog4, heap4, envs4)
            .has_value());
    REQUIRE(envs4.catch_depth() == 0);
}

// -- L16: special variables and dynamic binding -----------------------------
// Covered in-step rather than deferred: `bind_lambda_parameters` and
// `unwind_dynamic_bindings` are templates on the environment/arena types with
// no dependency on `eval_direct`, so the sender backend gets shallow binding
// by CALLING the same two functions, not by reimplementing them.

TEST_CASE("SenderProgramTest - SpecialLetBindsDynamically") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut(dynamic_binding_src, da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
    REQUIRE(envs.dynamic_depth() == 0);
}

TEST_CASE("SenderProgramTest - NonSpecialLetStillBindsLexically") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut(lexical_contrast_src, da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 1);
    REQUIRE(envs.dynamic_depth() == 0);
}

TEST_CASE("SenderProgramTest - ThrowPastDynamicBindingRestoresOuterValue") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut(throw_past_dynamic_src, da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
    REQUIRE(envs.dynamic_depth() == 0);
}

TEST_CASE(
    "SenderProgramTest - ReturnFromPastDynamicBindingRestoresOuterValue") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(progn (defvar *x* 1) (defun get-x () *x*)"
                     "  (+ (block b (let ((*x* 2)) (return-from b (get-x))))"
                     "     (get-x)))",
                     da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
    REQUIRE(envs.dynamic_depth() == 0);
}

TEST_CASE("SenderProgramTest - SetqOfSpecialAssignsInnermostDynamicBinding") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(progn (defvar *x* 1) (defun get-x () *x*)"
                     "  (+ (let ((*x* 2)) (setq *x* 3) (get-x)) (get-x)))",
                     da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 4);
    REQUIRE(envs.dynamic_depth() == 0);
}

TEST_CASE("SenderProgramTest - NestedDynamicBindingsRestoreInnermostFirst") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(progn (defvar *x* 1) (defun get-x () *x*)"
                     "  (+ (let ((*x* 2)) (+ (let ((*x* 3)) (get-x)) (get-x)))"
                     "     (get-x)))",
                     da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 6);
    REQUIRE(envs.dynamic_depth() == 0);
}

TEST_CASE("SenderProgramTest - SpecialLambdaParameterBindsDynamically") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(progn (defvar *x* 1) (defun get-x () *x*)"
                     "  (defun call-x (*x*) (get-x))"
                     "  (+ (call-x 7) (get-x)))",
                     da, prog, heap, store, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 8);
    REQUIRE(envs.dynamic_depth() == 0);
}

TEST_CASE(
    "SenderProgramTest - DynamicBindingOfUnboundSpecialIsDiagnosedError") {
    // DIV-0014, under the sender backend too.
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto r = run_mut("(progn (defvar *x*) (let ((*x* 2)) *x*))", da, prog, heap,
                     store, envs);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(std::string_view{r.error().message} ==
            "dynamic binding: special variable is unbound");
    REQUIRE(envs.dynamic_depth() == 0);
}

TEST_CASE("SenderProgramTest - EveryDynamicExitPathRestoresTheBindingStack") {
    // Value, throw, return-from and error: four ways out of a dynamic
    // extent, one restore discipline.
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    REQUIRE(run_mut("(progn (defvar *x* 1) (let ((*x* 2)) *x*))", da, prog,
                    heap, store, envs)
                .has_value());
    REQUIRE(envs.dynamic_depth() == 0);

    DatumArena da2;

    Program prog2;
    Heap heap2;
    Store store2;
    Envs envs2;
    REQUIRE(run_mut("(progn (defvar *x* 1)"
                    "  (catch 'c (let ((*x* 2)) (throw 'c 9))))",
                    da2, prog2, heap2, store2, envs2)
                .has_value());
    REQUIRE(envs2.dynamic_depth() == 0);

    DatumArena da3;

    Program prog3;
    Heap heap3;
    Store store3;
    Envs envs3;
    REQUIRE(run_mut("(progn (defvar *x* 1)"
                    "  (block b (let ((*x* 2)) (return-from b 9))))",
                    da3, prog3, heap3, store3, envs3)
                .has_value());
    REQUIRE(envs3.dynamic_depth() == 0);

    DatumArena da4;

    Program prog4;
    Heap heap4;
    Store store4;
    Envs envs4;
    REQUIRE_FALSE(run_mut("(progn (defvar *x* 1) (let ((*x* 2)) (nope)))", da4,
                          prog4, heap4, store4, envs4)
                      .has_value());
    REQUIRE(envs4.dynamic_depth() == 0);
}

// -- the channel mapping itself ---------------------------------------------
// These have no counterpart in the closure backends, because the thing they
// witness does not exist there: which of three channels a program completed
// on. They are the sender backend's own merge criterion.

TEST_CASE("SenderProgramTest - OrdinaryValueUsesTheValueChannel") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto o = run_channel("(+ 1 2)", da, prog, heap, store, envs);
    REQUIRE(o.is_value());
}

TEST_CASE("SenderProgramTest - DiagnosedErrorUsesTheErrorChannel") {
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto o = run_channel("(nope)", da, prog, heap, store, envs);
    REQUIRE(o.is_error());
}

TEST_CASE("SenderProgramTest - StoppedIsAlwaysConsumedByItsOwnScope") {
    // A property of the design worth pinning, because it is what makes
    // `to_result`'s third branch defensive rather than reachable: no
    // well-formed program can complete on the stopped channel at top level.
    //
    // D5 makes all nonlocal control upward-only and one-shot, and a stop is
    // only ever *created* after `return-from`/`throw` has already found a
    // LIVE target -- `lookup_block` failing, or `find_catch` returning null,
    // is a diagnosed error on the error channel instead. So whenever a stop
    // exists, the scope that will consume it is already below on the stack.
    // The two failure modes that might look like an escaping unwind are both
    // errors, and both are checked above: `ReturnFromOfDeadBlockIs...` and
    // `UncaughtThrowIs...`.
    //
    // So these programs contain a genuine stop -- the `unwind-protect` cases
    // above prove one crosses the adapter -- and still complete with a value.
    DatumArena da;
    Program prog;
    Heap heap;
    Store store;
    Envs envs;
    auto o =
        run_channel("(block b (return-from b 1))", da, prog, heap, store, envs);
    REQUIRE(o.is_value());
    REQUIRE(std::get<int>(o.val) == 1);

    // The stop crosses an `unwind-protect` on its way to the block, so it
    // provably existed, and the block still consumes it.
    DatumArena da2;
    Program prog2;
    Heap heap2;
    Store store2;
    Envs envs2;
    auto o2 = run_channel("(block b (unwind-protect (return-from b 1) 0))", da2,
                          prog2, heap2, store2, envs2);
    REQUIRE(o2.is_value());
    REQUIRE(std::get<int>(o2.val) == 1);

    // Same for `throw`, whose target is found dynamically rather than
    // lexically.
    DatumArena da3;
    Program prog3;
    Heap heap3;
    Store store3;
    Envs envs3;
    auto o3 = run_channel("(catch 'c (unwind-protect (throw 'c 2) 0))", da3,
                          prog3, heap3, store3, envs3);
    REQUIRE(o3.is_value());
    REQUIRE(std::get<int>(o3.val) == 2);
    REQUIRE(envs3.catch_depth() == 0);
}

// -- multiple values are NOT supported here (step L20, DIV-0018) ---------

TEST_CASE("SenderProgramTest - ValuesIsDiagnosedRatherThanTruncated") {
    // The sender backend's carrier holds one value per completion, so step
    // L20's `values` builtin has nowhere to put the rest. Silently returning
    // the primary value would make this backend disagree with the other two
    // on a program that looks like it worked; a diagnosed error does not.
    // See DIV-0018 for what closing this gap costs.
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(values 1 2)", da, prog, heap, envs);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(std::string_view{r.error().message} ==
            "values: multiple values are not supported by the sender backend");
}

TEST_CASE("SenderProgramTest - MultipleValueBindIsDiagnosed") {
    DatumArena da;
    Program prog;
    Heap heap;
    Envs envs;
    auto r = run("(multiple-value-bind (a b) (values 1 2) (+ a b))", da, prog,
                 heap, envs);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(std::string_view{r.error().message} ==
            "multiple-value-bind is not supported by the sender backend");
}
