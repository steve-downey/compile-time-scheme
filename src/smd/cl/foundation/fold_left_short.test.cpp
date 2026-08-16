// src/smd/cl/foundation/fold_left_short.test.cpp                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test (step R8, decision 2 corrected): the substantive
// fold_left_short tests moved to
// src/smd/kit/foundation/fold_left_short.test.cpp with the definition.
// This file only verifies that the forwarding shim compiles and that the
// forwarded names are usable from smd::cl::foundation exactly as before.

#include <smd/cl/foundation/fold_left_short.hpp>
#include <smd/cl/foundation/fold_left_short.hpp> // test 2nd include OK

#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/static_vector.hpp>

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::fold_left_short;
using smd::cl::foundation::parse_error;
using smd::cl::foundation::result;
using smd::cl::foundation::short_circuit_effect;
using smd::cl::foundation::static_vector;

TEST_CASE("FoldLeftShortShimTest - HeaderIsIdempotent") { REQUIRE(true); }

static_assert(short_circuit_effect<result<int>>);

TEST_CASE("FoldLeftShortShimTest - AccumulatesThroughShim") {
    static_vector<int, 3> xs;
    xs.push_back(1);
    xs.push_back(2);
    xs.push_back(3);
    auto checked_add = [](int acc, int element) -> result<int> {
        if (element < 0) {
            return parse_error{{}, "negative element"};
        }
        return acc + element;
    };
    auto folded = fold_left_short(xs, 0, checked_add);
    REQUIRE(folded.has_value());
    CHECK(folded.value() == 6);
}
