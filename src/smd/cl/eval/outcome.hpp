// src/smd/cl/eval/outcome.hpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_EVAL_OUTCOME_HPP
#define SRC_SMD_CL_EVAL_OUTCOME_HPP

#include <smd/cl/eval/value.hpp>
#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/symbol/symbol_id.hpp>

#include <utility>
#include <variant>

namespace smd::cl::eval {

// 8f5b0a1c-3f0e-4f6f-95b0-2a0b4a5f6c31
/// A non-local exit in flight: which exit point it is aimed at, and the
/// value it is carrying there.
///
/// The target is the block's name. "Is this unwind aimed at me?" is asked
/// and answered by the block's own frame, which is where the question
/// belongs — no evaluator-wide register, and no sentinel.
struct unwind {
    symbol::symbol_id target{}; ///< The block being returned from.
    value payload{};            ///< The value it carries.

    // HIDDEN FRIEND
    friend constexpr auto operator==(unwind const &, unwind const &)
        -> bool = default;
};

/// How one evaluation completes: with a value, with a diagnosed error, or
/// with an unwind in flight. Decision D13.
///
/// The three are distinct alternatives of one type, so control flow does
/// not travel in the error channel. The closure backends of the pivot
/// multiplexed all of this onto a two-alternative @c result and told the
/// cases apart with a pair of sentinel @c parse_error messages compared by
/// pointer identity — which is why those sentinels had to be named
/// @c inline @c constexpr @c char[] objects rather than bare literals,
/// since equal string literals need not share an address. None of that
/// machinery is ported. There is nothing to discriminate.
///
/// This is the generalisation of what the sender backend already got right
/// (`docs/compiler_architecture.org`, "A sender has three completion
/// channels, and that is a semantic resource"): a value to @c set_value, an
/// error to @c set_error, an unwind to @c set_stopped. What that section
/// warns must be *reconstructed* rather than inherited is
/// `unwind-protect`'s run-the-cleanup-on-every-exit-path property, because
/// with the channels split there is no longer a single merged exit path for
/// one unconditional statement to sit on. In this machine it is
/// reconstructed by a continuation frame that intercepts all three
/// alternatives and funnels them through the same cleanup —
/// see @c detail::k_protect in <smd/cl/eval/machine.hpp>.
class outcome {
  public:
    /// Completes with @p v.
    constexpr outcome(value v);

    /// Completes with the diagnosed error @p e.
    constexpr outcome(foundation::parse_error e);

    /// Completes with the unwind @p u in flight.
    constexpr outcome(unwind u);

    /// Returns true if this completed with a value.
    [[nodiscard]] constexpr auto is_value() const -> bool;

    /// Returns true if this completed with a diagnosed error.
    [[nodiscard]] constexpr auto is_error() const -> bool;

    /// Returns true if this completed with an unwind in flight.
    [[nodiscard]] constexpr auto is_unwind() const -> bool;

    /// Returns the value.
    /// @pre is_value()
    [[nodiscard]] constexpr auto as_value() const -> value const &;

    /// Returns the error.
    /// @pre is_error()
    [[nodiscard]] constexpr auto as_error() const
        -> foundation::parse_error const &;

    /// Returns the unwind.
    /// @pre is_unwind()
    [[nodiscard]] constexpr auto as_unwind() const -> unwind const &;

    // HIDDEN FRIEND
    friend constexpr auto operator==(outcome const &, outcome const &)
        -> bool = default;

  private:
    std::variant<value, foundation::parse_error, unwind> data_;
};
// 8f5b0a1c-3f0e-4f6f-95b0-2a0b4a5f6c31 end

constexpr outcome::outcome(value v) : data_{std::move(v)} {}

constexpr outcome::outcome(foundation::parse_error e) : data_{e} {}

constexpr outcome::outcome(unwind u) : data_{std::move(u)} {}

constexpr auto outcome::is_value() const -> bool {
    return std::holds_alternative<value>(data_);
}

constexpr auto outcome::is_error() const -> bool {
    return std::holds_alternative<foundation::parse_error>(data_);
}

constexpr auto outcome::is_unwind() const -> bool {
    return std::holds_alternative<unwind>(data_);
}

constexpr auto outcome::as_value() const -> value const & {
    return std::get<value>(data_);
}

constexpr auto outcome::as_error() const -> foundation::parse_error const & {
    return std::get<foundation::parse_error>(data_);
}

constexpr auto outcome::as_unwind() const -> unwind const & {
    return std::get<unwind>(data_);
}

/// Widens a two-alternative @ref foundation::result into the three-channel
/// @ref outcome: the value channel or the error channel, never the unwind
/// one.
///
/// This is the direction that is always safe. The other direction is not a
/// function at all, which is the point of the split: an unwind in flight
/// has nowhere to go in a type that has only value and error, and pretending
/// otherwise is what the sentinel messages were for.
[[nodiscard]] constexpr auto to_outcome(foundation::result<value> const &r)
    -> outcome {
    if (!r.has_value()) {
        return outcome{r.error()};
    }
    return outcome{r.value()};
}

} // namespace smd::cl::eval

#endif
