// src/smd/smdscheme/sender/comp_tree.test.cpp                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/comp_tree.hpp>
#include <smd/smdscheme/sender/comp_tree.hpp> // test 2nd include OK

#include <smd/smdscheme/elaborator/elaborate.hpp>
#include <smd/smdscheme/reader/read_datum.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
using namespace smd::smdscheme;
using namespace std::string_view_literals;

constexpr int MaxNodes = 32;
constexpr int MaxList = 16;

using Core = elaborator::core_type<MaxNodes, MaxList>;
using CompT = fixpoint_eval::Comp<MaxList>;
using ResComp = foundation::result<CompT>;

auto to_comp(std::string_view src) -> ResComp {
    // Read phase
    foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>, MaxNodes>
        arena_dr;
    auto dr =
        reader::read_datum<MaxNodes, MaxList>(parser::cursor{src}, arena_dr);
    if (!dr.has_value())
        return ResComp{dr.error()};

    // Elaborate phase
    foundation::tree_arena<Core, MaxNodes> core_arena;
    auto er = elaborator::elaborate<MaxNodes, MaxList>(dr.value().value,
                                                       arena_dr, core_arena);
    if (!er.has_value())
        return ResComp{er.error()};

    // Conversion phase
    return fixpoint_eval::core_to_comp<MaxNodes, MaxList>(er.value(),
                                                          core_arena);
}

} // namespace

TEST_CASE("CompTreeTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("CompTreeTest - IntegerLiteralIsCompPure") {
    auto comp_r = to_comp("42");
    REQUIRE(comp_r.has_value());

    auto const &layer = smd::fixpoint::unwrap_fix(comp_r.value());
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_pure>(layer));

    auto const &p = std::get<fixpoint_eval::comp_pure>(layer);
    REQUIRE(std::holds_alternative<int>(p.atom));
    REQUIRE(std::get<int>(p.atom) == 42);
}

TEST_CASE("CompTreeTest - BooleanTrueIsCompPure") {
    auto comp_r = to_comp("#t");
    REQUIRE(comp_r.has_value());

    auto const &layer = smd::fixpoint::unwrap_fix(comp_r.value());
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_pure>(layer));

    auto const &p = std::get<fixpoint_eval::comp_pure>(layer);
    REQUIRE(std::holds_alternative<bool>(p.atom));
    REQUIRE(std::get<bool>(p.atom) == true);
}

TEST_CASE("CompTreeTest - BooleanFalseIsCompPure") {
    auto comp_r = to_comp("#f");
    REQUIRE(comp_r.has_value());

    auto const &layer = smd::fixpoint::unwrap_fix(comp_r.value());
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_pure>(layer));

    auto const &p = std::get<fixpoint_eval::comp_pure>(layer);
    REQUIRE(std::holds_alternative<bool>(p.atom));
    REQUIRE(std::get<bool>(p.atom) == false);
}

TEST_CASE("CompTreeTest - SymbolIsCompLookup") {
    auto comp_r = to_comp("x");
    REQUIRE(comp_r.has_value());

    auto const &layer = smd::fixpoint::unwrap_fix(comp_r.value());
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_lookup>(layer));

    auto const &l = std::get<fixpoint_eval::comp_lookup>(layer);
    REQUIRE(l.name == "x");
}

TEST_CASE("CompTreeTest - QuoteAtomIsCompPure") {
    auto comp_r = to_comp("(quote abc)");
    REQUIRE(comp_r.has_value());

    auto const &layer = smd::fixpoint::unwrap_fix(comp_r.value());
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_pure>(layer));

    auto const &p = std::get<fixpoint_eval::comp_pure>(layer);
    REQUIRE(std::holds_alternative<std::string_view>(p.atom));
    REQUIRE(std::get<std::string_view>(p.atom) == "abc");
}

TEST_CASE("CompTreeTest - IfNodeStructure") {
    auto comp_r = to_comp("(if #t 1 2)");
    REQUIRE(comp_r.has_value());

    auto const &layer = smd::fixpoint::unwrap_fix(comp_r.value());
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_if<CompT>>(layer));

    auto const &ci = std::get<fixpoint_eval::comp_if<CompT>>(layer);

    // Check condition is boolean literal
    auto const &cond_layer = smd::fixpoint::unwrap_fix(*ci.cond);
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_pure>(cond_layer));

    // Check consequent is integer literal
    auto const &cons_layer = smd::fixpoint::unwrap_fix(*ci.cons);
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_pure>(cons_layer));

    // Check alternative is integer literal
    auto const &alt_layer = smd::fixpoint::unwrap_fix(*ci.alt);
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_pure>(alt_layer));
}

TEST_CASE("CompTreeTest - LambdaNodeStructure") {
    auto comp_r = to_comp("(lambda (x) x)");
    REQUIRE(comp_r.has_value());

    auto const &layer = smd::fixpoint::unwrap_fix(comp_r.value());
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_lambda<CompT, MaxList>>(
        layer));

    auto const &lam =
        std::get<fixpoint_eval::comp_lambda<CompT, MaxList>>(layer);
    REQUIRE(lam.params.size() == 1);
    REQUIRE(lam.params[0] == "x");

    // Body should be a symbol lookup
    auto const &body_layer = smd::fixpoint::unwrap_fix(*lam.body);
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_lookup>(body_layer));
    REQUIRE(std::get<fixpoint_eval::comp_lookup>(body_layer).name == "x");
}

TEST_CASE("CompTreeTest - ApplicationNodeStructure") {
    auto comp_r = to_comp("(+ 1 2)");
    REQUIRE(comp_r.has_value());

    auto const &layer = smd::fixpoint::unwrap_fix(comp_r.value());
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_apply<CompT, MaxList>>(
        layer));

    auto const &app =
        std::get<fixpoint_eval::comp_apply<CompT, MaxList>>(layer);
    REQUIRE(app.args.size() == 2);

    // func should be a symbol lookup for "+"
    auto const &func_layer = smd::fixpoint::unwrap_fix(*app.func);
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_lookup>(func_layer));
    REQUIRE(std::get<fixpoint_eval::comp_lookup>(func_layer).name == "+");

    // args should be integer literals
    auto const &arg0_layer = smd::fixpoint::unwrap_fix(*app.args[0]);
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_pure>(arg0_layer));

    auto const &arg1_layer = smd::fixpoint::unwrap_fix(*app.args[1]);
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_pure>(arg1_layer));
}

TEST_CASE("CompTreeTest - NestedApplicationStructure") {
    auto comp_r = to_comp("(+ 1 (* 2 3))");
    REQUIRE(comp_r.has_value());

    auto const &layer = smd::fixpoint::unwrap_fix(comp_r.value());
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_apply<CompT, MaxList>>(
        layer));

    auto const &app =
        std::get<fixpoint_eval::comp_apply<CompT, MaxList>>(layer);
    REQUIRE(app.args.size() == 2);

    // First arg is integer literal
    auto const &arg0_layer = smd::fixpoint::unwrap_fix(*app.args[0]);
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_pure>(arg0_layer));

    // Second arg is an application
    auto const &arg1_layer = smd::fixpoint::unwrap_fix(*app.args[1]);
    REQUIRE(std::holds_alternative<fixpoint_eval::comp_apply<CompT, MaxList>>(
        arg1_layer));

    auto const &inner_app =
        std::get<fixpoint_eval::comp_apply<CompT, MaxList>>(arg1_layer);
    REQUIRE(inner_app.args.size() == 2);
}
