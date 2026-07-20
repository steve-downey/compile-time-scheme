// src/smd/smdlisp/macroexpand/expander.test.cpp                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/macroexpand/expander.hpp>
#include <smd/smdlisp/macroexpand/expander.hpp> // test 2nd include OK

#include <smd/smdlisp/closure/env.hpp>
#include <smd/smdlisp/closure/eval_direct.hpp>
#include <smd/smdlisp/closure/value.hpp>
#include <smd/smdlisp/reader/read_datum.hpp>
#include <smd/smdscheme/parser/cursor.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
namespace lisp = smd::smdlisp;
namespace scm = smd::smdscheme;
using namespace std::string_view_literals;

constexpr int MaxNodes = 256;
constexpr int MaxList = 16;
constexpr int MaxBindings = 16;
constexpr int MaxEnvs = 32;

using Datum = lisp::reader::datum_type<MaxNodes, MaxList>;
using DatumArena = scm::foundation::tree_arena<Datum, MaxNodes>;
using DatumList = lisp::reader::datum_list<Datum, MaxNodes, MaxList>;
using Core = lisp::elaborator::core_type<MaxNodes, MaxList>;
using CoreArena = scm::foundation::tree_arena<Core, MaxNodes>;
using Heap = lisp::closure::pair_heap<Core, lisp::closure::default_max_pairs>;
using Envs = lisp::closure::env_arena<Core, MaxBindings, MaxEnvs>;
using Val = lisp::closure::value<Core>;
using Res = scm::foundation::result<Val>;

/// Reads a single top-level call form (a list datum) from @p src, for
/// exercising one host macro's expansion function directly.
auto read_call(std::string_view src, DatumArena &arena) -> DatumList {
    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{src}, arena);
    REQUIRE(dr.has_value());
    REQUIRE(std::holds_alternative<DatumList>(dr.value().value.inner));
    return std::get<DatumList>(dr.value().value.inner);
}

/// Returns the folded spelling of @p d if it is a bare symbol datum, or an
/// empty view otherwise -- a small assertion helper for expansion-shape
/// checks below.
auto symbol_name(Datum const &d) -> std::string_view {
    if (!std::holds_alternative<lisp::reader::datum_symbol>(d.inner))
        return {};
    return std::get<lisp::reader::datum_symbol>(d.inner).name.view();
}

/// Reads, fully macro-expands, elaborates, and evaluates @p src under a
/// default environment sharing @p heap and @p envs. Mirrors
/// eval_direct.test.cpp's `run()` helper: @p root is caller-owned storage
/// for the elaborated root, since a returned `value<Core>` may view into it
/// (see that file's docs for the underlying lifetime rule, which applies
/// unchanged here -- nothing about macro expansion changes it).
constexpr auto run(std::string_view src, DatumArena &datum_arena,
                   CoreArena &core_arena, Core &root, Heap &heap, Envs &envs)
    -> Res {
    auto table = lisp::macroexpand::default_macro_table<MaxNodes, MaxList>();
    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{src}, datum_arena);
    if (!dr.has_value())
        return Res{dr.error()};
    auto er = lisp::macroexpand::expand_and_elaborate<MaxNodes, MaxList>(
        dr.value().value, datum_arena, core_arena, table);
    if (!er.has_value())
        return Res{er.error()};
    root = er.value();
    auto environment = lisp::closure::default_env<Core, MaxBindings>(heap);
    return lisp::closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
        root, core_arena, environment, envs);
}

} // namespace

TEST_CASE("ExpanderTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- macroexpand_1 / macroexpand plumbing ----------------------------------

TEST_CASE("ExpanderTest - MacroexpandOneLeavesNonMacroCallUnchanged") {
    DatumArena arena;
    auto table = lisp::macroexpand::default_macro_table<MaxNodes, MaxList>();
    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{"(foo 1 2)"sv}, arena);
    REQUIRE(dr.has_value());
    auto step_r = lisp::macroexpand::macroexpand_1<MaxNodes, MaxList>(
        dr.value().value, arena, table);
    REQUIRE(step_r.has_value());
    CHECK_FALSE(step_r.value().expanded);
}

TEST_CASE("ExpanderTest - MacroexpandOneFiresOnceForARegisteredMacro") {
    DatumArena arena;
    auto table = lisp::macroexpand::default_macro_table<MaxNodes, MaxList>();
    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{"(when t 1)"sv}, arena);
    REQUIRE(dr.has_value());
    auto step_r = lisp::macroexpand::macroexpand_1<MaxNodes, MaxList>(
        dr.value().value, arena, table);
    REQUIRE(step_r.has_value());
    CHECK(step_r.value().expanded);
    CHECK(symbol_name(arena.get(
              std::get<DatumList>(step_r.value().value.inner).elements[0])) ==
          "IF");
}

TEST_CASE("ExpanderTest - MacroexpandBudgetExhaustionIsDiagnosedError") {
    DatumArena arena;
    // A pathological host macro that always re-expands into a call of
    // itself: `macroexpand` must diagnose budget exhaustion rather than
    // loop forever (docs/cl-pivot-plan.md step L17's explicit requirement).
    lisp::macroexpand::macro_table<MaxNodes, MaxList> table{};
    table.push_back(lisp::macroexpand::host_macro<MaxNodes, MaxList>{
        "LOOP-FOREVER",
        [](lisp::macroexpand::call_form<MaxNodes, MaxList> const &call,
           lisp::macroexpand::arena_type<MaxNodes, MaxList> &a)
            -> scm::foundation::result<Datum> {
            (void)call;
            auto r = lisp::reader::read_datum<MaxNodes, MaxList>(
                scm::parser::cursor{"(loop-forever)"sv}, a);
            if (!r.has_value())
                return r.error();
            return r.value().value;
        }});

    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{"(loop-forever)"sv}, arena);
    REQUIRE(dr.has_value());
    auto r = lisp::macroexpand::macroexpand<MaxNodes, MaxList>(dr.value().value,
                                                               arena, table, 8);
    REQUIRE_FALSE(r.has_value());
}

// -- expansion shape: cond --------------------------------------------------

TEST_CASE("ExpanderTest - CondExpandsToNestedIf") {
    DatumArena arena;
    auto call = read_call("(cond (nil 1) (t 2))", arena);
    auto r = lisp::macroexpand::cond_expand<MaxNodes, MaxList>(call, arena);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<DatumList>(r.value().inner));
    auto const &outer_if = std::get<DatumList>(r.value().inner);
    REQUIRE(outer_if.elements.size() == 4);
    CHECK(symbol_name(arena.get(outer_if.elements[0])) == "IF");
    CHECK(symbol_name(arena.get(outer_if.elements[1])) == "NIL");
    auto const &then_progn =
        std::get<DatumList>(arena.get(outer_if.elements[2]).inner);
    CHECK(symbol_name(arena.get(then_progn.elements[0])) == "PROGN");
    auto const &rest_cond =
        std::get<DatumList>(arena.get(outer_if.elements[3]).inner);
    CHECK(symbol_name(arena.get(rest_cond.elements[0])) == "COND");
}

TEST_CASE("ExpanderTest - CondWithNoClausesExpandsToNil") {
    DatumArena arena;
    auto call = read_call("(cond)", arena);
    auto r = lisp::macroexpand::cond_expand<MaxNodes, MaxList>(call, arena);
    REQUIRE(r.has_value());
    CHECK(symbol_name(r.value()) == "NIL");
}

// -- expansion shape: when / unless ------------------------------------------

TEST_CASE("ExpanderTest - WhenExpandsToTwoArgIf") {
    DatumArena arena;
    auto call = read_call("(when x 1 2)", arena);
    auto r = lisp::macroexpand::when_expand<MaxNodes, MaxList>(call, arena);
    REQUIRE(r.has_value());
    auto const &if_form = std::get<DatumList>(r.value().inner);
    REQUIRE(if_form.elements.size() == 3);
    CHECK(symbol_name(arena.get(if_form.elements[0])) == "IF");
    CHECK(symbol_name(arena.get(if_form.elements[1])) == "X");
    auto const &progn =
        std::get<DatumList>(arena.get(if_form.elements[2]).inner);
    CHECK(symbol_name(arena.get(progn.elements[0])) == "PROGN");
    CHECK(progn.elements.size() == 3);
}

TEST_CASE("ExpanderTest - UnlessExpandsToThreeArgIfWithNilConsequent") {
    DatumArena arena;
    auto call = read_call("(unless x 1)", arena);
    auto r = lisp::macroexpand::unless_expand<MaxNodes, MaxList>(call, arena);
    REQUIRE(r.has_value());
    auto const &if_form = std::get<DatumList>(r.value().inner);
    REQUIRE(if_form.elements.size() == 4);
    CHECK(symbol_name(arena.get(if_form.elements[0])) == "IF");
    CHECK(symbol_name(arena.get(if_form.elements[2])) == "NIL");
}

// -- expansion shape: and / or ------------------------------------------------

TEST_CASE("ExpanderTest - AndWithNoArgsExpandsToT") {
    DatumArena arena;
    auto call = read_call("(and)", arena);
    auto r = lisp::macroexpand::and_expand<MaxNodes, MaxList>(call, arena);
    REQUIRE(r.has_value());
    CHECK(symbol_name(r.value()) == "T");
}

TEST_CASE("ExpanderTest - AndWithOneArgExpandsToThatArg") {
    DatumArena arena;
    auto call = read_call("(and x)", arena);
    auto r = lisp::macroexpand::and_expand<MaxNodes, MaxList>(call, arena);
    REQUIRE(r.has_value());
    CHECK(symbol_name(r.value()) == "X");
}

TEST_CASE("ExpanderTest - AndWithMultipleArgsExpandsToIfChainingAnd") {
    DatumArena arena;
    auto call = read_call("(and a b c)", arena);
    auto r = lisp::macroexpand::and_expand<MaxNodes, MaxList>(call, arena);
    REQUIRE(r.has_value());
    auto const &if_form = std::get<DatumList>(r.value().inner);
    REQUIRE(if_form.elements.size() == 4);
    CHECK(symbol_name(arena.get(if_form.elements[0])) == "IF");
    CHECK(symbol_name(arena.get(if_form.elements[1])) == "A");
    CHECK(symbol_name(arena.get(if_form.elements[3])) == "NIL");
    auto const &rest_and =
        std::get<DatumList>(arena.get(if_form.elements[2]).inner);
    CHECK(symbol_name(arena.get(rest_and.elements[0])) == "AND");
    REQUIRE(rest_and.elements.size() == 3);
    CHECK(symbol_name(arena.get(rest_and.elements[1])) == "B");
}

TEST_CASE("ExpanderTest - OrWithMultipleArgsExpandsToLetIf") {
    DatumArena arena;
    auto call = read_call("(or a b)", arena);
    auto r = lisp::macroexpand::or_expand<MaxNodes, MaxList>(call, arena);
    REQUIRE(r.has_value());
    auto const &let_form = std::get<DatumList>(r.value().inner);
    REQUIRE(let_form.elements.size() == 3);
    CHECK(symbol_name(arena.get(let_form.elements[0])) == "LET");
    auto const &bindings =
        std::get<DatumList>(arena.get(let_form.elements[1]).inner);
    REQUIRE(bindings.elements.size() == 1);
    auto const &pair =
        std::get<DatumList>(arena.get(bindings.elements[0]).inner);
    REQUIRE(pair.elements.size() == 2);
    CHECK(symbol_name(arena.get(pair.elements[1])) == "A");
    auto const &if_form =
        std::get<DatumList>(arena.get(let_form.elements[2]).inner);
    REQUIRE(if_form.elements.size() == 4);
    CHECK(symbol_name(arena.get(if_form.elements[0])) == "IF");
    auto const &rest_or =
        std::get<DatumList>(arena.get(if_form.elements[3]).inner);
    CHECK(symbol_name(arena.get(rest_or.elements[0])) == "OR");
}

// -- expansion shape: case ----------------------------------------------------

TEST_CASE("ExpanderTest - CaseExpandsToLetCond") {
    DatumArena arena;
    auto call = read_call("(case x (1 :one) (t :other))", arena);
    auto r = lisp::macroexpand::case_expand<MaxNodes, MaxList>(call, arena);
    REQUIRE(r.has_value());
    auto const &let_form = std::get<DatumList>(r.value().inner);
    REQUIRE(let_form.elements.size() == 3);
    CHECK(symbol_name(arena.get(let_form.elements[0])) == "LET");
    auto const &cond_form =
        std::get<DatumList>(arena.get(let_form.elements[2]).inner);
    CHECK(symbol_name(arena.get(cond_form.elements[0])) == "COND");
    REQUIRE(cond_form.elements.size() == 3); // COND + 2 clauses
    auto const &clause1 =
        std::get<DatumList>(arena.get(cond_form.elements[1]).inner);
    auto const &eql_call =
        std::get<DatumList>(arena.get(clause1.elements[0]).inner);
    CHECK(symbol_name(arena.get(eql_call.elements[0])) == "EQL");
    auto const &clause2 =
        std::get<DatumList>(arena.get(cond_form.elements[2]).inner);
    CHECK(symbol_name(arena.get(clause2.elements[0])) == "T");
}

// -- whole-tree expand: recursion and quote boundary -------------------------

TEST_CASE("ExpanderTest - ExpandRecursesIntoSubexpressions") {
    DatumArena arena;
    auto table = lisp::macroexpand::default_macro_table<MaxNodes, MaxList>();
    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{"(if (when t 1) 2 3)"sv}, arena);
    REQUIRE(dr.has_value());
    auto r = lisp::macroexpand::expand<MaxNodes, MaxList>(dr.value().value,
                                                          arena, table);
    REQUIRE(r.has_value());
    auto const &outer_if = std::get<DatumList>(r.value().inner);
    REQUIRE(outer_if.elements.size() == 4);
    // The nested `(when t 1)` must have been expanded away into an `if`.
    auto const &inner =
        std::get<DatumList>(arena.get(outer_if.elements[1]).inner);
    CHECK(symbol_name(arena.get(inner.elements[0])) == "IF");
}

TEST_CASE("ExpanderTest - ExpandDoesNotWalkIntoQuotedData") {
    DatumArena arena;
    auto table = lisp::macroexpand::default_macro_table<MaxNodes, MaxList>();
    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{"'(cond a b)"sv}, arena);
    REQUIRE(dr.has_value());
    REQUIRE(std::holds_alternative<lisp::reader::datum_quote<Datum, MaxNodes>>(
        dr.value().value.inner));
    auto r = lisp::macroexpand::expand<MaxNodes, MaxList>(dr.value().value,
                                                          arena, table);
    REQUIRE(r.has_value());
    // A datum_quote node itself passes through expand() untouched; the
    // quoted data underneath must still be the literal 3-element list.
    REQUIRE(std::holds_alternative<lisp::reader::datum_quote<Datum, MaxNodes>>(
        r.value().inner));
    auto const &q =
        std::get<lisp::reader::datum_quote<Datum, MaxNodes>>(r.value().inner);
    auto const &lst = std::get<DatumList>(arena.get(q.quoted).inner);
    REQUIRE(lst.elements.size() == 3);
    CHECK(symbol_name(arena.get(lst.elements[0])) == "COND");
}

// -- merge criterion (docs/cl-pivot-plan.md, step L17) -----------------------

static_assert([] {
    DatumArena da;
    CoreArena ca;
    Core root;
    Heap heap;
    Envs envs;
    auto vr = run("(cond (nil 1) (t 2))", da, ca, root, heap, envs);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 2;
}());

TEST_CASE("ExpanderTest - CondEndToEnd") {
    DatumArena da;
    CoreArena ca;
    Core root;
    Heap heap;
    Envs envs;
    auto vr = run("(cond (nil 1) (t 2))", da, ca, root, heap, envs);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    CHECK(std::get<int>(vr.value()) == 2);
}

TEST_CASE("ExpanderTest - WhenEndToEnd") {
    DatumArena da;
    CoreArena ca;
    Core root;
    Heap heap;
    Envs envs;
    auto vr = run("(when t 1 2)", da, ca, root, heap, envs);
    REQUIRE(vr.has_value());
    CHECK(std::get<int>(vr.value()) == 2);
}

TEST_CASE("ExpanderTest - UnlessEndToEnd") {
    DatumArena da;
    CoreArena ca;
    Core root;
    Heap heap;
    Envs envs;
    auto vr = run("(unless nil 1 2)", da, ca, root, heap, envs);
    REQUIRE(vr.has_value());
    CHECK(std::get<int>(vr.value()) == 2);
}

TEST_CASE("ExpanderTest - AndEndToEnd") {
    DatumArena da;
    CoreArena ca;
    Core root;
    Heap heap;
    Envs envs;
    auto vr = run("(and 1 2 nil)", da, ca, root, heap, envs);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::nil_t>(vr.value()));
}

TEST_CASE("ExpanderTest - OrEndToEnd") {
    DatumArena da;
    CoreArena ca;
    Core root;
    Heap heap;
    Envs envs;
    auto vr = run("(or nil nil 3)", da, ca, root, heap, envs);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    CHECK(std::get<int>(vr.value()) == 3);
}

TEST_CASE("ExpanderTest - CaseEndToEnd") {
    DatumArena da;
    CoreArena ca;
    Core root;
    Heap heap;
    Envs envs;
    auto vr =
        run("(case 2 (1 :one) (2 :two) (t :other))", da, ca, root, heap, envs);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::keyword>(vr.value()));
    CHECK(std::get<lisp::closure::keyword>(vr.value()).name == "TWO");
}

TEST_CASE("ExpanderTest - CaseFallsThroughToOtherwise") {
    DatumArena da;
    CoreArena ca;
    Core root;
    Heap heap;
    Envs envs;
    auto vr =
        run("(case 99 (1 :one) (2 :two) (t :other))", da, ca, root, heap, envs);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::keyword>(vr.value()));
    CHECK(std::get<lisp::closure::keyword>(vr.value()).name == "OTHER");
}
