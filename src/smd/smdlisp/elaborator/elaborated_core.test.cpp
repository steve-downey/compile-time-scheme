// src/smd/smdlisp/elaborator/elaborated_core.test.cpp             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/elaborator/elaborated_core.hpp>
#include <smd/smdlisp/elaborator/elaborated_core.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <variant>

namespace {
namespace elab = smd::smdlisp::elaborator;
using Core = elab::core_type<16, 8>;

} // namespace

TEST_CASE("ElaboratedCoreTest - HeaderIsIdempotent") { REQUIRE(true); }

// Architecture invariant (see handoff.md / CLAUDE.md "Architecture facts"):
// core tree nodes must never embed value/closure types and must stay
// trivially destructible, so a whole elaborated program can be
// constant-evaluated out to global (compiled_lisp-style) storage without
// hitting constexpr constructor/destructor boundary errors.
static_assert(std::is_trivially_destructible_v<Core>);

TEST_CASE("ElaboratedCoreTest - IntegerNodeHoldsValue") {
    Core c{elab::core_f_factory<16, 8>::type<Core>{elab::core_integer{42}}};
    REQUIRE(std::holds_alternative<elab::core_integer>(c.inner));
    REQUIRE(std::get<elab::core_integer>(c.inner).value == 42);
}

TEST_CASE("ElaboratedCoreTest - NilAndTrueAreDistinctFromSymbol") {
    // Decision D3: NIL/T are self-evaluating constants, never VARIABLE
    // -namespace lookups, so they get their own core kinds distinct from
    // core_symbol.
    Core nil_node{elab::core_f_factory<16, 8>::type<Core>{elab::core_nil{}}};
    Core true_node{elab::core_f_factory<16, 8>::type<Core>{elab::core_true{}}};
    Core sym_node{elab::core_f_factory<16, 8>::type<Core>{
        elab::core_symbol{smd::smdlisp::reader::detail::fold("X")}}};
    REQUIRE(std::holds_alternative<elab::core_nil>(nil_node.inner));
    REQUIRE(std::holds_alternative<elab::core_true>(true_node.inner));
    REQUIRE(std::holds_alternative<elab::core_symbol>(sym_node.inner));
    REQUIRE_FALSE(std::holds_alternative<elab::core_symbol>(nil_node.inner));
    REQUIRE_FALSE(std::holds_alternative<elab::core_symbol>(true_node.inner));
}

TEST_CASE("ElaboratedCoreTest - QuoteAtomReusesSymbolAndKeywordShapes") {
    // core_symbol/core_keyword are reused, unchanged, as core_quote's atom
    // payload; this pins that the reuse compiles and is distinguishable.
    elab::core_quote q_sym{
        elab::core_symbol{smd::smdlisp::reader::detail::fold("FOO")}};
    elab::core_quote q_kw{
        elab::core_keyword{smd::smdlisp::reader::detail::fold("BAR")}};
    elab::core_quote q_int{7};
    REQUIRE(std::holds_alternative<elab::core_symbol>(q_sym.atom));
    REQUIRE(std::holds_alternative<elab::core_keyword>(q_kw.atom));
    REQUIRE(std::holds_alternative<int>(q_int.atom));
    REQUIRE(std::get<elab::core_symbol>(q_sym.atom).name.view() == "FOO");
    REQUIRE(std::get<elab::core_keyword>(q_kw.atom).name.view() == "BAR");
    REQUIRE(std::get<int>(q_int.atom) == 7);
}

TEST_CASE("ElaboratedCoreTest - ApplicationFuncIsAlwaysArenaBox") {
    // core_application.func always refers to a core_function node
    // (decision D4); this just pins that the field is a plain handle, not
    // a bare symbol/expression, at the type level.
    static_assert(
        std::is_same_v<decltype(elab::core_application<Core, 16, 8>{}.func),
                       smd::smdscheme::foundation::arena_box<Core, 16>>);
}
