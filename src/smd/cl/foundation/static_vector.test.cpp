// src/smd/cl/foundation/static_vector.test.cpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Reviewed union of the two prior copies of this test:
// src/smd/smdscheme/foundation/static_vector.test.cpp (compile-time-scheme) and
// src/smd/forth/foundation/static_vector.test.cpp (compile-time-forth).

#include <smd/cl/foundation/static_vector.hpp>
#include <smd/cl/foundation/static_vector.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

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
