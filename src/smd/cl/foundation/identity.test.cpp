// src/smd/cl/foundation/identity.test.cpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test (step R8, decision 2 corrected): the substantive
// identity Functor/Applicative law tests moved to
// src/smd/kit/foundation/identity.test.cpp with the definition. This file
// only verifies that the forwarding shim compiles and that the forwarded
// names are usable from smd::cl::foundation exactly as before.

#include <smd/cl/foundation/identity.hpp>
#include <smd/cl/foundation/identity.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::identity;
using smd::cl::foundation::identity_applicative_map;

TEST_CASE("IdentityShimTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("IdentityShimTest - ApplyThroughShim") {
    identity_applicative_map m{};
    auto add_one = m.pure([](int x) { return x + 1; });
    CHECK(m.apply(add_one, m.pure(4)) == identity<int>{5});
}
