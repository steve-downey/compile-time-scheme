// src/smd/kit/foundation/fold_left_short.test.cpp                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Moved in step R8 (decision 2, corrected) from
// src/smd/cl/foundation/fold_left_short.test.cpp.

#include <smd/kit/foundation/fold_left_short.hpp>
#include <smd/kit/foundation/fold_left_short.hpp> // test 2nd include OK

#include <smd/kit/foundation/parse_error.hpp>
#include <smd/kit/foundation/result.hpp>
#include <smd/kit/foundation/static_vector.hpp>

#include <catch2/catch_test_macros.hpp>

using smd::kit::foundation::fold_left_short;
using smd::kit::foundation::parse_error;
using smd::kit::foundation::result;
using smd::kit::foundation::short_circuit_effect;
using smd::kit::foundation::source_pos;
using smd::kit::foundation::static_vector;

TEST_CASE("FoldLeftShortTest - HeaderIsIdempotent") { REQUIRE(true); }

static_assert(short_circuit_effect<result<int>>);
static_assert(!short_circuit_effect<int>);

namespace {

constexpr auto values_1_2_3() -> static_vector<int, 4> {
    static_vector<int, 4> xs;
    xs.push_back(1);
    xs.push_back(2);
    xs.push_back(3);
    return xs;
}

constexpr auto checked_add(int acc, int element) -> result<int> {
    if (element < 0) {
        return parse_error{source_pos{}, "negative element"};
    }
    return acc + element;
}

} // namespace

// All steps succeed: the fold accumulates left to right.
static_assert(fold_left_short(values_1_2_3(), 0, checked_add).has_value());
static_assert(fold_left_short(values_1_2_3(), 0, checked_add).value() == 6);

// An empty range yields the initial accumulator, successfully.
static_assert(
    fold_left_short(static_vector<int, 4>{}, 42, checked_add).value() == 42);

// A failing step yields that step's error.
constexpr auto with_negative = [] {
    static_vector<int, 4> xs;
    xs.push_back(1);
    xs.push_back(-2);
    xs.push_back(3);
    return xs;
}();
static_assert(!fold_left_short(with_negative, 0, checked_add).has_value());

TEST_CASE("FoldLeftShortTest - AccumulatesOnSuccess") {
    auto folded = fold_left_short(values_1_2_3(), 0, checked_add);
    REQUIRE(folded.has_value());
    CHECK(folded.value() == 6);
}

TEST_CASE("FoldLeftShortTest - EmptyRangeYieldsInit") {
    auto folded = fold_left_short(static_vector<int, 4>{}, 42, checked_add);
    REQUIRE(folded.has_value());
    CHECK(folded.value() == 42);
}

TEST_CASE("FoldLeftShortTest - FirstErrorIsReturned") {
    auto folded = fold_left_short(with_negative, 0, checked_add);
    REQUIRE(!folded.has_value());
    CHECK(folded.error() == parse_error{source_pos{}, "negative element"});
}

TEST_CASE("FoldLeftShortTest - StopsVisitingAfterFirstError") {
    // The defining difference from std::ranges::fold_left and from
    // traverse: no element after the failing one is visited.
    int visited = 0;
    auto counting_step = [&visited](int acc, int element) -> result<int> {
        ++visited;
        if (element < 0) {
            return parse_error{source_pos{}, "negative element"};
        }
        return acc + element;
    };
    auto folded = fold_left_short(with_negative, 0, counting_step);
    CHECK(!folded.has_value());
    CHECK(visited == 2); // 1 succeeded, -2 failed, 3 never visited
}
