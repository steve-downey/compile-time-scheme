// src/smd/cl/sender/run_sender.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_SENDER_RUN_SENDER_HPP
#define SRC_SMD_CL_SENDER_RUN_SENDER_HPP

#include <smd/cl/eval/outcome.hpp>
#include <smd/cl/eval/value.hpp>
#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/sender/sender_v.hpp>

#include <utility>

namespace smd::cl::sender {

// bc5a6bf9-8aa1-45f2-bcbf-39c3ff2966b2
/// A receiver that records which of three completion channels fired, as an
/// @ref eval::outcome.
///
/// The inverse of @ref machine_operation_state::start (`machine_sender.hpp`):
/// that turns an @c eval::outcome into a completion, this turns a completion
/// back into one. Both directions exist for the same reason -- D13's claim is
/// that a value/error/unwind outcome and a value/error/stopped completion are
/// the same fact seen from two sides -- so both halves of the round trip are
/// worth having as named code rather than as a claim in a comment.
///
/// A stopped completion carries nothing back (Execution26's `set_stopped` is
/// niladic, and by the time one reaches the top of a program every live
/// `block`/`catch` has already had first refusal at it), so it is reported as
/// a diagnosed error: an unwind with nowhere left to land is not a value, and
/// there is no record left of which exit it even was.
struct outcome_receiver {
    using receiver_concept = sender_v::receiver_tag;

    eval::outcome *out; ///< Where the observed completion is written.

    /// Records a value completion.
    auto set_value(eval::value v) && noexcept -> void {
        *out = eval::outcome{std::move(v)};
    }
    /// Records an error completion.
    auto set_error(foundation::parse_error e) && noexcept -> void {
        *out = eval::outcome{e};
    }
    /// Records a stopped completion: an unwind reached this driver with no
    /// scope left to claim it.
    auto set_stopped() && noexcept -> void {
        *out = eval::outcome{
            foundation::parse_error{{},
                                    "an unwind reached the top of the "
                                    "program uncaught"}};
    }
};

/// Connects @p s to an @ref outcome_receiver, starts it, and returns the
/// completion it observed.
///
/// Every sender this backend builds completes synchronously and inline —
/// DIV-0015 records that nothing here can suspend across a real scheduler
/// boundary yet — so the operation state can live on the stack for the
/// duration of this call and the observed outcome is guaranteed written by
/// the time @c start returns.
///
/// @tparam S A sender whose completions are exactly @ref machine_completions'
///           three: `set_value_t(eval::value)`, `set_error_t(parse_error)`,
///           `set_stopped_t()`.
template <typename S>
[[nodiscard]] auto run_sender(S &&s) -> eval::outcome {
    eval::outcome observed{eval::value{}};
    auto op =
        sender_v::connect(std::forward<S>(s), outcome_receiver{&observed});
    sender_v::start(op);
    return observed;
}
// bc5a6bf9-8aa1-45f2-bcbf-39c3ff2966b2 end

} // namespace smd::cl::sender

#endif
