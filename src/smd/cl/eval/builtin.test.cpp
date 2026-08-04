// src/smd/cl/eval/builtin.test.cpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/eval/builtin.hpp>
#include <smd/cl/eval/builtin.hpp> // test 2nd include OK

#include <smd/cl/eval/heap.hpp>
#include <smd/cl/eval/outcome.hpp>
#include <smd/cl/eval/value.hpp>
#include <smd/cl/foundation/static_vector.hpp>
#include <smd/cl/symbol/symbol_id.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <ranges>
#include <variant>

using smd::cl::eval::apply_builtin;
using smd::cl::eval::builtin_op;
using smd::cl::eval::builtins;
using smd::cl::eval::function_binding;
using smd::cl::eval::heap;
using smd::cl::eval::install_builtins;
using smd::cl::eval::outcome;
using smd::cl::eval::standard_symbols;
using smd::cl::eval::symbol_table;
using smd::cl::eval::value;
using smd::cl::eval::value_fixnum;
using smd::cl::foundation::static_vector;
using smd::cl::symbol::symbol_id;

namespace {

using store_type = heap<64, 16, 2>;
using args_type = static_vector<value, 8>;
using table_type = symbol_table<64, 512>;

constexpr standard_symbols known{symbol_id{0}, symbol_id{1}};

constexpr auto nil() -> value { return smd::cl::eval::nil_value(known); }
constexpr auto truth() -> value { return smd::cl::eval::symbol_value(known.t); }
constexpr auto number(int n) -> value { return value{value_fixnum{n}}; }

constexpr auto args_of(std::initializer_list<value> vs) -> args_type {
    args_type xs;
    std::ranges::for_each(vs, [&xs](value const &v) { xs.push_back(v); });
    return xs;
}

// Applies @p op to @p vs on a throwaway heap.
constexpr auto applied(builtin_op op, std::initializer_list<value> vs)
    -> outcome {
    store_type store;
    return apply_builtin(op, args_of(vs), store, known);
}

constexpr auto yields(builtin_op op, std::initializer_list<value> vs,
                      value const &expected) -> bool {
    auto const result = applied(op, vs);
    return result.is_value() && result.as_value() == expected;
}

constexpr auto fails(builtin_op op, std::initializer_list<value> vs) -> bool {
    return applied(op, vs).is_error();
}

// ------------------------------------------------------------------
// No builtin unwinds
// ------------------------------------------------------------------

// The third channel has no producer here, and that is a property worth
// pinning rather than assuming: nothing among the builtins is a
// control-transfer operator, so a builtin completes with a value or an
// error and never with an unwind in flight.
constexpr auto no_builtin_ever_unwinds() -> bool {
    return !applied(builtin_op::car, {nil()}).is_unwind() &&
           !applied(builtin_op::car, {number(1)}).is_unwind() &&
           !applied(builtin_op::add, {truth()}).is_unwind() &&
           !applied(builtin_op::cons, {}).is_unwind();
}

// ------------------------------------------------------------------
// Lists
// ------------------------------------------------------------------

// ANSI 14.1.2: (car nil) and (cdr nil) are nil, not errors.
constexpr auto car_and_cdr_of_nil_are_nil() -> bool {
    return yields(builtin_op::car, {nil()}, nil()) &&
           yields(builtin_op::cdr, {nil()}, nil());
}

constexpr auto cons_car_cdr_round_trip() -> bool {
    store_type store;
    auto const pair = apply_builtin(
        builtin_op::cons, args_of({number(1), number(2)}), store, known);
    if (!pair.is_value()) {
        return false;
    }
    auto const one = args_of({pair.as_value()});
    return apply_builtin(builtin_op::car, one, store, known).as_value() ==
               number(1) &&
           apply_builtin(builtin_op::cdr, one, store, known).as_value() ==
               number(2);
}

constexpr auto list_builds_a_proper_list() -> bool {
    store_type store;
    auto const built =
        apply_builtin(builtin_op::list,
                      args_of({number(1), number(2), number(3)}), store, known);
    if (!built.is_value()) {
        return false;
    }
    auto const one = args_of({built.as_value()});
    return apply_builtin(builtin_op::length, one, store, known).as_value() ==
               number(3) &&
           apply_builtin(builtin_op::car, one, store, known).as_value() ==
               number(1);
}

constexpr auto list_of_nothing_is_nil() -> bool {
    return yields(builtin_op::list, {}, nil());
}

constexpr auto car_of_a_non_list_is_diagnosed() -> bool {
    return fails(builtin_op::car, {number(1)}) &&
           fails(builtin_op::cdr, {truth()}) &&
           fails(builtin_op::length, {number(1)});
}

// ------------------------------------------------------------------
// Predicates
// ------------------------------------------------------------------

constexpr auto null_and_not_are_the_same_test() -> bool {
    return yields(builtin_op::null, {nil()}, truth()) &&
           yields(builtin_op::not_, {nil()}, truth()) &&
           yields(builtin_op::null, {number(0)}, nil()) &&
           yields(builtin_op::not_, {number(0)}, nil());
}

// `nil` is an atom and a symbol and the empty list, all at once.
constexpr auto nil_answers_to_three_predicates() -> bool {
    return yields(builtin_op::atom, {nil()}, truth()) &&
           yields(builtin_op::symbolp, {nil()}, truth()) &&
           yields(builtin_op::consp, {nil()}, nil()) &&
           yields(builtin_op::null, {nil()}, truth());
}

constexpr auto consp_and_atom_are_complementary() -> bool {
    store_type store;
    auto const pair =
        apply_builtin(builtin_op::cons, args_of({nil(), nil()}), store, known);
    auto const one = args_of({pair.as_value()});
    return apply_builtin(builtin_op::consp, one, store, known).as_value() ==
               truth() &&
           apply_builtin(builtin_op::atom, one, store, known).as_value() ==
               nil();
}

// Identity is real here — an id, a cell index — rather than the pivot's
// string comparison on a symbol name (DIV-0002), so eq on two distinct
// cells is false even when their contents agree.
constexpr auto eq_is_identity_not_structure() -> bool {
    store_type store;
    auto const first = apply_builtin(builtin_op::cons,
                                     args_of({number(1), nil()}), store, known);
    auto const second = apply_builtin(
        builtin_op::cons, args_of({number(1), nil()}), store, known);
    auto const two = args_of({first.as_value(), second.as_value()});
    auto const same = args_of({first.as_value(), first.as_value()});
    return apply_builtin(builtin_op::eq, two, store, known).as_value() ==
               nil() &&
           apply_builtin(builtin_op::eq, same, store, known).as_value() ==
               truth();
}

// ------------------------------------------------------------------
// Arithmetic
// ------------------------------------------------------------------

// The identities, which is why + of nothing is 0 and * of nothing is 1.
constexpr auto n_ary_arithmetic_has_identities() -> bool {
    return yields(builtin_op::add, {}, number(0)) &&
           yields(builtin_op::multiply, {}, number(1)) &&
           yields(builtin_op::add, {number(2), number(3), number(4)},
                  number(9)) &&
           yields(builtin_op::multiply, {number(2), number(3)}, number(6));
}

// One argument negates; more subtract left to right.
constexpr auto subtraction_negates_one_argument() -> bool {
    return yields(builtin_op::subtract, {number(5)}, number(-5)) &&
           yields(builtin_op::subtract, {number(10), number(3), number(2)},
                  number(5)) &&
           fails(builtin_op::subtract, {});
}

// The comparisons are n-ary in ANSI: no adjacent pair out of order.
constexpr auto comparisons_are_n_ary() -> bool {
    return yields(builtin_op::less, {number(1), number(2), number(3)},
                  truth()) &&
           yields(builtin_op::less, {number(1), number(3), number(2)}, nil()) &&
           yields(builtin_op::greater, {number(3), number(2), number(1)},
                  truth()) &&
           yields(builtin_op::numeric_equal, {number(2), number(2)}, truth()) &&
           yields(builtin_op::numeric_equal, {number(2), number(3)}, nil()) &&
           yields(builtin_op::less, {number(1)}, truth());
}

constexpr auto arithmetic_on_a_non_number_is_diagnosed() -> bool {
    return fails(builtin_op::add, {number(1), truth()}) &&
           fails(builtin_op::less, {nil()});
}

// ------------------------------------------------------------------
// Arity
// ------------------------------------------------------------------

constexpr auto wrong_arity_is_diagnosed() -> bool {
    return fails(builtin_op::car, {}) &&
           fails(builtin_op::car, {nil(), nil()}) &&
           fails(builtin_op::cons, {nil()}) && fails(builtin_op::eq, {nil()});
}

// ------------------------------------------------------------------
// Installation
// ------------------------------------------------------------------

// A builtin lives in an ordinary function slot, which is what keeps it out
// of the call path as a special case (decision D12).
constexpr auto installing_fills_function_slots() -> bool {
    table_type symbols;
    auto const known_ids = install_builtins(symbols);
    if (!known_ids.has_value()) {
        return false;
    }
    auto const car = symbols.find("CAR");
    auto const plus = symbols.find("+");
    return car && plus && symbols.function(*car).has_value() &&
           *symbols.function(*car) == function_binding{builtin_op::car} &&
           *symbols.function(*plus) == function_binding{builtin_op::add} &&
           symbols.name(known_ids.value().nil) == "NIL" &&
           symbols.name(known_ids.value().t) == "T" &&
           // Nothing was put in a value or macro slot.
           !symbols.value(*car).has_value() && !symbols.macro(*car).has_value();
}

constexpr auto every_builtin_is_installed() -> bool {
    table_type symbols;
    if (!install_builtins(symbols).has_value()) {
        return false;
    }
    return std::ranges::all_of(builtins, [&symbols](auto const &entry) {
        auto const id = symbols.find(entry.name);
        return id && symbols.function(*id).has_value() &&
               *symbols.function(*id) == function_binding{entry.op};
    });
}

// Capacity is diagnosed here too: a table too small to hold the builtins
// says so rather than asserting.
constexpr auto a_table_too_small_is_diagnosed() -> bool {
    symbol_table<4, 512> symbols;
    return !install_builtins(symbols).has_value();
}

} // namespace

static_assert(no_builtin_ever_unwinds());
static_assert(car_and_cdr_of_nil_are_nil());
static_assert(cons_car_cdr_round_trip());
static_assert(list_builds_a_proper_list());
static_assert(list_of_nothing_is_nil());
static_assert(car_of_a_non_list_is_diagnosed());
static_assert(null_and_not_are_the_same_test());
static_assert(nil_answers_to_three_predicates());
static_assert(consp_and_atom_are_complementary());
static_assert(eq_is_identity_not_structure());
static_assert(n_ary_arithmetic_has_identities());
static_assert(subtraction_negates_one_argument());
static_assert(comparisons_are_n_ary());
static_assert(arithmetic_on_a_non_number_is_diagnosed());
static_assert(wrong_arity_is_diagnosed());
static_assert(installing_fills_function_slots());
static_assert(every_builtin_is_installed());
static_assert(a_table_too_small_is_diagnosed());

TEST_CASE("BuiltinTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("BuiltinTest - NoBuiltinUnwinds") {
    CHECK(no_builtin_ever_unwinds());
}

TEST_CASE("BuiltinTest - Lists") {
    CHECK(car_and_cdr_of_nil_are_nil());
    CHECK(cons_car_cdr_round_trip());
    CHECK(list_builds_a_proper_list());
    CHECK(list_of_nothing_is_nil());
    CHECK(car_of_a_non_list_is_diagnosed());
}

TEST_CASE("BuiltinTest - Predicates") {
    CHECK(null_and_not_are_the_same_test());
    CHECK(nil_answers_to_three_predicates());
    CHECK(consp_and_atom_are_complementary());
    CHECK(eq_is_identity_not_structure());
}

TEST_CASE("BuiltinTest - Arithmetic") {
    CHECK(n_ary_arithmetic_has_identities());
    CHECK(subtraction_negates_one_argument());
    CHECK(comparisons_are_n_ary());
    CHECK(arithmetic_on_a_non_number_is_diagnosed());
    CHECK(wrong_arity_is_diagnosed());
}

TEST_CASE("BuiltinTest - Installation") {
    CHECK(installing_fills_function_slots());
    CHECK(every_builtin_is_installed());
    CHECK(a_table_too_small_is_diagnosed());
}
