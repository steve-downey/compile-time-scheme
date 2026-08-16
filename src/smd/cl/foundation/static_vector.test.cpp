// src/smd/cl/foundation/static_vector.test.cpp                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test (step R8): the substantive static_vector tests moved to
// src/smd/kit/foundation/static_vector.test.cpp with the definition. This
// file only verifies that the forwarding shim compiles and that the
// forwarded name is usable from smd::cl::foundation exactly as before,
// including the capacity-as-storage contract (D14) that every downstream
// cl area (eval::machine's frame stack, symbol_table's name pool) depends
// on unchanged.

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
static_assert(make_vec().capacity() == 4);

TEST_CASE("StaticVectorShimTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("StaticVectorShimTest - PushBackPopBackThroughShim") {
    static_vector<int, 4> v;
    v.push_back(10);
    v.push_back(20);
    v.pop_back();
    CHECK(v.size() == 1);
    CHECK(v[0] == 10);
    CHECK(v.capacity() == 4);
}
