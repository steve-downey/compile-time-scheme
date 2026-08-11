// src/smd/cl/conformance/corpus.hpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_CONFORMANCE_CORPUS_HPP
#define SRC_SMD_CL_CONFORMANCE_CORPUS_HPP

#include <smd/cl/conformance/driver.hpp>
#include <smd/cl/eval/outcome.hpp>
#include <smd/cl/eval/value.hpp>

#include <array>
#include <string_view>
#include <variant>

/// The conformance corpus of decision D16: one place where a program's
/// spelling, its expected outcome, and the classification of any divergence
/// live together, run against `src/smd/cl/eval` through the
/// @ref smd::cl::conformance::evaluate_program driver.
///
/// Two provenances, tagged rather than filed apart, because the honest
/// history is one narrow subset adapted from `ansi-test` (`pfdietz/ansi-test`
/// at the pinned commit recorded in `THIRD-PARTY-LICENSES.md`) plus a
/// larger hand-derived set covering the rest of the operator surface —
/// `ansi-test` leans on strings, characters and the numeric tower (D10 puts
/// all three out of scope), so most of its files contribute nothing
/// adaptable, exactly as the plan's R6 section predicted.
///
/// Every entry states its expected outcome *by channel* — value, diagnosed
/// error, or unwind — per D16: collapsing the three back into one would
/// give back exactly what R5 spent its phase separating. Nothing here pins
/// a `defect` (`docs/divergences/README.md`): the small number of `eq`
/// entries pin DIV-0002, which is a `scope-decision` and therefore may be
/// pinned; nothing exercises `(block nil ...)`, lambda-list keywords, or any
/// of the other currently-diagnosed gaps as if they were correct answers.
namespace smd::cl::conformance {

/// Where a corpus entry's expected outcome came from — the whole of what
/// `may this be pinned` needs distinguishing (D16's classification lives in
/// `docs/divergences/README.md`; this is narrower: not "is the behaviour
/// intended" but "did a human derive it from the spec, or lift it from
/// `ansi-test`").
enum class provenance_kind : unsigned char {
    ansi_test_adapted, ///< Adapted from a `pfdietz/ansi-test` `deftest`.
    spec_derived       ///< Hand-derived from the ANSI specification.
};

/// The program's result is this fixnum.
struct expect_fixnum {
    int value = 0; ///< The expected integer.
};

/// The program's result is the canonical `t` or `nil` symbol.
struct expect_boolean {
    bool truthy = true; ///< `true` expects `t`, `false` expects `nil`.
};

/// The program's result is some symbol, without regard to which — used
/// where the check that matters is channel and shape, not identity (a
/// `defun`'s return value, for instance; `machine.test.cpp`'s
/// `defun_returns_its_name` makes the same choice for the same reason).
struct expect_any_symbol {};

/// The program is diagnosed: some `foundation::parse_error`, from any
/// pipeline stage.
struct expect_error {};

/// The program ends with an unwind in flight, carrying this fixnum, aimed
/// at no block the program itself catches.
struct expect_unwind_fixnum {
    int value = 0; ///< The expected payload.
};

/// One corpus entry's expected outcome, exactly one alternative per D16's
/// three channels plus the two value shapes a fixnum-and-symbol-only
/// operator surface actually produces.
using expectation =
    std::variant<expect_fixnum, expect_boolean, expect_any_symbol, expect_error,
                 expect_unwind_fixnum>;

/// One conformance case: a program, what it must do, and where the
/// expectation came from.
struct corpus_case {
    std::string_view name;      ///< A short, stable, human-readable name.
    std::string_view source;    ///< One or more top-level forms.
    expectation expect;         ///< The expected outcome, by channel.
    provenance_kind provenance; ///< Adapted, or hand-derived.
    std::string_view origin;    ///< The `ansi-test` file/deftest, or the ANSI
                                ///< section the spec-derived case encodes.
};

/// Returns true if @p report's completion is the fixnum @p e names.
[[nodiscard]] constexpr auto matches(program_report const &report,
                                     expect_fixnum e) -> bool {
    return report.how.is_value() &&
           std::holds_alternative<eval::value_fixnum>(report.how.as_value()) &&
           std::get<eval::value_fixnum>(report.how.as_value()).value == e.value;
}

/// Returns true if @p report's completion is exactly `t` or `nil`, per
/// @p e.truthy.
[[nodiscard]] constexpr auto matches(program_report const &report,
                                     expect_boolean e) -> bool {
    if (!report.how.is_value()) {
        return false;
    }
    auto const expected =
        eval::symbol_value(e.truthy ? report.known.t : report.known.nil);
    return report.how.as_value() == expected;
}

/// Returns true if @p report's completion is any symbol at all.
[[nodiscard]] constexpr auto matches(program_report const &report,
                                     expect_any_symbol) -> bool {
    return report.how.is_value() &&
           std::holds_alternative<eval::value_symbol>(report.how.as_value());
}

/// Returns true if @p report's completion is a diagnosed error.
[[nodiscard]] constexpr auto matches(program_report const &report, expect_error)
    -> bool {
    return report.how.is_error();
}

/// Returns true if @p report's completion is an unwind carrying the fixnum
/// @p e names.
[[nodiscard]] constexpr auto matches(program_report const &report,
                                     expect_unwind_fixnum e) -> bool {
    return report.how.is_unwind() &&
           std::holds_alternative<eval::value_fixnum>(
               report.how.as_unwind().payload) &&
           std::get<eval::value_fixnum>(report.how.as_unwind().payload).value ==
               e.value;
}

/// Runs @p c's source through @ref evaluate_program and checks it against
/// @p c's expectation.
[[nodiscard]] constexpr auto satisfies(corpus_case const &c) -> bool {
    auto const report = evaluate_program(c.source);
    return std::visit([&report](auto const &e) { return matches(report, e); },
                      c.expect);
}

// 6bafb7e1-11a8-48bd-b0b2-341a546df04d
/// Cases adapted from `pfdietz/ansi-test` (MIT; see
/// `THIRD-PARTY-LICENSES.md` for the copyright notice and the pinned
/// commit). Every source form here is the original `deftest`'s form,
/// unrestructured — a `deftest` body using `let`, `values`, `flet`, or any
/// other operator past this project's surface was discarded rather than
/// rewritten, per the fetch-and-adapt diligence this step was given.
///
/// What *is* adapted is how the expected result is checked: this harness
/// cannot inspect a returned symbol's name (the corpus driver deliberately
/// does not expose the symbol table, so a `program_report` outlives it), so
/// a `deftest` whose expected value is a bare symbol is wrapped in `eq` or
/// `null` against a quoted literal from the *same* source string — the same
/// symbol, interned once, in the same run. That wrapping changes nothing
/// about what CL form is under test.
inline constexpr std::array<corpus_case, 35> ansi_test_adapted{{
    // cons/cons.lsp
    {"cons.error.1", "(cons)", expect_error{},
     provenance_kind::ansi_test_adapted, "cons/cons.lsp deftest cons.error.1"},
    {"cons.error.2", "(cons 'a)", expect_error{},
     provenance_kind::ansi_test_adapted, "cons/cons.lsp deftest cons.error.2"},
    {"cons.error.3", "(cons 'a 'b 'c)", expect_error{},
     provenance_kind::ansi_test_adapted, "cons/cons.lsp deftest cons.error.3"},
    {"cons-of-symbols.car", "(eq (car (cons 'a 'b)) 'a)", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "cons/cons.lsp deftest cons-of-symbols (car half)"},
    {"cons-of-symbols.cdr", "(eq (cdr (cons 'a 'b)) 'b)", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "cons/cons.lsp deftest cons-of-symbols (cdr half)"},
    {"cons-with-nil.car", "(eq (car (cons 'a nil)) 'a)", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "cons/cons.lsp deftest cons-with-nil (car half)"},
    {"cons-with-nil.cdr", "(null (cdr (cons 'a nil)))", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "cons/cons.lsp deftest cons-with-nil (cdr half)"},
    // cons/cxr.lsp — car and cdr only; every compound accessor (caar,
    // cadr, ...) is out of the operator surface and was discarded.
    {"cxr.cons.23", "(eq (car (quote (a))) 'a)", expect_boolean{true},
     provenance_kind::ansi_test_adapted, "cons/cxr.lsp deftest cons.23"},
    // cons.24, (cdr '(a . b)) => b, is NOT adapted: it needs reader syntax
    // for the dotted-pair consing dot, which the reader does not implement
    // (D6, `docs/cl-pivot-plan.md`: "deferred until a step needs it; file a
    // divergence doc when deferring bites" — DIV-0022 is that doc).
    {"cxr.cdr.1.car", "(eq (car (cdr '(a b))) 'b)", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "cons/cxr.lsp deftest cdr.1 (car-of-cdr half)"},
    {"cxr.cdr.1.cdr", "(null (cdr (cdr '(a b))))", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "cons/cxr.lsp deftest cdr.1 (cdr-of-cdr half)"},
    // data-and-control-flow/if.lsp
    {"if.1", "(if t 1 2)", expect_fixnum{1}, provenance_kind::ansi_test_adapted,
     "data-and-control-flow/if.lsp deftest if.1"},
    {"if.2", "(if nil 1 2)", expect_fixnum{2},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/if.lsp deftest if.2"},
    {"if.4", "(null (if nil 'a))", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/if.lsp deftest if.4"},
    // data-and-control-flow/progn.lsp
    {"progn.1", "(null (progn))", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/progn.lsp deftest progn.1"},
    {"progn.2", "(eq (progn 'a) 'a)", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/progn.lsp deftest progn.2"},
    {"progn.3", "(eq (progn 'b 'a) 'a)", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/progn.lsp deftest progn.3"},
    // data-and-control-flow/block.lsp
    {"block.1", "(block foo (return-from foo 1))", expect_fixnum{1},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/block.lsp deftest block.1"},
    {"block.4",
     "(eq (block foo (block foo (return-from foo 'bad)) 'good) 'good)",
     expect_boolean{true}, provenance_kind::ansi_test_adapted,
     "data-and-control-flow/block.lsp deftest block.4"},
    {"block.8", "(null (block foo))", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/block.lsp deftest block.8"},
    // data-and-control-flow/return-from.lsp — the original's `:bad` is
    // spelled `'bad` here (a quoted symbol rather than a keyword); both are
    // self-evaluating data never reached once return-from fires, so the
    // substitution changes nothing about what is under test.
    {"return-from.1", "(null (block xyz (return-from xyz) 'bad))",
     expect_boolean{true}, provenance_kind::ansi_test_adapted,
     "data-and-control-flow/return-from.lsp deftest return-from.1"},
    // data-and-control-flow/nil.lsp
    {"nil.4", "(null (car nil))", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/nil.lsp deftest nil.4"},
    {"nil.5", "(null (cdr nil))", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/nil.lsp deftest nil.5"},
    // eqt/nil.8 pins DIV-0002 (eq and eql coincide), a `scope-decision`,
    // which D16 says a test may pin.
    {"nil.8", "(eq nil 'nil)", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/nil.lsp deftest nil.8"},
    // data-and-control-flow/defun.lsp — defun.1's shape (a function whose
    // body is only return-from, called after definition), renamed to avoid
    // colliding with any other corpus entry's function name.
    {"defun.1",
     "(defun conformance-defun-1 () (return-from conformance-defun-1 "
     "'good)) (eq (conformance-defun-1) 'good)",
     expect_boolean{true}, provenance_kind::ansi_test_adapted,
     "data-and-control-flow/defun.lsp deftest defun.1 (renamed)"},
    // numbers/plus.lsp, numbers/minus.lsp, numbers/times.lsp
    {"plus.1", "(+)", expect_fixnum{0}, provenance_kind::ansi_test_adapted,
     "numbers/plus.lsp deftest plus.1"},
    {"minus.error.1", "(-)", expect_error{}, provenance_kind::ansi_test_adapted,
     "numbers/minus.lsp deftest minus.error.1"},
    {"times.1", "(*)", expect_fixnum{1}, provenance_kind::ansi_test_adapted,
     "numbers/times.lsp deftest *.1"},
    // numbers/number-comparison.lsp — only the arity errors are usable
    // without `let`; every literal comparison test binds its operands with
    // `let` first, which is out of the operator surface and was discarded
    // rather than inlined (the equivalent facts are hand-derived below,
    // marked spec-derived rather than claimed as ansi-test's own forms).
    {"=.error.1", "(=)", expect_error{}, provenance_kind::ansi_test_adapted,
     "numbers/number-comparison.lsp deftest =.error.1"},
    {"<.error.1", "(<)", expect_error{}, provenance_kind::ansi_test_adapted,
     "numbers/number-comparison.lsp deftest <.error.1"},
    {">.error.1", "(>)", expect_error{}, provenance_kind::ansi_test_adapted,
     "numbers/number-comparison.lsp deftest >.error.1"},
    // sequences/length.lsp
    {"length.list.1", "(length nil)", expect_fixnum{0},
     provenance_kind::ansi_test_adapted,
     "sequences/length.lsp deftest length.list.1"},
    {"length.list.2", "(length '(a b c d e))", expect_fixnum{5},
     provenance_kind::ansi_test_adapted,
     "sequences/length.lsp deftest length.list.2"},
    {"length.error.6", "(length)", expect_error{},
     provenance_kind::ansi_test_adapted,
     "sequences/length.lsp deftest length.error.6"},
    {"length.error.7", "(length nil nil)", expect_error{},
     provenance_kind::ansi_test_adapted,
     "sequences/length.lsp deftest length.error.7"},
    // length.error.8's outer `(locally (length 'a) t)` is stripped to
    // `(length 'a)`: `locally` carries no declarations here, so it is inert
    // wrapper syntax around the one form actually under test, and `locally`
    // itself is not in this project's operator surface.
    {"length.error.8", "(length 'a)", expect_error{},
     provenance_kind::ansi_test_adapted,
     "sequences/length.lsp deftest length.error.8 (locally wrapper "
     "stripped)"},
}};
// 6bafb7e1-11a8-48bd-b0b2-341a546df04d end

/// Cases hand-derived from the ANSI specification, covering the operator
/// surface `ansi-test`'s directly-adaptable forms did not reach: `quote`
/// (no dedicated `ansi-test` file at all), `list`, `null`, `not`, `eq` and
/// `eql` beyond DIV-0002's single case, `atom`, `consp`, `symbolp`,
/// multi-argument arithmetic and comparison, `unwind-protect`'s three
/// channels (every `ansi-test` case uses `let`, `push`, `catch` or
/// `tagbody`), an unaimed `return-from` reaching the top as an unwind
/// (D13), and the recursive-`defun` acceptance witness (DIV-0009).
inline constexpr std::array<corpus_case, 40> spec_derived{{
    // quote — ANSI 2.4.6: 'x reads as (quote x); quote returns its argument
    // unevaluated.
    {"quote.fixnum", "(quote 5)", expect_fixnum{5},
     provenance_kind::spec_derived, "ANSI 2.4.6, 3.1.2.1.1 (quote)"},
    {"quote.list-car", "(car (quote (1 2 3)))", expect_fixnum{1},
     provenance_kind::spec_derived, "ANSI 2.4.6, 3.1.2.1.1 (quote)"},
    {"quote.shorthand", "(eq (quote a) 'a)", expect_boolean{true},
     provenance_kind::spec_derived, "ANSI 2.4.6 ('x is (quote x))"},
    // list — ANSI 5.1 (function LIST)
    {"list.car", "(car (list 1 2 3))", expect_fixnum{1},
     provenance_kind::spec_derived, "ANSI 5.1 (function LIST)"},
    {"list.length", "(length (list 1 2 3))", expect_fixnum{3},
     provenance_kind::spec_derived, "ANSI 5.1 (function LIST)"},
    {"list.empty", "(null (list))", expect_boolean{true},
     provenance_kind::spec_derived, "ANSI 5.1 ((list) is nil)"},
    // null / not — ANSI 5.1, 6.1: both true of exactly nil.
    {"null.true", "(null nil)", expect_boolean{true},
     provenance_kind::spec_derived, "ANSI 5.1 (function NULL)"},
    {"null.false", "(null 1)", expect_boolean{false},
     provenance_kind::spec_derived, "ANSI 5.1 (function NULL)"},
    {"not.true", "(not nil)", expect_boolean{true},
     provenance_kind::spec_derived, "ANSI 6.1 (function NOT)"},
    {"not.false", "(not 5)", expect_boolean{false},
     provenance_kind::spec_derived, "ANSI 6.1 (function NOT)"},
    // eq / eql — ANSI 3.2, 7.1 (eq/eql, symbols always eq to themselves;
    // eql additionally defined on numbers of the same type and value).
    {"eq.same-symbol", "(eq 'a 'a)", expect_boolean{true},
     provenance_kind::spec_derived, "ANSI 3.2 (symbols are eq to themselves)"},
    {"eq.different-symbols", "(eq 'a 'b)", expect_boolean{false},
     provenance_kind::spec_derived, "ANSI 3.2"},
    {"eql.same-fixnum", "(eql 3 3)", expect_boolean{true},
     provenance_kind::spec_derived,
     "ANSI 3.2 (eql on numbers of the same type and value)"},
    {"eql.different-fixnum", "(eql 3 4)", expect_boolean{false},
     provenance_kind::spec_derived, "ANSI 3.2"},
    // atom / consp — ANSI 3.2, 14.1: complementary except both false of
    // nothing (every object is either a cons or an atom).
    {"atom.fixnum", "(atom 1)", expect_boolean{true},
     provenance_kind::spec_derived, "ANSI 3.2 (function ATOM)"},
    {"atom.cons", "(atom (cons 1 2))", expect_boolean{false},
     provenance_kind::spec_derived, "ANSI 3.2 (function ATOM)"},
    {"consp.cons", "(consp (cons 1 2))", expect_boolean{true},
     provenance_kind::spec_derived, "ANSI 3.2 (function CONSP)"},
    {"consp.fixnum", "(consp 1)", expect_boolean{false},
     provenance_kind::spec_derived, "ANSI 3.2 (function CONSP)"},
    {"consp.nil", "(consp nil)", expect_boolean{false},
     provenance_kind::spec_derived, "ANSI 3.2 (nil is a symbol, not a cons)"},
    // symbolp — ANSI 3.2: nil and t are ordinary symbols.
    {"symbolp.symbol", "(symbolp 'a)", expect_boolean{true},
     provenance_kind::spec_derived, "ANSI 3.2 (function SYMBOLP)"},
    {"symbolp.fixnum", "(symbolp 1)", expect_boolean{false},
     provenance_kind::spec_derived, "ANSI 3.2 (function SYMBOLP)"},
    {"symbolp.nil", "(symbolp nil)", expect_boolean{true},
     provenance_kind::spec_derived, "ANSI 3.2 (nil is a symbol)"},
    // Multi-argument arithmetic and comparison — ANSI 12.1 (+, -, *),
    // 12.2 (numeric comparison), n-ary in every case.
    {"plus.multi", "(+ 1 2 3)", expect_fixnum{6}, provenance_kind::spec_derived,
     "ANSI 12.1.1 (function +)"},
    {"minus.binary", "(- 5 2)", expect_fixnum{3}, provenance_kind::spec_derived,
     "ANSI 12.1.1 (function -)"},
    {"minus.unary", "(- 5)", expect_fixnum{-5}, provenance_kind::spec_derived,
     "ANSI 12.1.1 (unary - negates)"},
    {"times.multi", "(* 2 3 4)", expect_fixnum{24},
     provenance_kind::spec_derived, "ANSI 12.1.1 (function *)"},
    {"less.true", "(< 0 1)", expect_boolean{true},
     provenance_kind::spec_derived, "ANSI 12.2 (function <)"},
    {"less.false", "(< 1 0)", expect_boolean{false},
     provenance_kind::spec_derived, "ANSI 12.2 (function <)"},
    {"greater.true", "(> 1 0)", expect_boolean{true},
     provenance_kind::spec_derived, "ANSI 12.2 (function >)"},
    {"greater.false", "(> 0 1)", expect_boolean{false},
     provenance_kind::spec_derived, "ANSI 12.2 (function >)"},
    {"numeric-equal.true", "(= 1 1 1)", expect_boolean{true},
     provenance_kind::spec_derived,
     "ANSI 12.2 (= is n-ary: no adjacent pair unequal)"},
    {"numeric-equal.false", "(= 1 2)", expect_boolean{false},
     provenance_kind::spec_derived, "ANSI 12.2 (function =)"},
    // unwind-protect — ANSI 5.2, 7.8.8: the protected form's own completion
    // survives the cleanup on every channel; the only exception is a
    // transfer out of the cleanup itself, which replaces it. Every
    // `ansi-test` unwind-protect.lsp case uses `let`, `push`, `catch` or
    // `tagbody`, none of them adaptable, so this whole group is
    // spec-derived.
    {"unwind-protect.value", "(unwind-protect 1 2 3)", expect_fixnum{1},
     provenance_kind::spec_derived,
     "ANSI 7.8.8 (cleanup forms' values are discarded)"},
    {"unwind-protect.error-propagates", "(unwind-protect (car 2) 1)",
     expect_error{}, provenance_kind::spec_derived,
     "ANSI 7.8.8 (the protected form's error is not masked)"},
    {"unwind-protect.unwind-becomes-value",
     "(block out (unwind-protect (return-from out 6) 1))", expect_fixnum{6},
     provenance_kind::spec_derived,
     "ANSI 7.8.8 and 5.2 (a caught unwind is the protected form's value)"},
    {"unwind-protect.cleanup-transfer-wins", "(unwind-protect 1 (car 2))",
     expect_error{}, provenance_kind::spec_derived,
     "ANSI 7.8.8 (a transfer out of a cleanup replaces the protected "
     "form's completion)"},
    // An unaimed return-from reaches the top as an unwind, not an error —
    // D13's whole point, and the third channel's only producer besides
    // unwind-protect.
    {"return-from.unaimed", "(return-from nowhere 4)", expect_unwind_fixnum{4},
     provenance_kind::spec_derived,
     "D13 (control flow does not travel in the error channel)"},
    // defun — arity checking and the undefined-function diagnostic; neither
    // is reachable through ansi-test's adaptable defun.lsp subset.
    {"defun.wrong-arity", "(defun f (x) x) (f 1 2)", expect_error{},
     provenance_kind::spec_derived,
     "ANSI 3.1.2.1.2.4 (wrong number of arguments is an error)"},
    {"call.undefined-function", "(nosuchfunction 1)", expect_error{},
     provenance_kind::spec_derived,
     "ANSI 3.1.2.1.2.4 (an undefined function is an error)"},
    // The recursive-defun acceptance witness (DIV-0009), as a corpus entry:
    // the callee resolves in the function slot at call time, so a body
    // that names itself finds the definition being installed.
    {"defun.recursive",
     "(defun conformance-len (l) (if (null l) 0 (+ 1 (conformance-len (cdr "
     "l))))) (conformance-len '(a b c))",
     expect_fixnum{3}, provenance_kind::spec_derived,
     "DIV-0009 (recursive defun, closed by decision D12)"},
}};
// end spec_derived

} // namespace smd::cl::conformance

#endif
