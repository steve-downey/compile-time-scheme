// src/smd/cl/foundation/trampoline.test.cpp                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/foundation/trampoline.hpp>
#include <smd/cl/foundation/trampoline.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::step_status;
using smd::cl::foundation::trampoline;

namespace {

// A machine that counts down to zero, one step at a time.
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

// A machine that never finishes, for the budget contract.
constexpr auto spin(countdown &state) -> step_status {
    ++state.remaining;
    return step_status::running;
}

constexpr auto steps_to_finish(int from) -> int {
    countdown state{from};
    auto const taken = trampoline(state, 1000, tick);
    return taken.has_value() ? taken.value() : -1;
}

constexpr auto final_state(int from) -> int {
    countdown state{from};
    auto const taken = trampoline(state, 1000, tick);
    return taken.has_value() ? state.remaining : -1;
}

constexpr auto budget_exhausted() -> bool {
    countdown state{0};
    return !trampoline(state, 8, spin).has_value();
}

} // namespace

TEST_CASE("TrampolineTest - HeaderIsIdempotent") { REQUIRE(true); }

// A machine already final is stepped exactly once — the step function is
// what decides it is final, so it must be asked at least that once.
static_assert(steps_to_finish(0) == 1);
static_assert(steps_to_finish(3) == 4);

// The state advances in place: the trampoline reports the count, the caller
// keeps the state.
static_assert(final_state(3) == 0);

// The budget is diagnosed, never asserted, and never silently truncated.
static_assert(budget_exhausted());

TEST_CASE("TrampolineTest - RunsUntilDone") {
    countdown state{5};
    auto const taken = trampoline(state, 1000, tick);
    REQUIRE(taken.has_value());
    REQUIRE(taken.value() == 6);
    REQUIRE(state.remaining == 0);
}

TEST_CASE("TrampolineTest - BudgetIsDiagnosed") {
    countdown state{0};
    auto const taken = trampoline(state, 8, spin);
    REQUIRE(!taken.has_value());
    // The budget was spent, not abandoned early.
    REQUIRE(state.remaining == 8);
}
