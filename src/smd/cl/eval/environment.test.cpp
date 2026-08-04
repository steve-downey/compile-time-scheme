// src/smd/cl/eval/environment.test.cpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/eval/environment.hpp>
#include <smd/cl/eval/environment.hpp> // test 2nd include OK

#include <smd/cl/eval/heap.hpp>
#include <smd/cl/eval/value.hpp>
#include <smd/cl/symbol/symbol_id.hpp>

#include <catch2/catch_test_macros.hpp>

using smd::cl::eval::heap;
using smd::cl::eval::standard_symbols;
using smd::cl::eval::value;
using smd::cl::eval::value_fixnum;
using smd::cl::eval::environment::extend;
using smd::cl::eval::environment::lookup;
using smd::cl::symbol::symbol_id;

namespace {

using store_type = heap<32, 4, 2>;

constexpr standard_symbols known{symbol_id{0}, symbol_id{1}};
constexpr symbol_id x{2};
constexpr symbol_id y{3};
constexpr symbol_id z{4};

constexpr auto empty() -> value { return smd::cl::eval::nil_value(known); }

constexpr auto number(int n) -> value { return value{value_fixnum{n}}; }

// ------------------------------------------------------------------
// The three laws an association-list environment has to satisfy
// ------------------------------------------------------------------

// Nothing is bound in the empty environment, and the empty environment is
// just a value — no separate arena and no capacity of its own (D14).
constexpr auto nothing_is_bound_in_the_empty_environment() -> bool {
    store_type store;
    return !lookup(store, empty(), x).has_value();
}

// extend then lookup returns what was bound.
constexpr auto lookup_after_extend_finds_the_binding() -> bool {
    store_type store;
    auto const scope = extend(store, empty(), x, number(1));
    if (!scope.has_value()) {
        return false;
    }
    auto const found = lookup(store, scope.value(), x);
    return found.has_value() && *found == number(1);
}

// Extending with one name leaves every other name alone.
constexpr auto extend_touches_only_the_name_it_binds() -> bool {
    store_type store;
    auto const outer = extend(store, empty(), x, number(1));
    auto const inner = extend(store, outer.value(), y, number(2));
    if (!inner.has_value()) {
        return false;
    }
    auto const found_x = lookup(store, inner.value(), x);
    auto const found_y = lookup(store, inner.value(), y);
    return found_x.has_value() && *found_x == number(1) &&
           found_y.has_value() && *found_y == number(2) &&
           !lookup(store, inner.value(), z).has_value();
}

// Shadowing is not a rule the environment enforces; it falls out of the
// representation, because extend adds to the left and lookup finds the
// leftmost binding.
constexpr auto the_innermost_binding_wins() -> bool {
    store_type store;
    auto const outer = extend(store, empty(), x, number(1));
    auto const inner = extend(store, outer.value(), x, number(2));
    if (!inner.has_value()) {
        return false;
    }
    auto const shadowed = lookup(store, inner.value(), x);
    auto const original = lookup(store, outer.value(), x);
    // The outer environment is a value, so extending it did not change it.
    return shadowed.has_value() && *shadowed == number(2) &&
           original.has_value() && *original == number(1);
}

// ------------------------------------------------------------------
// Capacity
// ------------------------------------------------------------------

constexpr auto a_full_heap_makes_extend_fail() -> bool {
    heap<1, 1, 1> store;
    return !extend(store, empty(), x, number(1)).has_value();
}

} // namespace

static_assert(nothing_is_bound_in_the_empty_environment());
static_assert(lookup_after_extend_finds_the_binding());
static_assert(extend_touches_only_the_name_it_binds());
static_assert(the_innermost_binding_wins());
static_assert(a_full_heap_makes_extend_fail());

TEST_CASE("EnvironmentTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("EnvironmentTest - Laws") {
    CHECK(nothing_is_bound_in_the_empty_environment());
    CHECK(lookup_after_extend_finds_the_binding());
    CHECK(extend_touches_only_the_name_it_binds());
    CHECK(the_innermost_binding_wins());
}

TEST_CASE("EnvironmentTest - Capacity") {
    CHECK(a_full_heap_makes_extend_fail());
}
