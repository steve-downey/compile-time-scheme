// src/smd/cl/foundation/result_instances.test.cpp                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test (step R8, decision 2 corrected): the substantive result
// typeclass instance law tests moved to
// src/smd/kit/foundation/result_instances.test.cpp with the definition.
// This file only verifies that the forwarding shim compiles and that the
// forwarded names are usable from smd::cl::foundation exactly as before.

#include <smd/cl/foundation/result_instances.hpp>
#include <smd/cl/foundation/result_instances.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::fmap;
using smd::cl::foundation::invoke;
using smd::cl::foundation::result;

TEST_CASE("ResultInstancesShimTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ResultInstancesShimTest - FmapAndInvokeThroughShim") {
    CHECK(fmap([](int x) { return x + 1; }, result<int>{1}) == result<int>{2});
    auto summed = invoke([](int x, int y) { return x + y; }, result<int>{3},
                         result<int>{4});
    CHECK(summed == result<int>{7});
}
