// src/smd/cl/foundation/static_vector.test.cpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Reviewed union of the two prior copies of this test:
// src/smd/smdscheme/foundation/static_vector.test.cpp (compile-time-scheme) and
// src/smd/forth/foundation/static_vector.test.cpp (compile-time-forth).

#include <smd/cl/foundation/static_vector.hpp>
#include <smd/cl/foundation/static_vector.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <string_view>

using smd::cl::foundation::static_vector;

constexpr auto make_vec() {
    static_vector<int, 4> xs;
    xs.push_back(1);
    xs.push_back(2);
    return xs;
}

static_assert(make_vec().size() == 2);
static_assert(!make_vec().empty());
static_assert(make_vec()[0] == 1);
static_assert(make_vec()[1] == 2);

static_assert(static_vector<int, 4>{}.empty());
static_assert(static_vector<int, 4>{}.size() == 0);
static_assert(static_vector<int, 4>{}.capacity() == 4);
static_assert(make_vec().capacity() == 4);

constexpr auto filled_holds_count_copies() -> bool {
    auto const xs = static_vector<int, 4>::filled(3, -1);
    return xs.size() == 3 && xs.capacity() == 4 && xs[0] == -1 && xs[1] == -1 &&
           xs[2] == -1;
}

constexpr auto filled_zero_is_empty() -> bool {
    return static_vector<int, 4>::filled(0, 7).empty();
}

constexpr auto filled_to_capacity() -> bool {
    auto const xs = static_vector<int, 4>::filled(4, 7);
    return xs.size() == 4 && xs.size() == xs.capacity() && xs[3] == 7;
}

constexpr auto append_range_appends_in_order() -> bool {
    static_vector<int, 8> xs;
    xs.push_back(1);
    xs.append_range(make_vec());
    return xs.size() == 3 && xs[0] == 1 && xs[1] == 1 && xs[2] == 2;
}

constexpr auto append_range_of_empty_is_identity() -> bool {
    auto xs = make_vec();
    xs.append_range(static_vector<int, 4>{});
    return xs == make_vec();
}

constexpr auto append_range_converts_elements() -> bool {
    // The reference type need only be convertible: char into a char vector
    // is how symbol_table appends an interned name.
    static_vector<char, 8> chars;
    chars.append_range(std::string_view{"AB"});
    return chars.size() == 2 && chars[0] == 'A' && chars[1] == 'B';
}

static_assert(filled_holds_count_copies());
static_assert(filled_zero_is_empty());
static_assert(filled_to_capacity());
static_assert(append_range_appends_in_order());
static_assert(append_range_of_empty_is_identity());
static_assert(append_range_converts_elements());

TEST_CASE("StaticVectorTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("StaticVectorTest - DefaultConstructionEmpty") {
    constexpr static_vector<int, 8> v;
    CHECK(v.empty());
    CHECK(v.size() == 0);
}

TEST_CASE("StaticVectorTest - PushBackAndAccess") {
    static_vector<int, 4> v;
    v.push_back(10);
    v.push_back(20);
    v.push_back(30);
    CHECK(v.size() == 3);
    CHECK(v[0] == 10);
    CHECK(v[1] == 20);
    CHECK(v[2] == 30);
}

TEST_CASE("StaticVectorTest - ConstexprUsage") {
    constexpr auto v = make_vec();
    CHECK(v.size() == 2);
    CHECK(v[0] == 1);
    CHECK(v[1] == 2);
}

TEST_CASE("StaticVectorTest - CapacityIsFixed") {
    static_vector<int, 4> v;
    CHECK(v.capacity() == 4);
    v.push_back(1);
    CHECK(v.capacity() == 4);
    CHECK(v.size() < v.capacity());
}

TEST_CASE("StaticVectorTest - MutatingAccess") {
    static_vector<int, 4> v;
    v.push_back(5);
    v[0] = 99;
    CHECK(v[0] == 99);
}

TEST_CASE("StaticVectorTest - FilledHoldsCountCopies") {
    CHECK(filled_holds_count_copies());
    CHECK(filled_zero_is_empty());
    CHECK(filled_to_capacity());
}

TEST_CASE("StaticVectorTest - AppendRange") {
    CHECK(append_range_appends_in_order());
    CHECK(append_range_of_empty_is_identity());
    CHECK(append_range_converts_elements());
}
