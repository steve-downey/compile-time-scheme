// src/smd/cl/foundation/trampoline.test.cpp                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test (step R8, decision 2 corrected): the substantive
// trampoline tests moved to src/smd/kit/foundation/trampoline.test.cpp
// with the definition. This file only verifies that the forwarding shim
// compiles and that the forwarded names are usable from
// smd::cl::foundation exactly as before.

#include <smd/cl/foundation/trampoline.hpp>
#include <smd/cl/foundation/trampoline.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::step_status;
using smd::cl::foundation::trampoline;

namespace {
struct countdown {
    int remaining = 0;
};

constexpr auto tick(countdown &state) -> step_status {
    if (state.remaining == 0) {
        return step_status::done;
    }
    --state.remaining;
    return step_status::running;
}
} // namespace

TEST_CASE("TrampolineShimTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("TrampolineShimTest - RunsUntilDoneThroughShim") {
    countdown state{3};
    auto const taken = trampoline(state, 1000, tick);
    REQUIRE(taken.has_value());
    CHECK(taken.value() == 4);
    CHECK(state.remaining == 0);
}
