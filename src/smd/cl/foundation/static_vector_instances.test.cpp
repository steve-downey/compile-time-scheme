// src/smd/cl/foundation/static_vector_instances.test.cpp            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test (step R8, decision 2 corrected): the substantive
// static_vector typeclass instance law tests moved to
// src/smd/kit/foundation/static_vector_instances.test.cpp with the
// definition. This file only verifies that the forwarding shim compiles
// and that the forwarded names are usable from smd::cl::foundation exactly
// as before.

#include <smd/cl/foundation/static_vector_instances.hpp>
#include <smd/cl/foundation/static_vector_instances.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::fmap;
using smd::cl::foundation::fold_left;
using smd::cl::foundation::length;
using smd::cl::foundation::static_vector;

TEST_CASE("StaticVectorInstancesShimTest - HeaderIsIdempotent") {
    REQUIRE(true);
}

TEST_CASE("StaticVectorInstancesShimTest - FunctorAndFoldableThroughShim") {
    static_vector<int, 3> xs;
    xs.push_back(1);
    xs.push_back(2);
    xs.push_back(3);
    auto mapped = fmap([](int x) { return x + 1; }, xs);
    CHECK(mapped[0] == 2);
    CHECK(length(xs) == 3);
    CHECK(fold_left([](int acc, int x) { return acc + x; }, 0, xs) == 6);
}
