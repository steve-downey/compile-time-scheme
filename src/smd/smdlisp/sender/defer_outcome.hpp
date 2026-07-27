// src/smd/smdlisp/sender/defer_outcome.hpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_SENDER_DEFER_OUTCOME_HPP
#define SRC_SMD_SMDLISP_SENDER_DEFER_OUTCOME_HPP

#include <smd/smdlisp/sender/outcome.hpp>
#include <smd/smdlisp/sender/sender_v.hpp>

#include <type_traits>
#include <utility>

namespace smd::smdlisp::sender {

// e64f025f-7464-419a-b739-301569245544
/// The completion signature set shared by every sender in this backend.
///
/// Fixing one set for the whole backend, rather than computing it per
/// adapter, is what keeps the hand-written senders small: @ref
/// unwind_protect_sender can forward its child's completions verbatim
/// because the child's completions are always exactly these three.  It is
/// also the type-level statement of decision D5 -- a `smdlisp` computation
/// either produces a value, is diagnosed, or is unwinding -- so it is worth
/// having as a name rather than as a repeated triple.
///
/// @tparam Core The core AST type.
template <typename Core>
using lisp_completions = sender_v::completion_signatures<
    sender_v::set_value_t(closure::value<Core>),
    sender_v::set_error_t(smd::smdscheme::foundation::parse_error),
    sender_v::set_stopped_t()>;

/// The operation state of a @ref defer_outcome_sender: holds the thunk and
/// the receiver, and does the channel dispatch when started.
///
/// @tparam Core     The core AST type.
/// @tparam F        A nullary invocable returning @c outcome<Core>.
/// @tparam Receiver The connected receiver.
template <typename Core, typename F, typename Receiver>
struct defer_outcome_state {
    using operation_state_concept = sender_v::operation_state_tag;

    F fn;              ///< The deferred evaluation.
    Receiver receiver; ///< Whom to complete.

    /// Runs @ref fn and routes its @ref outcome to the matching completion
    /// function.
    ///
    /// This three-way `switch` is the *only* place in the backend where an
    /// @ref outcome becomes a real completion, which is what makes the
    /// mapping in @ref channel's docs a single auditable statement rather
    /// than a convention spread over twenty visitor arms.
    auto start() & noexcept -> void {
        outcome<Core> o = fn();
        switch (o.kind) {
        case channel::value:
            sender_v::set_value(std::move(receiver), std::move(o.val));
            return;
        case channel::error:
            sender_v::set_error(std::move(receiver), o.err);
            return;
        case channel::stopped:
            sender_v::set_stopped(std::move(receiver));
            return;
        }
    }
};

/// A lazy sender whose completion channel is decided at @c start time by
/// running a thunk.
///
/// This is the backend's one leaf sender, and it exists because none of the
/// stock factories can do the job: @c just always completes with a value,
/// @c just_error always with an error, @c just_stopped always stopped, and
/// which of the three a `smdlisp` form completes on is not known until the
/// form has been evaluated.  @c let_value cannot select between them either,
/// since all of its branches must have one type.
///
/// Being lazy matters, and is not incidental: nothing in @p fn runs until the
/// operation state is started, so wrapping this sender in @ref
/// unwind_protect_sender genuinely places the protected form's evaluation
/// *inside* the adapter's operation, which is what makes cleanup-on-every-
/// channel a property of the sender graph rather than of the C++ control flow
/// around it.
///
/// @tparam Core The core AST type.
/// @tparam F    A nullary invocable returning @c outcome<Core>.
template <typename Core, typename F>
struct defer_outcome_sender {
    using sender_concept = sender_v::sender_tag;

    F fn; ///< The deferred evaluation.

    /// Reports the fixed backend-wide completion set (@ref lisp_completions).
    template <typename Self, typename... Env>
    static consteval auto get_completion_signatures() {
        return lisp_completions<Core>{};
    }

    /// Connects to @p r, moving the thunk into the operation state.
    template <typename Receiver>
    auto connect(Receiver r) && -> defer_outcome_state<Core, F, Receiver> {
        return defer_outcome_state<Core, F, Receiver>{std::move(fn),
                                                      std::move(r)};
    }
};

/// Builds a @ref defer_outcome_sender from @p fn.
///
/// @tparam Core The core AST type.
/// @param  fn   A nullary invocable returning @c outcome<Core>.
template <typename Core, typename F>
[[nodiscard]] auto defer_outcome(F fn)
    -> defer_outcome_sender<Core, std::decay_t<F>> {
    return defer_outcome_sender<Core, std::decay_t<F>>{std::move(fn)};
}
// e64f025f-7464-419a-b739-301569245544 end

} // namespace smd::smdlisp::sender

#endif
