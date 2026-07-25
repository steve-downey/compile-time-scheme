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

// -- setq / defun / defvar / defparameter (step L12) -----------------------

TEST_CASE("ElaborateTest - SetqOnePairElaboratesToCoreSetq") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(setq x 1)", da, ca);
    REQUIRE(er.has_value());
    using Setq = lisp::elaborator::core_setq<Core, MaxNodes, MaxList>;
    REQUIRE(std::holds_alternative<Setq>(er.value().inner));
    auto const &sq = std::get<Setq>(er.value().inner);
    REQUIRE(sq.names.size() == 1);
    REQUIRE(sq.names[0] == "X");
    REQUIRE(sq.exprs.size() == 1);
}

TEST_CASE("ElaborateTest - SetqMultiplePairsElaboratesInOrder") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(setq x 1 y 2)", da, ca);
    REQUIRE(er.has_value());
    using Setq = lisp::elaborator::core_setq<Core, MaxNodes, MaxList>;
    REQUIRE(std::holds_alternative<Setq>(er.value().inner));
    auto const &sq = std::get<Setq>(er.value().inner);
    REQUIRE(sq.names.size() == 2);
    REQUIRE(sq.names[0] == "X");
    REQUIRE(sq.names[1] == "Y");
    REQUIRE(sq.exprs.size() == 2);
}

TEST_CASE("ElaborateTest - SetqOddArgumentCountIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(setq x 1 y)", da, ca).has_value());
}

TEST_CASE("ElaborateTest - SetqTargetMustBeSymbol") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(setq 1 2)", da, ca).has_value());
}

TEST_CASE("ElaborateTest - DefunElaboratesToCoreDefunWithLambda") {
    // Step L14: a defun body is implicitly wrapped in (block name ...), so
    // the lambda's own body is a single core_block node named after the
    // function, and *that* block's body holds the original expression(s) --
    // see core_block's and core_defun's docs.
    DatumArena da;
    CoreArena ca;
    auto er = elab("(defun twice (x) (+ x x))", da, ca);
    REQUIRE(er.has_value());
    using Defun = lisp::elaborator::core_defun<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<Defun>(er.value().inner));
    auto const &df = std::get<Defun>(er.value().inner);
    REQUIRE(df.name == "TWICE");
    using Lam = lisp::elaborator::core_lambda<Core, MaxNodes, MaxList>;
    REQUIRE(std::holds_alternative<Lam>(ca.get(df.lambda_node).inner));
    auto const &lam = std::get<Lam>(ca.get(df.lambda_node).inner);
    REQUIRE(lam.params.size() == 1);
    REQUIRE(lam.params[0] == "X");
    REQUIRE(lam.body.size() == 1);

    using Block = lisp::elaborator::core_block<Core, MaxNodes, MaxList>;
    REQUIRE(std::holds_alternative<Block>(ca.get(lam.body[0]).inner));
    auto const &blk = std::get<Block>(ca.get(lam.body[0]).inner);
    REQUIRE(blk.name == "TWICE");
    REQUIRE(blk.body.size() == 1);
    using App = lisp::elaborator::core_application<Core, MaxNodes, MaxList>;
    REQUIRE(std::holds_alternative<App>(ca.get(blk.body[0]).inner));
}

TEST_CASE("ElaborateTest - DefunNameMustBeSymbol") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(defun 1 (x) x)", da, ca).has_value());
}

TEST_CASE("ElaborateTest - DefunTooFewArgumentsIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(defun twice (x))", da, ca).has_value());
}

TEST_CASE("ElaborateTest - DefvarWithInitElaboratesToCoreDefvar") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(defvar *x* 1)", da, ca);
    REQUIRE(er.has_value());
    using Defvar = lisp::elaborator::core_defvar<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<Defvar>(er.value().inner));
    auto const &dv = std::get<Defvar>(er.value().inner);
    REQUIRE(dv.name == "*X*");
    REQUIRE(dv.has_init);
    REQUIRE_FALSE(dv.is_parameter);
}

TEST_CASE("ElaborateTest - DefvarWithoutInitOnlyMarksSpecial") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(defvar *x*)", da, ca);
    REQUIRE(er.has_value());
    using Defvar = lisp::elaborator::core_defvar<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<Defvar>(er.value().inner));
    REQUIRE_FALSE(std::get<Defvar>(er.value().inner).has_init);
}

TEST_CASE("ElaborateTest - DefparameterAlwaysRequiresInit") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(defparameter *x*)", da, ca).has_value());
    auto er = elab("(defparameter *x* 1)", da, ca);
    REQUIRE(er.has_value());
    using Defvar = lisp::elaborator::core_defvar<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<Defvar>(er.value().inner));
    auto const &dv = std::get<Defvar>(er.value().inner);
    REQUIRE(dv.has_init);
    REQUIRE(dv.is_parameter);
}

TEST_CASE("ElaborateTest - DefvarNameMustBeSymbol") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(defvar 1 2)", da, ca).has_value());
}

// -- block / return-from (step L14) -----------------------------------------

TEST_CASE("ElaborateTest - BlockElaboratesToCoreBlockWithName") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(block b 1 2 3)", da, ca);
    REQUIRE(er.has_value());
    using Block = lisp::elaborator::core_block<Core, MaxNodes, MaxList>;
    REQUIRE(std::holds_alternative<Block>(er.value().inner));
    auto const &blk = std::get<Block>(er.value().inner);
    REQUIRE(blk.name == "B");
    REQUIRE(blk.body.size() == 3);
}

TEST_CASE("ElaborateTest - BlockNameMustBeSymbol") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(block 1 2)", da, ca).has_value());
}

TEST_CASE("ElaborateTest - BlockWithNoFormsIsImplicitNilBody") {
    // ANSI CL: body is &rest, zero or more forms; zero forms evaluates to
    // nil (the evaluator's job) -- but the elaborated node still carries
    // exactly one (synthesized) body expression, matching core_lambda's and
    // core_progn's "at least one" invariant.
    DatumArena da;
    CoreArena ca;
    auto er = elab("(block b)", da, ca);
    REQUIRE(er.has_value());
    using Block = lisp::elaborator::core_block<Core, MaxNodes, MaxList>;
    auto const &blk = std::get<Block>(er.value().inner);
    REQUIRE(blk.body.size() == 1);
    REQUIRE(std::holds_alternative<lisp::elaborator::core_nil>(
        ca.get(blk.body[0]).inner));
}

TEST_CASE("ElaborateTest - ReturnFromElaboratesToCoreReturnFrom") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(block b (return-from b 42))", da, ca);
    REQUIRE(er.has_value());
    using Block = lisp::elaborator::core_block<Core, MaxNodes, MaxList>;
    auto const &blk = std::get<Block>(er.value().inner);
    using ReturnFrom = lisp::elaborator::core_return_from<Core, MaxNodes>;
    REQUIRE(std::holds_alternative<ReturnFrom>(ca.get(blk.body[0]).inner));
    auto const &rf = std::get<ReturnFrom>(ca.get(blk.body[0]).inner);
    REQUIRE(rf.name == "B");
    REQUIRE(
        std::get<lisp::elaborator::core_integer>(ca.get(rf.expr).inner).value ==
        42);
}

TEST_CASE("ElaborateTest - ReturnFromWithoutValueIsImplicitNil") {
    DatumArena da;
    CoreArena ca;
    auto er = elab("(block b (return-from b))", da, ca);
    REQUIRE(er.has_value());
    using Block = lisp::elaborator::core_block<Core, MaxNodes, MaxList>;
    auto const &blk = std::get<Block>(er.value().inner);
    using ReturnFrom = lisp::elaborator::core_return_from<Core, MaxNodes>;
    auto const &rf = std::get<ReturnFrom>(ca.get(blk.body[0]).inner);
    REQUIRE(std::holds_alternative<lisp::elaborator::core_nil>(
        ca.get(rf.expr).inner));
}

TEST_CASE("ElaborateTest - ReturnFromUndefinedBlockIsError") {
    // Decision D5: block-name resolution is purely lexical, done at
    // elaboration time -- an unresolvable name is a compile-time error, not
    // deferred to the evaluator.
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(return-from b 1)", da, ca).has_value());
    REQUIRE(!elab("(block a (return-from b 1))", da, ca).has_value());
}

TEST_CASE("ElaborateTest - ReturnFromSeesEnclosingBlockThroughNestedLambda") {
    // CL's block scope is an ordinary lexical scope: it is not reset by an
    // intervening lambda, which is exactly what makes the closure-smuggling
    // runtime diagnosis (eval_direct.test.cpp / cps_code.test.cpp) possible
    // in the first place -- this only pins the *elaboration-time* half:
    // the reference is legal here.
    DatumArena da;
    CoreArena ca;
    auto er = elab("(block b (lambda () (return-from b 1)))", da, ca);
    REQUIRE(er.has_value());
}

TEST_CASE("ElaborateTest - ReturnFromNameMustBeSymbol") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(block b (return-from 1 2))", da, ca).has_value());
}

TEST_CASE("ElaborateTest - ReturnFromWrongArityIsError") {
    DatumArena da;
    CoreArena ca;
    REQUIRE(!elab("(block b (return-from))", da, ca).has_value());
    REQUIRE(!elab("(block b (return-from b 1 2))", da, ca).has_value());
}

TEST_CASE("ElaborateTest - DefunBodyIsImplicitlyWrappedInBlockNamedForIt") {
    // ANSI CL: a defun body gets an implicit (block name ...), so
    // return-from to the function's own name is valid inside its body,
    // including inside a nested lambda (same lexical-scope point as
    // ReturnFromSeesEnclosingBlockThroughNestedLambda above).
    DatumArena da;
    CoreArena ca;
    auto er = elab("(defun f (x) (if x (return-from f 0) 1))", da, ca);
    REQUIRE(er.has_value());

    DatumArena da2;
    CoreArena ca2;
    auto er2 = elab("(defun f () (lambda () (return-from f 1)))", da2, ca2);
    REQUIRE(er2.has_value());
}
