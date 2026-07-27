// src/smd/smdlisp/sender/unwind_protect.hpp                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_SENDER_UNWIND_PROTECT_HPP
#define SRC_SMD_SMDLISP_SENDER_UNWIND_PROTECT_HPP

#include <smd/smdlisp/sender/defer_outcome.hpp>
#include <smd/smdlisp/sender/outcome.hpp>
#include <smd/smdlisp/sender/sender_v.hpp>

#include <type_traits>
#include <utility>

namespace smd::smdlisp::sender {

namespace detail {

/// Routes @p o to the matching completion function of @p r.
///
/// The inverse of @ref outcome_receiver: that turns a completion into an
/// @ref outcome, this turns an @ref outcome back into a completion. Both
/// exist so that the three channels can be reasoned about as data in the
/// middle of an adapter and still leave as real completions.
template <typename Core, typename Receiver>
auto complete_with(Receiver &&r, outcome<Core> o) noexcept -> void {
    switch (o.kind) {
    case channel::value:
        sender_v::set_value(std::forward<Receiver>(r), std::move(o.val));
        return;
    case channel::error:
        sender_v::set_error(std::forward<Receiver>(r), o.err);
        return;
    case channel::stopped:
        sender_v::set_stopped(std::forward<Receiver>(r));
        return;
    }
}

} // namespace detail

// cae50b27-0d34-4779-b73f-7d625d1060d8
/// The receiver that makes `unwind-protect` an *adapter* rather than a
/// special case in the evaluator.
///
/// Decision D5 says cleanup runs on **all three** completion channels, and
/// this is that sentence as code: @c set_value, @c set_error and
/// @c set_stopped are three separate functions that immediately funnel into
/// one @ref finish.
///
/// It is worth being clear about what this buys, because the closure
/// backends already get the same guarantee by a different route.  In
/// `closure/eval_direct.hpp` and `closure/cps_code.hpp` a value, an ordinary
/// error, a `return-from` unwind and a `throw` unwind are *already* merged
/// into one @c result before `core_unwind_protect` sees them, so "run the
/// cleanups on every path" is a single unconditional loop -- they achieve
/// restore-on-every-path by arranging that there is only one path.  A sender
/// backend cannot inherit that from C++ control flow: the three channels are
/// genuinely three functions, called from three places inside the child
/// operation.  So the property has to be *reconstructed*, and reconstructing
/// it is the whole substance of porting `unwind-protect` to senders.  Having
/// each channel do nothing but call @ref finish is how the reconstruction
/// stays checkable by eye.
///
/// ANSI CL's precedence rule is preserved: a cleanup that itself completes
/// abnormally supersedes whatever was in flight; a cleanup that completes
/// normally has its value discarded and the original completion continues.
///
/// @tparam Core     The core AST type.
/// @tparam Cleanup  A nullary invocable returning @c outcome<Core>, which
///                   runs the cleanup forms.
/// @tparam Receiver The downstream receiver.
template <typename Core, typename Cleanup, typename Receiver>
struct unwind_protect_receiver {
    using receiver_concept = sender_v::receiver_tag;

    Cleanup cleanup;   ///< Runs the cleanup forms.
    Receiver receiver; ///< Downstream.

    /// The protected form produced a value.
    auto set_value(closure::value<Core> v) && noexcept -> void {
        std::move(*this).finish(make_value<Core>(std::move(v)));
    }
    /// The protected form was diagnosed.
    auto set_error(smd::smdscheme::foundation::parse_error e) && noexcept
        -> void {
        std::move(*this).finish(make_error<Core>(e));
    }
    /// The protected form is unwinding (`return-from` or `throw`).
    auto set_stopped() && noexcept -> void {
        std::move(*this).finish(make_stopped<Core>());
    }

    /// Runs the cleanup, then completes downstream with whichever of the two
    /// outcomes ANSI CL says wins.
    ///
    /// @param in_flight The protected form's completion, as data.
    auto finish(outcome<Core> in_flight) && noexcept -> void {
        outcome<Core> const cleaned = cleanup();
        detail::complete_with<Core>(std::move(receiver),
                                    cleaned.is_value() ? std::move(in_flight)
                                                       : cleaned);
    }
};

/// The operation state of an @ref unwind_protect_sender.
///
/// Holds the child operation by value and is immovable, so that the child's
/// operation state -- which holds a pointer to the @ref
/// unwind_protect_receiver stored inside it -- is never relocated after
/// @c connect.
///
/// @tparam Core     The core AST type.
/// @tparam Child    The protected form's sender type.
/// @tparam Cleanup  A nullary invocable returning @c outcome<Core>.
/// @tparam Receiver The downstream receiver.
template <typename Core, typename Child, typename Cleanup, typename Receiver>
struct unwind_protect_state {
    using operation_state_concept = sender_v::operation_state_tag;
    using inner_receiver_t = unwind_protect_receiver<Core, Cleanup, Receiver>;

    /// Connects the protected form to the cleanup-running receiver.
    unwind_protect_state(Child child, Cleanup cleanup, Receiver receiver)
        : inner_(sender_v::connect(
              std::move(child),
              inner_receiver_t{std::move(cleanup), std::move(receiver)})) {}

    unwind_protect_state(unwind_protect_state const &) = delete;
    auto operator=(unwind_protect_state const &)
        -> unwind_protect_state & = delete;

    /// Starts the protected form.  Everything else happens on the way out.
    auto start() & noexcept -> void { sender_v::start(inner_); }

  private:
    sender_v::connect_result_t<Child, inner_receiver_t> inner_;
};

/// A sender that runs @p cleanup when its child completes, on **every**
/// completion channel.
///
/// @tparam Core    The core AST type.
/// @tparam Child   The protected form's sender type.
/// @tparam Cleanup A nullary invocable returning @c outcome<Core>.
template <typename Core, typename Child, typename Cleanup>
struct unwind_protect_sender {
    using sender_concept = sender_v::sender_tag;

    Child child;     ///< The protected form.
    Cleanup cleanup; ///< The cleanup forms.

    /// Reports the fixed backend-wide completion set (@ref lisp_completions).
    ///
    /// The child always has exactly these three completions too, so the
    /// adapter forwards rather than transforms: a cleanup can change *which*
    /// channel fires, never which channels are possible.
    template <typename Self, typename... Env>
    static consteval auto get_completion_signatures() {
        return lisp_completions<Core>{};
    }

    /// Connects downstream, wrapping @p r in an @ref unwind_protect_receiver.
    template <typename Receiver>
    auto connect(
        Receiver r) && -> unwind_protect_state<Core, Child, Cleanup, Receiver> {
        return unwind_protect_state<Core, Child, Cleanup, Receiver>(
            std::move(child), std::move(cleanup), std::move(r));
    }
};

/// Builds an @ref unwind_protect_sender protecting @p child with @p cleanup.
///
/// @tparam Core    The core AST type.
/// @param  child   The protected form's sender.
/// @param  cleanup A nullary invocable returning @c outcome<Core>.
template <typename Core, typename Child, typename Cleanup>
[[nodiscard]] auto unwind_protect(Child child, Cleanup cleanup)
    -> unwind_protect_sender<Core, std::decay_t<Child>, std::decay_t<Cleanup>> {
    return unwind_protect_sender<Core, std::decay_t<Child>,
                                 std::decay_t<Cleanup>>{std::move(child),
                                                        std::move(cleanup)};
}
// cae50b27-0d34-4779-b73f-7d625d1060d8 end

} // namespace smd::smdlisp::sender

#endif
