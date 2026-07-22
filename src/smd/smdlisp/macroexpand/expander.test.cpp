// src/smd/smdlisp/macroexpand/expander.test.cpp                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/macroexpand/expander.hpp>
#include <smd/smdlisp/macroexpand/expander.hpp> // test 2nd include OK

#include <smd/smdlisp/closure/eval_direct.hpp>
#include <smd/smdlisp/reader/read_datum.hpp>
#include <smd/smdscheme/parser/cursor.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
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

using Datum = lisp::reader::datum_type<MaxNodes, MaxList>;
using DatumArena = scm::foundation::tree_arena<Datum, MaxNodes>;
using DatumList = lisp::reader::datum_list<Datum, MaxNodes, MaxList>;
using Core = lisp::elaborator::core_type<MaxNodes, MaxList>;
using CoreArena = scm::foundation::tree_arena<Core, MaxNodes>;
using Val = lisp::closure::value<Core>;
using Res = scm::foundation::result<Val>;
using Heap = lisp::closure::pair_heap<Core, lisp::closure::default_max_pairs>;
using Envs = lisp::closure::env_arena<Core, MaxBindings, MaxEnvs>;

/// Reads @p src into a fresh datum tree in @p arena.
auto read(std::string_view src, DatumArena &arena) -> Datum {
    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{src}, arena);
    REQUIRE(dr.has_value());
    return dr.value().value;
}

/// Returns the head symbol's folded spelling of a list-shaped datum, or an
/// empty view if @p d is not a list headed by a symbol.
auto head_name(Datum const &d, DatumArena const &arena) -> std::string_view {
    if (!std::holds_alternative<DatumList>(d.inner))
        return {};
    auto const &lst = std::get<DatumList>(d.inner);
    if (lst.elements.empty())
        return {};
    auto const &head = arena.get(lst.elements[0]);
    if (!std::holds_alternative<lisp::reader::datum_symbol>(head.inner))
        return {};
    return std::get<lisp::reader::datum_symbol>(head.inner).name.view();
}

/// Reads, expands, elaborates, and directly evaluates @p src. Mirrors
/// eval_direct.test.cpp's `run()` helper: arenas and the elaborated root
/// are all caller-owned reference parameters, never function-locals
/// returned past (see handoff.md's L10/L11 stack-use-after-return lessons).
auto run(std::string_view src, DatumArena &datum_arena, CoreArena &core_arena,
         Core &root, Heap &heap, Envs &envs) -> Res {
    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{src}, datum_arena);
    if (!dr.has_value())
        return Res{dr.error()};
    auto er = lisp::macroexpand::expand_and_elaborate<MaxNodes, MaxList>(
        dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return Res{er.error()};
    root = er.value();
    auto environment = lisp::closure::default_env<Core, MaxBindings>(heap);
    return lisp::closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
        root, core_arena, environment, envs);
}

/// A deliberately non-terminating host macro (expands a call back into an
/// identical call, forever) used only to exercise @ref
/// lisp::macroexpand::macroexpand's budget-exhaustion diagnostic -- none of
/// the six real macros in @ref lisp::macroexpand::default_macro_table ever
/// loop, so this is the only way to drive that path.
auto expand_loop_forever(DatumList const &call, DatumArena &arena)
    -> scm::foundation::result<Datum> {
    (void)arena;
    using DatumF =
        typename lisp::reader::datum_f_factory<MaxNodes,
                                               MaxList>::template type<Datum>;
    return Datum{DatumF{call}};
}

} // namespace

TEST_CASE("ExpanderTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- expansion-shape tests (docs/cl-pivot-plan.md step L17) ------------------

TEST_CASE("ExpanderTest - WhenExpandsToIfProgn") {
    DatumArena arena;
    auto d = read("(when x 1 2)", arena);
    auto step = lisp::macroexpand::macroexpand_1<MaxNodes, MaxList>(d, arena);
    REQUIRE(step.has_value());
    REQUIRE(step.value().expanded);
    auto const &expanded = step.value().node;
    REQUIRE(head_name(expanded, arena) == "IF");
    auto const &if_list = std::get<DatumList>(expanded.inner);
    REQUIRE(if_list.elements.size() == 4);
    REQUIRE(head_name(arena.get(if_list.elements[2]), arena) == "PROGN");
    REQUIRE(head_name(arena.get(if_list.elements[3]), arena).empty());
    auto const &alt = arena.get(if_list.elements[3]);
    REQUIRE(std::holds_alternative<lisp::reader::datum_symbol>(alt.inner));
    REQUIRE(std::get<lisp::reader::datum_symbol>(alt.inner).name.view() ==
            "NIL");
}

TEST_CASE("ExpanderTest - UnlessExpandsToIfProgn") {
    DatumArena arena;
    auto d = read("(unless x 1)", arena);
    auto step = lisp::macroexpand::macroexpand_1<MaxNodes, MaxList>(d, arena);
    REQUIRE(step.has_value());
    REQUIRE(step.value().expanded);
    auto const &expanded = step.value().node;
    REQUIRE(head_name(expanded, arena) == "IF");
    auto const &if_list = std::get<DatumList>(expanded.inner);
    REQUIRE(if_list.elements.size() == 4);
    auto const &cons = arena.get(if_list.elements[2]);
    REQUIRE(std::holds_alternative<lisp::reader::datum_symbol>(cons.inner));
    REQUIRE(std::get<lisp::reader::datum_symbol>(cons.inner).name.view() ==
            "NIL");
    REQUIRE(head_name(arena.get(if_list.elements[3]), arena) == "PROGN");
}

TEST_CASE("ExpanderTest - AndWithZeroArgsIsT") {
    DatumArena arena;
    auto d = read("(and)", arena);
    auto step = lisp::macroexpand::macroexpand_1<MaxNodes, MaxList>(d, arena);
    REQUIRE(step.has_value());
    REQUIRE(step.value().expanded);
    auto const &expanded = step.value().node;
    REQUIRE(std::holds_alternative<lisp::reader::datum_symbol>(expanded.inner));
    REQUIRE(std::get<lisp::reader::datum_symbol>(expanded.inner).name.view() ==
            "T");
}

TEST_CASE("ExpanderTest - AndWithOneArgIsThatArg") {
    DatumArena arena;
    auto d = read("(and x)", arena);
    auto step = lisp::macroexpand::macroexpand_1<MaxNodes, MaxList>(d, arena);
    REQUIRE(step.has_value());
    REQUIRE(step.value().expanded);
    auto const &expanded = step.value().node;
    REQUIRE(std::holds_alternative<lisp::reader::datum_symbol>(expanded.inner));
    REQUIRE(std::get<lisp::reader::datum_symbol>(expanded.inner).name.view() ==
            "X");
}

TEST_CASE("ExpanderTest - AndWithManyArgsNestsIf") {
    DatumArena arena;
    auto d = read("(and a b c)", arena);
    auto step = lisp::macroexpand::macroexpand_1<MaxNodes, MaxList>(d, arena);
    REQUIRE(step.has_value());
    REQUIRE(step.value().expanded);
    auto const &outer = step.value().node;
    REQUIRE(head_name(outer, arena) == "IF");
    auto const &outer_list = std::get<DatumList>(outer.inner);
    REQUIRE(outer_list.elements.size() == 4);
    auto const &inner = arena.get(outer_list.elements[2]);
    REQUIRE(head_name(inner, arena) == "IF");
}

TEST_CASE("ExpanderTest - OrWithZeroArgsIsNil") {
    DatumArena arena;
    auto d = read("(or)", arena);
    auto step = lisp::macroexpand::macroexpand_1<MaxNodes, MaxList>(d, arena);
    REQUIRE(step.has_value());
    REQUIRE(step.value().expanded);
    auto const &expanded = step.value().node;
    REQUIRE(std::holds_alternative<lisp::reader::datum_symbol>(expanded.inner));
    REQUIRE(std::get<lisp::reader::datum_symbol>(expanded.inner).name.view() ==
            "NIL");
}

TEST_CASE("ExpanderTest - OrWithManyArgsExpandsToLetIf") {
    DatumArena arena;
    auto d = read("(or a b)", arena);
    auto step = lisp::macroexpand::macroexpand_1<MaxNodes, MaxList>(d, arena);
    REQUIRE(step.has_value());
    REQUIRE(step.value().expanded);
    auto const &expanded = step.value().node;
    REQUIRE(head_name(expanded, arena) == "LET");
}

TEST_CASE("ExpanderTest - CondExpandsToNestedIf") {
    DatumArena arena;
    auto d = read("(cond (a 1) (b 2))", arena);
    auto step = lisp::macroexpand::macroexpand_1<MaxNodes, MaxList>(d, arena);
    REQUIRE(step.has_value());
    REQUIRE(step.value().expanded);
    auto const &outer = step.value().node;
    REQUIRE(head_name(outer, arena) == "IF");
    auto const &outer_list = std::get<DatumList>(outer.inner);
    REQUIRE(head_name(arena.get(outer_list.elements[2]), arena) == "PROGN");
    auto const &alt = arena.get(outer_list.elements[3]);
    REQUIRE(head_name(alt, arena) == "IF");
}

TEST_CASE("ExpanderTest - CondWithNoClausesIsNil") {
    DatumArena arena;
    auto d = read("(cond)", arena);
    auto step = lisp::macroexpand::macroexpand_1<MaxNodes, MaxList>(d, arena);
    REQUIRE(step.has_value());
    REQUIRE(step.value().expanded);
    auto const &expanded = step.value().node;
    REQUIRE(std::holds_alternative<lisp::reader::datum_symbol>(expanded.inner));
    REQUIRE(std::get<lisp::reader::datum_symbol>(expanded.inner).name.view() ==
            "NIL");
}

TEST_CASE("ExpanderTest - CondTestOnlyClauseExpandsToLet") {
    DatumArena arena;
    auto d = read("(cond (a))", arena);
    auto step = lisp::macroexpand::macroexpand_1<MaxNodes, MaxList>(d, arena);
    REQUIRE(step.has_value());
    REQUIRE(step.value().expanded);
    REQUIRE(head_name(step.value().node, arena) == "LET");
}

TEST_CASE("ExpanderTest - CaseExpandsToLetCond") {
    DatumArena arena;
    auto d = read("(case x (1 'one) (t 'other))", arena);
    auto step = lisp::macroexpand::macroexpand_1<MaxNodes, MaxList>(d, arena);
    REQUIRE(step.has_value());
    REQUIRE(step.value().expanded);
    auto const &expanded = step.value().node;
    REQUIRE(head_name(expanded, arena) == "LET");
    auto const &let_list = std::get<DatumList>(expanded.inner);
    REQUIRE(let_list.elements.size() == 3);
    REQUIRE(head_name(arena.get(let_list.elements[2]), arena) == "COND");
}

TEST_CASE("ExpanderTest - CaseMultiKeyClauseUsesOr") {
    DatumArena arena;
    auto d = read("(case x ((2 3) 'two-or-three))", arena);
    auto step = lisp::macroexpand::macroexpand_1<MaxNodes, MaxList>(d, arena);
    REQUIRE(step.has_value());
    REQUIRE(step.value().expanded);
    auto const &let_list = std::get<DatumList>(step.value().node.inner);
    auto const &cond_form = arena.get(let_list.elements[2]);
    auto const &cond_list = std::get<DatumList>(cond_form.inner);
    // cond_list.elements[0] is the COND symbol, [1] is the one clause.
    auto const &clause = arena.get(cond_list.elements[1]);
    auto const &clause_list = std::get<DatumList>(clause.inner);
    REQUIRE(head_name(arena.get(clause_list.elements[0]), arena) == "OR");
}

TEST_CASE("ExpanderTest - CaseOtherwiseClauseMustBeLast") {
    DatumArena arena;
    auto d = read("(case x (otherwise 'default) (1 'one))", arena);
    auto step = lisp::macroexpand::macroexpand_1<MaxNodes, MaxList>(d, arena);
    REQUIRE(!step.has_value());
}

// -- macroexpand budget (docs/cl-pivot-plan.md step L17) ---------------------

TEST_CASE("ExpanderTest - MacroexpandBudgetExceededIsDiagnosedError") {
    DatumArena arena;
    auto d = read("(loop-forever)", arena);
    std::array<lisp::macroexpand::host_macro<MaxNodes, MaxList>, 1> table{
        {{"LOOP-FOREVER", &expand_loop_forever}}};
    auto r =
        lisp::macroexpand::macroexpand<MaxNodes, MaxList>(d, arena, table,
                                                          /*max_expansions=*/3);
    REQUIRE(!r.has_value());
}

TEST_CASE("ExpanderTest - MacroexpandStopsAtFixpointForNonLoopingMacro") {
    DatumArena arena;
    auto d = read("(when x 1)", arena);
    auto r = lisp::macroexpand::macroexpand<MaxNodes, MaxList>(d, arena);
    REQUIRE(r.has_value());
    REQUIRE(head_name(r.value(), arena) == "IF");
}

// -- quoted data is never macro-expanded -------------------------------------

TEST_CASE("ExpanderTest - QuoteShorthandSuppressesExpansion") {
    DatumArena arena;
    auto d = read("'(when x 1)", arena);
    auto r = lisp::macroexpand::expand_datum<MaxNodes, MaxList>(d, arena);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::reader::datum_quote<Datum, MaxNodes>>(
        r.value().inner));
    auto const &q =
        std::get<lisp::reader::datum_quote<Datum, MaxNodes>>(r.value().inner);
    REQUIRE(head_name(arena.get(q.quoted), arena) == "WHEN");
}

TEST_CASE("ExpanderTest - QuoteListFormSuppressesExpansion") {
    DatumArena arena;
    auto d = read("(quote (when x 1))", arena);
    auto r = lisp::macroexpand::expand_datum<MaxNodes, MaxList>(d, arena);
    REQUIRE(r.has_value());
    REQUIRE(head_name(r.value(), arena) == "QUOTE");
    auto const &lst = std::get<DatumList>(r.value().inner);
    REQUIRE(head_name(arena.get(lst.elements[1]), arena) == "WHEN");
}

// -- recursive expansion reaches nested positions ----------------------------

TEST_CASE("ExpanderTest - ExpansionReachesLambdaBody") {
    DatumArena arena;
    auto d = read("(lambda (x) (when x 1))", arena);
    auto r = lisp::macroexpand::expand_datum<MaxNodes, MaxList>(d, arena);
    REQUIRE(r.has_value());
    auto const &lst = std::get<DatumList>(r.value().inner);
    // (LAMBDA (X) body) -- body is elements[2].
    REQUIRE(head_name(arena.get(lst.elements[2]), arena) == "IF");
}

// -- end-to-end merge criterion (docs/cl-pivot-plan.md step L17) -------------
// (cond (nil 1) (t 2)) => 2, using L11's existing nil/t truthiness and if
// semantics, with no evaluator or elaborator changes.

static_assert([] {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Envs envs;

    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{"(cond (nil 1) (t 2))"sv}, da);
    if (!dr.has_value())
        return false;
    auto er = lisp::macroexpand::expand_and_elaborate<MaxNodes, MaxList>(
        dr.value().value, da, ca);
    if (!er.has_value())
        return false;
    auto environment = lisp::closure::default_env<Core, MaxBindings>(heap);
    auto vr =
        lisp::closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            er.value(), ca, environment, envs);
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
    REQUIRE(std::get<int>(vr.value()) == 2);
}

TEST_CASE("ExpanderTest - WhenEndToEnd") {
    DatumArena da;
    CoreArena ca;
    Core root;
    Heap heap;
    Envs envs;
    auto vr = run("(when t 1 2)", da, ca, root, heap, envs);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 2);
}

TEST_CASE("ExpanderTest - UnlessEndToEnd") {
    DatumArena da;
    CoreArena ca;
    Core root;
    Heap heap;
    Envs envs;
    auto vr = run("(unless nil 5)", da, ca, root, heap, envs);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 5);
}

TEST_CASE("ExpanderTest - AndEndToEnd") {
    DatumArena da;
    CoreArena ca;
    Core root;
    Heap heap;
    Envs envs;
    auto vr = run("(and 1 2 3)", da, ca, root, heap, envs);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 3);
}

TEST_CASE("ExpanderTest - AndShortCircuitsOnNil") {
    DatumArena da;
    CoreArena ca;
    Core root;
    Heap heap;
    Envs envs;
    auto vr = run("(and nil undefined-var)", da, ca, root, heap, envs);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::nil_t>(vr.value()));
}

TEST_CASE("ExpanderTest - OrEndToEndSingleEvaluation") {
    DatumArena da;
    CoreArena ca;
    Core root;
    Heap heap;
    Envs envs;
    auto vr = run("(or nil 7)", da, ca, root, heap, envs);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 7);
}

TEST_CASE("ExpanderTest - CaseEndToEnd") {
    DatumArena da;
    CoreArena ca;
    Core root;
    Heap heap;
    Envs envs;
    auto vr =
        run("(case 2 (1 100) ((2 3) 200) (t 300))", da, ca, root, heap, envs);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 200);
}

TEST_CASE("ExpanderTest - CaseEndToEndDefaultClause") {
    DatumArena da;
    CoreArena ca;
    Core root;
    Heap heap;
    Envs envs;
    auto vr = run("(case 99 (1 100) (t 300))", da, ca, root, heap, envs);
    REQUIRE(vr.has_value());
    REQUIRE(std::holds_alternative<int>(vr.value()));
    REQUIRE(std::get<int>(vr.value()) == 300);
}
