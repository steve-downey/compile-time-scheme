// src/smd/smdlisp/elaborator/elaborate.test.cpp                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/elaborator/elaborate.hpp>
#include <smd/smdlisp/elaborator/elaborate.hpp> // test 2nd include OK

#include <smd/smdlisp/reader/read_datum.hpp>
#include <smd/smdscheme/parser/cursor.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
namespace lisp = smd::smdlisp;
namespace scm = smd::smdscheme;

constexpr int MaxNodes = 64;
constexpr int MaxList = 16;
using Core = lisp::elaborator::core_type<MaxNodes, MaxList>;
using DatumArena =
    scm::foundation::tree_arena<lisp::reader::datum_type<MaxNodes, MaxList>,
                                MaxNodes>;
using CoreArena = scm::foundation::tree_arena<Core, MaxNodes>;

/// Reads then elaborates @p src, returning the root core node or an error.
/// @p core_arena must outlive any use of the result (it owns every
/// sub-node the returned tree references via arena_box). @p datum_arena
/// does not need to: core_symbol/core_keyword copy their folded name out of
/// the datum tree rather than viewing into it, precisely so the returned
/// core tree survives independently of datum_arena and of the read_datum
/// call's own root-datum temporary (see lisp::elaborator::core_symbol's
/// docs for why that copy is required, not just defensive).
auto elab(std::string_view src, DatumArena &datum_arena, CoreArena &core_arena)
    -> scm::foundation::result<Core> {
    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{src}, datum_arena);
    if (!dr.has_value())
        return dr.error();
    return lisp::elaborator::elaborate<MaxNodes, MaxList>(
        dr.value().value, datum_arena, core_arena);
}

} // namespace

TEST_CASE("ElaborateTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- atoms --------------------------------------------------------------

TEST_CASE("ElaborateTest - IntegerElaboratesToCoreInteger") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("42", da, ca);
    REQUIRE(er.has_value());
    REQUIRE(std::holds_alternative<lisp::elaborator::core_integer>(
        er.value().inner));
    REQUIRE(std::get<lisp::elaborator::core_integer>(er.value().inner).value ==
            42);
}

TEST_CASE("ElaborateTest - SymbolElaboratesToVariableNamespaceRef") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("foo", da, ca);
    REQUIRE(er.has_value());
    REQUIRE(std::holds_alternative<lisp::elaborator::core_symbol>(
        er.value().inner));
    REQUIRE(
        std::get<lisp::elaborator::core_symbol>(er.value().inner).name.view() ==
        "FOO");
}

TEST_CASE("ElaborateTest - NilSymbolElaboratesToCoreNil") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("nil", da, ca);
    REQUIRE(er.has_value());
    REQUIRE(
        std::holds_alternative<lisp::elaborator::core_nil>(er.value().inner));
}

TEST_CASE("ElaborateTest - TSymbolElaboratesToCoreTrue") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("t", da, ca);
    REQUIRE(er.has_value());
    REQUIRE(
        std::holds_alternative<lisp::elaborator::core_true>(er.value().inner));
}

TEST_CASE("ElaborateTest - KeywordElaboratesToCoreKeyword") {
    DatumArena da;
    CoreArena ca;
    auto er = elab(":foo", da, ca);
    REQUIRE(er.has_value());
    REQUIRE(std::holds_alternative<lisp::elaborator::core_keyword>(
        er.value().inner));
    REQUIRE(std::get<lisp::elaborator::core_keyword>(er.value().inner)
                .name.view() == "FOO");
}

// -- quote ----------------------------------------------------------------

TEST_CASE("ElaborateTest - QuotedSymbolElaboratesToCoreQuote") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("'foo", da, ca);
    REQUIRE(er.has_value());
    REQUIRE(
        std::holds_alternative<lisp::elaborator::core_quote>(er.value().inner));
    auto const &q = std::get<lisp::elaborator::core_quote>(er.value().inner);
    REQUIRE(std::holds_alternative<lisp::elaborator::core_symbol>(q.atom));
    REQUIRE(std::get<lisp::elaborator::core_symbol>(q.atom).name.view() ==
            "FOO");
}

TEST_CASE("ElaborateTest - QuotedIntegerElaboratesToCoreQuote") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("'42", da, ca);
    REQUIRE(er.has_value());
    auto const &q = std::get<lisp::elaborator::core_quote>(er.value().inner);
    REQUIRE(std::holds_alternative<int>(q.atom));
    REQUIRE(std::get<int>(q.atom) == 42);
}

TEST_CASE("ElaborateTest - QuotedNilElaboratesToCoreNilNotCoreQuote") {
    // Quoting NIL is a no-op per D3: 'nil and nil denote the identical
    // value, so there is no separate "quoted nil" representation.
    DatumArena da;
    CoreArena ca;
    auto er = elab("'nil", da, ca);
    REQUIRE(er.has_value());
    REQUIRE(
        std::holds_alternative<lisp::elaborator::core_nil>(er.value().inner));
}

TEST_CASE("ElaborateTest - QuotedEmptyListElaboratesToCoreNil") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("'()", da, ca);
    REQUIRE(er.has_value());
    REQUIRE(
        std::holds_alternative<lisp::elaborator::core_nil>(er.value().inner));
}

TEST_CASE("ElaborateTest - QuotedListElaboratesToConsChain") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("'(1 2 3)", da, ca);
    REQUIRE(er.has_value());
    using Cons = lisp::elaborator::core_cons<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<Cons>(er.value().inner));

    auto const &c1 = std::get<Cons>(er.value().inner);
    REQUIRE(std::holds_alternative<lisp::elaborator::core_quote>(
        ca.get(c1.car).inner));
    REQUIRE(std::get<int>(
                std::get<lisp::elaborator::core_quote>(ca.get(c1.car).inner)
                    .atom) == 1);

    auto const &c2 = std::get<Cons>(ca.get(c1.cdr).inner);
    REQUIRE(std::get<int>(
                std::get<lisp::elaborator::core_quote>(ca.get(c2.car).inner)
                    .atom) == 2);

    auto const &c3 = std::get<Cons>(ca.get(c2.cdr).inner);
    REQUIRE(std::get<int>(
                std::get<lisp::elaborator::core_quote>(ca.get(c3.car).inner)
                    .atom) == 3);

    REQUIRE(std::holds_alternative<lisp::elaborator::core_nil>(
        ca.get(c3.cdr).inner));
}

TEST_CASE("ElaborateTest - NestedQuoteBuildsQuoteList") {
    // ''x -> the two-element list (QUOTE X).
    DatumArena da;
    CoreArena ca;
    auto er = elab("''x", da, ca);
    REQUIRE(er.has_value());
    using Cons = lisp::elaborator::core_cons<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<Cons>(er.value().inner));
    auto const &outer = std::get<Cons>(er.value().inner);
    auto const &head_quote =
        std::get<lisp::elaborator::core_quote>(ca.get(outer.car).inner);
    REQUIRE(
        std::get<lisp::elaborator::core_symbol>(head_quote.atom).name.view() ==
        "QUOTE");
}

TEST_CASE("ElaborateTest - QuoteUnsupportedArityIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(quote)", da, ca).has_value());
    REQUIRE(!elab("(quote a b)", da, ca).has_value());
}

// -- function / #' ----------------------------------------------------------

TEST_CASE("ElaborateTest - SharpQuoteOfSymbolElaboratesToCoreFunction") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("#'foo", da, ca);
    REQUIRE(er.has_value());
    using Fn = lisp::elaborator::core_function<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<Fn>(er.value().inner));
    auto const &fn = std::get<Fn>(er.value().inner);
    REQUIRE(std::holds_alternative<std::string_view>(fn.target));
    REQUIRE(std::get<std::string_view>(fn.target) == "FOO");
}

TEST_CASE("ElaborateTest - FunctionFormOfSymbolMatchesSharpQuote") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(function foo)", da, ca);
    REQUIRE(er.has_value());
    using Fn = lisp::elaborator::core_function<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<Fn>(er.value().inner));
    REQUIRE(std::get<std::string_view>(std::get<Fn>(er.value().inner).target) ==
            "FOO");
}

TEST_CASE("ElaborateTest - SharpQuoteOfLambdaEmbedsLambdaNode") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("#'(lambda (x) x)", da, ca);
    REQUIRE(er.has_value());
    using Fn = lisp::elaborator::core_function<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<Fn>(er.value().inner));
    auto const &fn = std::get<Fn>(er.value().inner);
    using Handle = smd::smdscheme::foundation::arena_box<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<Handle>(fn.target));
    REQUIRE(std::holds_alternative<
            lisp::elaborator::core_lambda<Core, MaxNodes, MaxList>>(
        ca.get(std::get<Handle>(fn.target)).inner));
}

TEST_CASE("ElaborateTest - FunctionOfNonSymbolNonLambdaIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("#'42", da, ca).has_value());
    REQUIRE(!elab("(function 1 2)", da, ca).has_value());
}

// -- if ---------------------------------------------------------------------

TEST_CASE("ElaborateTest - IfThreeArgsElaboratesToCoreIf") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(if 1 2 3)", da, ca);
    REQUIRE(er.has_value());
    using If = lisp::elaborator::core_if<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<If>(er.value().inner));
    auto const &node = std::get<If>(er.value().inner);
    REQUIRE(
        std::get<lisp::elaborator::core_integer>(ca.get(node.alternative).inner)
            .value == 3);
}

TEST_CASE("ElaborateTest - IfTwoArgsGetsImplicitNilAlternative") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(if 1 2)", da, ca);
    REQUIRE(er.has_value());
    using If = lisp::elaborator::core_if<Core, MaxNodes>;
    auto const &node = std::get<If>(er.value().inner);
    REQUIRE(std::holds_alternative<lisp::elaborator::core_nil>(
        ca.get(node.alternative).inner));
}

TEST_CASE("ElaborateTest - IfWrongArityIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(if 1)", da, ca).has_value());
    REQUIRE(!elab("(if)", da, ca).has_value());
    REQUIRE(!elab("(if 1 2 3 4)", da, ca).has_value());
}

// -- progn --------------------------------------------------------------

TEST_CASE("ElaborateTest - PrognElaboratesToCoreProgn") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(progn 1 2 3)", da, ca);
    REQUIRE(er.has_value());
    using Progn = lisp::elaborator::core_progn<Core, MaxNodes, MaxList>;
    REQUIRE(std::holds_alternative<Progn>(er.value().inner));
    REQUIRE(std::get<Progn>(er.value().inner).exprs.size() == 3);
}

TEST_CASE("ElaborateTest - PrognEmptyIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(progn)", da, ca).has_value());
}

// -- lambda -------------------------------------------------------------

TEST_CASE("ElaborateTest - LambdaElaboratesToCoreLambdaWithImplicitProgn") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(lambda (x y) x y)", da, ca);
    REQUIRE(er.has_value());
    using Lam = lisp::elaborator::core_lambda<Core, MaxNodes, MaxList>;
    REQUIRE(std::holds_alternative<Lam>(er.value().inner));
    auto const &lam = std::get<Lam>(er.value().inner);
    REQUIRE(lam.params.size() == 2);
    REQUIRE(lam.params[0] == "X");
    REQUIRE(lam.params[1] == "Y");
    REQUIRE(lam.body.size() == 2);
}

TEST_CASE("ElaborateTest - LambdaDuplicateParamIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(lambda (x x) x)", da, ca).has_value());
}

TEST_CASE("ElaborateTest - LambdaNonListFormalsIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(lambda x x)", da, ca).has_value());
}

TEST_CASE("ElaborateTest - LambdaEmptyBodyIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(lambda (x))", da, ca).has_value());
}

// -- setq / defun / defvar / defparameter (step L12) ---------------------

TEST_CASE("ElaborateTest - SetqElaboratesToCoreSetq") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(setq x 1)", da, ca);
    REQUIRE(er.has_value());
    using Setq = lisp::elaborator::core_setq<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<Setq>(er.value().inner));
    REQUIRE(std::get<Setq>(er.value().inner).name == "X");
}

TEST_CASE("ElaborateTest - SetqWrongArityIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(setq x)", da, ca).has_value());
    REQUIRE(!elab("(setq x 1 2)", da, ca).has_value());
}

TEST_CASE("ElaborateTest - SetqNonSymbolNameIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(setq 1 2)", da, ca).has_value());
}

TEST_CASE("ElaborateTest - DefunElaboratesToCoreDefunWrappingLambda") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(defun twice (x) (+ x x))", da, ca);
    REQUIRE(er.has_value());
    using Defun = lisp::elaborator::core_defun<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<Defun>(er.value().inner));
    auto const &def = std::get<Defun>(er.value().inner);
    REQUIRE(def.name == "TWICE");
    using Lam = lisp::elaborator::core_lambda<Core, MaxNodes, MaxList>;
    auto const &lam_node = ca.get(def.lambda);
    REQUIRE(std::holds_alternative<Lam>(lam_node.inner));
    auto const &lam = std::get<Lam>(lam_node.inner);
    REQUIRE(lam.params.size() == 1);
    REQUIRE(lam.params[0] == "X");
    REQUIRE(lam.body.size() == 1);
}

TEST_CASE("ElaborateTest - DefunMultiFormBodyIsImplicitProgn") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(defun f (x) x (+ x 1))", da, ca);
    REQUIRE(er.has_value());
    using Defun = lisp::elaborator::core_defun<Core, MaxNodes>;
    auto const &def = std::get<Defun>(er.value().inner);
    using Lam = lisp::elaborator::core_lambda<Core, MaxNodes, MaxList>;
    auto const &lam = std::get<Lam>(ca.get(def.lambda).inner);
    REQUIRE(lam.body.size() == 2);
}

TEST_CASE("ElaborateTest - DefunWrongArityIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(defun f (x))", da, ca).has_value());
    REQUIRE(!elab("(defun f)", da, ca).has_value());
}

TEST_CASE("ElaborateTest - DefunNonSymbolNameIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(defun 1 (x) x)", da, ca).has_value());
}

TEST_CASE("ElaborateTest - DefunDuplicateParamIsError") {
    // Reuses elaborate_lambda's own checks -- pins that reuse actually
    // happens rather than a separate, laxer formals parser for defun.
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(defun f (x x) x)", da, ca).has_value());
}

TEST_CASE("ElaborateTest - DefvarElaboratesToCoreDefvar") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(defvar x 1)", da, ca);
    REQUIRE(er.has_value());
    using Defvar = lisp::elaborator::core_defvar<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<Defvar>(er.value().inner));
    REQUIRE(std::get<Defvar>(er.value().inner).name == "X");
}

TEST_CASE("ElaborateTest - DefvarWrongArityIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(defvar x)", da, ca).has_value());
}

TEST_CASE("ElaborateTest - DefparameterElaboratesToCoreDefparameter") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(defparameter x 1)", da, ca);
    REQUIRE(er.has_value());
    using Defparam = lisp::elaborator::core_defparameter<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<Defparam>(er.value().inner));
    REQUIRE(std::get<Defparam>(er.value().inner).name == "X");
}

TEST_CASE("ElaborateTest - DefparameterWrongArityIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(defparameter x)", da, ca).has_value());
}

// -- let / let* -----------------------------------------------------------

TEST_CASE("ElaborateTest - LetDesugarsToApplicationOfLambda") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(let ((x 1) (y 2)) x y)", da, ca);
    REQUIRE(er.has_value());
    using App = lisp::elaborator::core_application<Core, MaxNodes, MaxList>;
    using Fn = lisp::elaborator::core_function<Core, MaxNodes>;
    using Lam = lisp::elaborator::core_lambda<Core, MaxNodes, MaxList>;
    using Handle = smd::smdscheme::foundation::arena_box<Core, MaxNodes>;

    REQUIRE(std::holds_alternative<App>(er.value().inner));
    auto const &app = std::get<App>(er.value().inner);
    REQUIRE(app.args.size() == 2);

    auto const &fn = std::get<Fn>(ca.get(app.func).inner);
    REQUIRE(std::holds_alternative<Handle>(fn.target));
    auto const &lam = std::get<Lam>(ca.get(std::get<Handle>(fn.target)).inner);
    REQUIRE(lam.params.size() == 2);
    REQUIRE(lam.params[0] == "X");
    REQUIRE(lam.params[1] == "Y");
    REQUIRE(lam.body.size() == 2);
}

TEST_CASE("ElaborateTest - LetDuplicateBindingNameIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(let ((x 1) (x 2)) x)", da, ca).has_value());
}

TEST_CASE("ElaborateTest - LetStarNestsLambdaApplicationsPerBinding") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(let* ((x 1) (y x)) y)", da, ca);
    REQUIRE(er.has_value());
    using App = lisp::elaborator::core_application<Core, MaxNodes, MaxList>;
    using Fn = lisp::elaborator::core_function<Core, MaxNodes>;
    using Lam = lisp::elaborator::core_lambda<Core, MaxNodes, MaxList>;
    using Handle = smd::smdscheme::foundation::arena_box<Core, MaxNodes>;

    // Outer layer binds X.
    auto const &outer_app = std::get<App>(er.value().inner);
    auto const &outer_fn = std::get<Fn>(ca.get(outer_app.func).inner);
    auto const &outer_lam =
        std::get<Lam>(ca.get(std::get<Handle>(outer_fn.target)).inner);
    REQUIRE(outer_lam.params.size() == 1);
    REQUIRE(outer_lam.params[0] == "X");
    REQUIRE(outer_lam.body.size() == 1);

    // Inner layer (the outer lambda's body) binds Y, with the original body.
    auto const &inner_app = std::get<App>(ca.get(outer_lam.body[0]).inner);
    auto const &inner_fn = std::get<Fn>(ca.get(inner_app.func).inner);
    auto const &inner_lam =
        std::get<Lam>(ca.get(std::get<Handle>(inner_fn.target)).inner);
    REQUIRE(inner_lam.params.size() == 1);
    REQUIRE(inner_lam.params[0] == "Y");
    REQUIRE(inner_lam.body.size() == 1);
}

TEST_CASE("ElaborateTest - LetStarEmptyBindingsIsImplicitProgn") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(let* () 1 2)", da, ca);
    REQUIRE(er.has_value());
    using Progn = lisp::elaborator::core_progn<Core, MaxNodes, MaxList>;
    REQUIRE(std::holds_alternative<Progn>(er.value().inner));
    REQUIRE(std::get<Progn>(er.value().inner).exprs.size() == 2);
}

TEST_CASE("ElaborateTest - LetWrongShapeIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(let (x 1) x)", da, ca).has_value());
    REQUIRE(!elab("(let ((x 1)))", da, ca).has_value());
}

// -- application ------------------------------------------------------------

TEST_CASE("ElaborateTest - ApplicationHeadSymbolResolvesInFunctionNamespace") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(foo 1 2)", da, ca);
    REQUIRE(er.has_value());
    using App = lisp::elaborator::core_application<Core, MaxNodes, MaxList>;
    using Fn = lisp::elaborator::core_function<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<App>(er.value().inner));
    auto const &app = std::get<App>(er.value().inner);
    REQUIRE(app.args.size() == 2);
    auto const &fn = std::get<Fn>(ca.get(app.func).inner);
    REQUIRE(std::holds_alternative<std::string_view>(fn.target));
    REQUIRE(std::get<std::string_view>(fn.target) == "FOO");
}

TEST_CASE("ElaborateTest - ApplicationHeadLambdaIsAllowed") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("((lambda (x) x) 5)", da, ca);
    REQUIRE(er.has_value());
    using App = lisp::elaborator::core_application<Core, MaxNodes, MaxList>;
    using Fn = lisp::elaborator::core_function<Core, MaxNodes>;
    using Handle = smd::smdscheme::foundation::arena_box<Core, MaxNodes>;
    auto const &app = std::get<App>(er.value().inner);
    auto const &fn = std::get<Fn>(ca.get(app.func).inner);
    REQUIRE(std::holds_alternative<Handle>(fn.target));
}

TEST_CASE("ElaborateTest - ApplicationHeadNonSymbolNonLambdaIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(1 2 3)", da, ca).has_value());
}

TEST_CASE("ElaborateTest - EmptyApplicationIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("()", da, ca).has_value());
}

TEST_CASE("ElaborateTest - FuncallAndApplyAreOrdinaryApplications") {
    // funcall/apply are not special operators; they elaborate as ordinary
    // applications, resolved at evaluation time from the default
    // environment's FUNCTION namespace (step L9).
    DatumArena da;
    CoreArena ca;
    auto er = elab("(funcall #'foo 1)", da, ca);
    REQUIRE(er.has_value());
    using App = lisp::elaborator::core_application<Core, MaxNodes, MaxList>;
    using Fn = lisp::elaborator::core_function<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<App>(er.value().inner));
    auto const &fn =
        std::get<Fn>(ca.get(std::get<App>(er.value().inner).func).inner);
    REQUIRE(std::get<std::string_view>(fn.target) == "FUNCALL");
}

// -- error positions --------------------------------------------------------

TEST_CASE("ElaborateTest - ElaboratorLevelErrorsUseDefaultPosition") {
    // Elaborator-level (malformed-form) errors carry a foundation::parse_error
    // (source_pos/span from foundation), exactly as the Scheme elaborator's
    // do. The datum tree does not thread source positions through its
    // nodes, so there is no real position to recover once a datum has
    // already been read; the position is the default-constructed
    // source_pos, matching the Scheme elaborator's behavior precisely.
    DatumArena da;
    CoreArena ca;
    auto er = elab("(if 1)", da, ca);
    REQUIRE(!er.has_value());
    REQUIRE(er.error().where == smd::smdscheme::foundation::source_pos{});
}

TEST_CASE("ElaborateTest - ReaderLevelErrorsCarryRealPositions") {
    // In contrast, an error surfaced by the reader (before elaboration even
    // starts) does carry a real, non-default position.
    DatumArena da;
    CoreArena ca;
    auto er = elab("(if 1 2", da, ca);
    REQUIRE(!er.has_value());
    REQUIRE(er.error().where != smd::smdscheme::foundation::source_pos{});
}
