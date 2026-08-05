// src/smd/cl/eval/value.test.cpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/eval/value.hpp>
#include <smd/cl/eval/value.hpp> // test 2nd include OK

#include <smd/cl/symbol/symbol_id.hpp>

#include <catch2/catch_test_macros.hpp>

#include <variant>

using smd::cl::eval::boolean_value;
using smd::cl::eval::closure_ref;
using smd::cl::eval::cons_ref;
using smd::cl::eval::is_false;
using smd::cl::eval::is_symbol;
using smd::cl::eval::nil_value;
using smd::cl::eval::standard_symbols;
using smd::cl::eval::string_ref;
using smd::cl::eval::symbol_value;
using smd::cl::eval::value;
using smd::cl::eval::value_character;
using smd::cl::eval::value_cons;
using smd::cl::eval::value_fixnum;
using smd::cl::eval::value_symbol;
using smd::cl::symbol::symbol_id;

namespace {

// Stand-in ids; the machine gets these from a real table.
constexpr standard_symbols known{symbol_id{0}, symbol_id{1}};
constexpr symbol_id other{2};

// ------------------------------------------------------------------
// Handles
// ------------------------------------------------------------------

constexpr auto default_handles_name_nothing() -> bool {
    return !cons_ref{}.valid() && !closure_ref{}.valid() &&
           cons_ref{0}.valid() && closure_ref{0}.valid() &&
           string_ref{} == string_ref{0, 0};
}

// Decision D14: nothing a running program passes around is parameterised on
// a capacity, so two values are comparable without agreeing on a heap size.
constexpr auto values_carry_no_capacity() -> bool {
    return sizeof(cons_ref) == sizeof(int) &&
           value{value_cons{cons_ref{3}}} == value{value_cons{cons_ref{3}}};
}

// ------------------------------------------------------------------
// Truth
// ------------------------------------------------------------------

// The whole of Common Lisp's rule: `nil` is false and everything else is
// true — including the fixnum 0, which is where Scheme's habits mislead.
constexpr auto only_nil_is_false() -> bool {
    return is_false(nil_value(known), known) &&
           !is_false(value{value_fixnum{0}}, known) &&
           !is_false(symbol_value(known.t), known) &&
           !is_false(symbol_value(other), known) &&
           !is_false(value{value_character{'\0'}}, known);
}

constexpr auto boolean_values_are_the_two_symbols() -> bool {
    return boolean_value(true, known) == symbol_value(known.t) &&
           boolean_value(false, known) == nil_value(known);
}

// `nil` is an ordinary symbol, as ANSI says: simultaneously false, the
// empty list, and something symbolp answers true for.
constexpr auto nil_is_a_symbol() -> bool {
    return std::holds_alternative<value_symbol>(nil_value(known)) &&
           is_symbol(nil_value(known), known.nil) &&
           !is_symbol(nil_value(known), known.t) &&
           !is_symbol(value{value_fixnum{0}}, known.nil);
}

} // namespace

static_assert(default_handles_name_nothing());
static_assert(values_carry_no_capacity());
static_assert(only_nil_is_false());
static_assert(boolean_values_are_the_two_symbols());
static_assert(nil_is_a_symbol());

TEST_CASE("ValueTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ValueTest - Handles") {
    CHECK(default_handles_name_nothing());
    CHECK(values_carry_no_capacity());
}

TEST_CASE("ValueTest - Truth") {
    CHECK(only_nil_is_false());
    CHECK(boolean_values_are_the_two_symbols());
    CHECK(nil_is_a_symbol());
}
