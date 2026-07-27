// src/smd/smdlisp/sender/outcome.hpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_SENDER_OUTCOME_HPP
#define SRC_SMD_SMDLISP_SENDER_OUTCOME_HPP

#include <smd/smdlisp/closure/value.hpp>
#include <smd/smdlisp/sender/sender_v.hpp>
#include <smd/smdscheme/foundation/parse_error.hpp>
#include <smd/smdscheme/foundation/result.hpp>

#include <utility>

namespace smd::smdlisp::sender {

// 5c044086-ef0c-4041-8f39-b89b92cde079
/// Which of a sender's three completion channels fired.
///
/// **This enum is the whole difference between the sender backend and the two
/// closure backends**, and it is worth being precise about what changed.
///
/// `closure::eval_direct` and `closure::cps_code` both return a single
/// @ref smd::smdscheme::foundation::result, so *four* different things --  an
/// ordinary value, a diagnosed error, a `return-from` unwind and a `throw`
/// unwind -- have to travel down one wire.  Telling them apart needs an
/// out-of-band discriminator, which is exactly what `closure/env.hpp`'s
/// @c block_unwind_marker and @c throw_unwind_marker are: sentinel
/// `parse_error` messages compared by *pointer identity*, which in turn is
/// why they have to be named `inline constexpr char[]` objects rather than
/// bare string literals (see that header's docs, and step L14's bug).
///
/// A sender has three completion channels natively, so the sentinels are not
/// ported here -- they are **deleted**.  The mapping (decision D5) is:
///
///  - an ordinary value            -> @c set_value(value<Core>)
///  - a diagnosed error            -> @c set_error(parse_error)
///  - a `return-from`/`throw` unwind in flight -> @c set_stopped()
///
/// Nothing is multiplexed, so nothing needs discriminating: an unwind cannot
/// be mistaken for an error because it does not arrive on the error channel.
/// What the target `block`/`catch` still needs to know -- "is this stop aimed
/// at *me*?" -- is answered exactly as the closure backends already answer it,
/// by testing @c live on its own @ref closure::exit_record /
/// @ref closure::catch_record.  The unwind's payload likewise stays where it
/// already lives, in that record; @c set_stopped() carries no value and does
/// not need to.
///
/// The two cases D5 calls out as *not* unwinds stay on the error channel and
/// are diagnosed, never UB: an uncaught `throw` (no live frame matches the
/// tag) and a `return-from` naming a block whose extent has ended.
enum class channel {
    value,  ///< @c set_value fired; @ref outcome::val is meaningful.
    error,  ///< @c set_error fired; @ref outcome::err is meaningful.
    stopped ///< @c set_stopped fired; the in-flight unwind's payload lives
            ///< in the targeted exit/catch record, not here.
};

/// The observed completion of one evaluation, as recorded by
/// @ref outcome_receiver.
///
/// This is the sender backend's carrier type -- the analogue of
/// @c result<value<Core>> in the closure backends, widened by exactly one
/// channel.  It is a plain aggregate rather than a variant so that the
/// stopped case can be represented without inventing a payload for it.
///
/// @tparam Core The core AST type.
template <typename Core>
struct outcome {
    channel kind{channel::value};                  ///< Which channel fired.
    closure::value<Core> val{};                    ///< Valid iff @c kind is
                                                   ///< @c channel::value.
    smd::smdscheme::foundation::parse_error err{}; ///< Valid iff @c kind is
                                                   ///< @c channel::error.

    /// True iff this completion arrived on the value channel.
    [[nodiscard]] constexpr auto is_value() const -> bool {
        return kind == channel::value;
    }
    /// True iff this completion arrived on the error channel.
    [[nodiscard]] constexpr auto is_error() const -> bool {
        return kind == channel::error;
    }
    /// True iff this completion arrived on the stopped channel, i.e. a
    /// nonlocal exit is in flight.
    [[nodiscard]] constexpr auto is_stopped() const -> bool {
        return kind == channel::stopped;
    }
};

/// Builds a value-channel @ref outcome carrying @p v.
template <typename Core>
[[nodiscard]] constexpr auto make_value(closure::value<Core> v)
    -> outcome<Core> {
    return outcome<Core>{channel::value, std::move(v), {}};
}

/// Builds an error-channel @ref outcome carrying @p e.
template <typename Core>
[[nodiscard]] constexpr auto
make_error(smd::smdscheme::foundation::parse_error e) -> outcome<Core> {
    return outcome<Core>{channel::error, {}, e};
}

/// Builds a stopped-channel @ref outcome: a nonlocal exit is in flight, and
/// its payload is in the exit/catch record it targets.
template <typename Core>
[[nodiscard]] constexpr auto make_stopped() -> outcome<Core> {
    return outcome<Core>{channel::stopped, {}, {}};
}

/// Lifts a closure-backend @c result into an @ref outcome, mapping a failed
/// result onto the error channel.
///
/// Used at the boundaries where the sender backend reuses code the closure
/// backends already own and that predates the third channel -- @ref
/// closure::apply_prim, @ref closure::env::lookup_value, @ref
/// closure::detail::bind_lambda_parameters.  None of those can produce an
/// unwind, so the two-channel-to-three-channel widening is total and lossless
/// here.
template <typename Core>
[[nodiscard]] constexpr auto
from_result(smd::smdscheme::foundation::result<closure::value<Core>> const &r)
    -> outcome<Core> {
    if (r.has_value())
        return make_value<Core>(r.value());
    return make_error<Core>(r.error());
}

/// Projects an @ref outcome back onto a closure-backend @c result, for
/// callers (tests, @ref sender_program) that want the two backends to be
/// compared with one @c REQUIRE.
///
/// A stopped completion that reaches the top of a program is an unwind with
/// no target left to catch it. The closure backends cannot produce that state
/// at top level either -- `block`/`catch` resolve their own unwinds, and an
/// unwind naming nothing live is already diagnosed at the `return-from`/
/// `throw` site -- so it is reported as a diagnosed error rather than being
/// silently flattened into a value.
template <typename Core>
[[nodiscard]] constexpr auto to_result(outcome<Core> const &o)
    -> smd::smdscheme::foundation::result<closure::value<Core>> {
    using Res = smd::smdscheme::foundation::result<closure::value<Core>>;
    switch (o.kind) {
    case channel::value:
        return Res{o.val};
    case channel::error:
        return Res{o.err};
    case channel::stopped:
        break;
    }
    return Res{smd::smdscheme::foundation::parse_error{
        {}, "nonlocal exit escaped the top level"}};
}
// 5c044086-ef0c-4041-8f39-b89b92cde079 end

// 6ffe3840-6f72-435d-b975-b945cd4e46f2
/// A receiver that records which completion channel fired, instead of doing
/// anything with it.
///
/// This is the sender backend's *only* driver, and it is deliberately not
/// @c sync_wait.  @c sync_wait collapses the three channels back into two:
/// it returns an engaged @c optional for a value, a disengaged one for
/// stopped, and it **throws** on @c set_error.  A backend whose entire thesis
/// is that the three channels are distinct cannot observe its own
/// completions through an adapter that merges two of them into an exception,
/// and this project does not use exceptions for diagnosed errors anywhere
/// else either.  @c connect + @c start with a recording receiver is both
/// simpler and the honest primitive.
///
/// @tparam Core The core AST type.
template <typename Core>
struct outcome_receiver {
    using receiver_concept = sender_v::receiver_tag;

    outcome<Core> *out; ///< Where the observed completion is written.

    /// Records a value completion.
    auto set_value(closure::value<Core> v) && noexcept -> void {
        *out = make_value<Core>(std::move(v));
    }
    /// Records an error completion.
    auto set_error(smd::smdscheme::foundation::parse_error e) && noexcept
        -> void {
        *out = make_error<Core>(e);
    }
    /// Records a stopped completion, i.e. an unwind reaching this driver.
    auto set_stopped() && noexcept -> void { *out = make_stopped<Core>(); }
};

/// Connects @p s to an @ref outcome_receiver, starts it, and returns the
/// completion it observed.
///
/// Every sender the evaluator builds completes synchronously and inline, so
/// the operation state can live on the stack for the duration of this call
/// and the recorded outcome is guaranteed to have been written by the time
/// @c start returns.
///
/// @tparam Core The core AST type.
/// @param  s    The sender to run.
/// @return Which of the three channels @p s completed on.
template <typename Core, typename S>
[[nodiscard]] auto run_sender(S &&s) -> outcome<Core> {
    outcome<Core> observed{};
    auto op = sender_v::connect(std::forward<S>(s),
                                outcome_receiver<Core>{&observed});
    sender_v::start(op);
    return observed;
}
// 6ffe3840-6f72-435d-b975-b945cd4e46f2 end

} // namespace smd::smdlisp::sender

#endif
