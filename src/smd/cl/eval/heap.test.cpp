// src/smd/cl/eval/heap.test.cpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/eval/heap.hpp>
#include <smd/cl/eval/heap.hpp> // test 2nd include OK

#include <smd/cl/eval/value.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/symbol/symbol_id.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <iterator>
#include <ranges>

using smd::cl::eval::as_cons;
using smd::cl::eval::closure_ref;
using smd::cl::eval::cons_ref;
using smd::cl::eval::heap;
using smd::cl::eval::standard_symbols;
using smd::cl::eval::value;
using smd::cl::eval::value_fixnum;
using smd::cl::eval::value_string;
using smd::cl::foundation::result;
using smd::cl::symbol::symbol_id;

namespace {

using store_type = heap<16, 16, 4>;

constexpr standard_symbols known{symbol_id{0}, symbol_id{1}};

constexpr auto nil() -> value { return smd::cl::eval::nil_value(known); }

constexpr auto number(int n) -> value { return value{value_fixnum{n}}; }

// ------------------------------------------------------------------
// list_view's contract: order, termination, emptiness
// ------------------------------------------------------------------

// The range instance's documented order is car first, then the cdr chain.
static_assert(std::ranges::forward_range<store_type::list_view>);
static_assert(std::ranges::view<store_type::list_view>);

// Builds (1 2 3) and reads it back through list_view.
constexpr auto list_view_yields_elements_in_order() -> bool {
    store_type store;
    auto tail = store.cons(number(3), nil());
    auto middle = store.cons(number(2), tail.value());
    auto head = store.cons(number(1), middle.value());
    int seen = 0;
    int product = 1;
    std::ranges::for_each(store.list(head.value()), [&](value const &v) {
        ++seen;
        product *= 10;
        product += std::get<value_fixnum>(v).value;
    });
    return seen == 3 && product == 1123;
}

// The empty list has no elements, and that falls out of the representation
// rather than being a special case: `nil` is not a cons, so the range is
// empty for the same reason a fixnum's is.
constexpr auto a_non_cons_is_an_empty_range() -> bool {
    store_type store;
    return std::ranges::distance(store.list(nil())) == 0 &&
           std::ranges::distance(store.list(number(7))) == 0 &&
           std::ranges::empty(store.list(nil()));
}

// Iteration stops at the first non-cons tail, so a dotted list yields its
// elements and drops its tail. Documented behaviour, pinned.
constexpr auto a_dotted_list_drops_its_tail() -> bool {
    store_type store;
    auto const dotted = store.cons(number(1), number(2));
    return std::ranges::distance(store.list(dotted.value())) == 1;
}

// ------------------------------------------------------------------
// Allocation
// ------------------------------------------------------------------

constexpr auto cons_returns_a_handle_to_what_it_stored() -> bool {
    store_type store;
    auto const pair = store.cons(number(4), number(5));
    if (!pair.has_value()) {
        return false;
    }
    cons_ref const ref = as_cons(pair.value());
    return ref.valid() && store.cell(ref).car == number(4) &&
           store.cell(ref).cdr == number(5) && !as_cons(number(4)).valid();
}

constexpr auto strings_own_their_characters() -> bool {
    store_type store;
    auto const first = store.make_string("ab");
    auto const second = store.make_string("cde");
    if (!first.has_value() || !second.has_value()) {
        return false;
    }
    return store.string(std::get<value_string>(first.value()).ref) == "ab" &&
           store.string(std::get<value_string>(second.value()).ref) == "cde";
}

constexpr auto closures_remember_node_and_environment() -> bool {
    store_type store;
    auto const made = store.make_closure(9, number(3));
    return made.has_value() && made.value().valid() &&
           store.closure_at(made.value()).lambda_node == 9 &&
           store.closure_at(made.value()).env == number(3);
}

// ------------------------------------------------------------------
// Capacity is diagnosed, never asserted
// ------------------------------------------------------------------

// A program merely too big for its arena gets an error, not an assert and
// not a truncated answer. The discipline the elaborator's add_leaf_checked
// established, carried into the evaluator.
constexpr auto a_full_heap_is_diagnosed() -> bool {
    heap<1, 1, 1> store;
    return store.cons(nil(), nil()).has_value() &&
           !store.cons(nil(), nil()).has_value();
}

constexpr auto a_full_string_pool_is_diagnosed() -> bool {
    heap<1, 3, 1> store;
    return store.make_string("abc").has_value() &&
           !store.make_string("d").has_value() &&
           !heap<1, 3, 1>{}.make_string("abcd").has_value();
}

constexpr auto too_many_closures_is_diagnosed() -> bool {
    heap<1, 1, 1> store;
    return store.make_closure(0, nil()).has_value() &&
           !store.make_closure(1, nil()).has_value();
}

} // namespace

static_assert(list_view_yields_elements_in_order());
static_assert(a_non_cons_is_an_empty_range());
static_assert(a_dotted_list_drops_its_tail());
static_assert(cons_returns_a_handle_to_what_it_stored());
static_assert(strings_own_their_characters());
static_assert(closures_remember_node_and_environment());
static_assert(a_full_heap_is_diagnosed());
static_assert(a_full_string_pool_is_diagnosed());
static_assert(too_many_closures_is_diagnosed());

TEST_CASE("HeapTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("HeapTest - ListViewContract") {
    CHECK(list_view_yields_elements_in_order());
    CHECK(a_non_cons_is_an_empty_range());
    CHECK(a_dotted_list_drops_its_tail());
}

TEST_CASE("HeapTest - Allocation") {
    CHECK(cons_returns_a_handle_to_what_it_stored());
    CHECK(strings_own_their_characters());
    CHECK(closures_remember_node_and_environment());
}

TEST_CASE("HeapTest - CapacityIsDiagnosed") {
    CHECK(a_full_heap_is_diagnosed());
    CHECK(a_full_string_pool_is_diagnosed());
    CHECK(too_many_closures_is_diagnosed());
}
