// src/smd/kit/foundation/trampoline.hpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Extracted in step R8 from smd::cl::foundation::trampoline. Decision 2's
// original cut left this in cl on the grounds that no sibling front end had
// a small-step machine to drive. That reasoning was overturned: the
// absence reflects the algebraic refactoring never having been done on the
// Forth side (compile-time-forth's own interpreter is exactly the kind of
// state machine this drives), not a fact about this code's generality. The
// corrected standard is docs/cl-rebuild-plan.md §5's own test — generic in
// shape and free of language-specific types — which this file passes
// outright: @c State and @c Step are both abstract, and @c step_status
// names nothing from cl's evaluator.
#ifndef SRC_SMD_KIT_FOUNDATION_TRAMPOLINE_HPP
#define SRC_SMD_KIT_FOUNDATION_TRAMPOLINE_HPP

#include <smd/kit/foundation/parse_error.hpp>
#include <smd/kit/foundation/result.hpp>

#include <concepts>
#include <functional>

namespace smd::kit::foundation {

/// What one step of a small-step machine reports about the next one.
enum class step_status : unsigned char {
    running, ///< The state advanced and can advance again.
    done     ///< The state is final; do not step it again.
};

/// A step function: advances a machine state in place and says whether the
/// machine is finished.
///
/// A step reports no error of its own. A machine whose outcomes include
/// failure carries that failure in its own state — which is the whole point
/// of decision D13's three channels — so the only thing this algorithm can
/// fail at is running out of budget.
template <class F, class State>
concept machine_step = requires(F &f, State &state) {
    { std::invoke(f, state) } -> std::same_as<step_status>;
};

/// Drives @p state by applying @p step until it reports
/// @ref step_status::done, at most @p max_steps times.
///
/// The step budget is a real part of the contract rather than a safety net.
/// A non-terminating object program must not become a non-terminating
/// *translation*: constant evaluation of an unbounded loop is a compiler
/// hang, so exhausting the budget is diagnosed the way every other capacity
/// in this tree is diagnosed, and never asserted.
///
/// @tparam State The machine state, advanced in place.
/// @tparam Step  Callable with signature @c step_status(State&).
/// @param  state     The initial state; on return, the state reached.
/// @param  max_steps The budget, in steps.
/// @param  step      The step function.
/// @return The number of steps taken, or an error if the budget ran out
///         before the machine finished.
// 4b3c37ec-1a54-4b6b-a10c-4d20ff2e3427
template <class State, class Step>
    requires machine_step<Step, State>
[[nodiscard]] constexpr auto trampoline(State &state, int max_steps, Step step)
    -> result<int> {
    int taken = 0;
    while (taken < max_steps) { // substrate generic algorithm
        ++taken;
        if (std::invoke(step, state) == step_status::done) {
            return taken;
        }
    }
    return parse_error{{}, "evaluation step limit exceeded"};
}
// 4b3c37ec-1a54-4b6b-a10c-4d20ff2e3427 end

} // namespace smd::kit::foundation

#endif
