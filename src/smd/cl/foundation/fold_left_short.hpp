// src/smd/cl/foundation/fold_left_short.hpp                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// New in this tree: the short-circuiting fold that decision D15 rests on.
// std::ranges::fold_left cannot stop early; the elaborator's error
// propagation needs a fold that does.
#ifndef SRC_SMD_CL_FOUNDATION_FOLD_LEFT_SHORT_HPP
#define SRC_SMD_CL_FOUNDATION_FOLD_LEFT_SHORT_HPP

#include <concepts>
#include <functional>
#include <ranges>
#include <type_traits>
#include <utility>

namespace smd::cl::foundation {

/// An effect type a fold can stop on: it can be asked whether it succeeded,
/// and carries a value on success and an error otherwise.
/// @ref result models this.
///
/// This is deliberately a structural constraint and not @c monad
/// (<smd/cl/foundation/monad.hpp>). Asking an effect whether it has already
/// failed is exactly the operation a Monad does not have, and exactly the
/// one a strict @c foldM needs; see @ref fold_left_short.
template <class E>
concept short_circuit_effect = requires(E const &e) {
    { e.has_value() } -> std::convertible_to<bool>;
    e.value();
    e.error();
};

/// Short-circuiting left fold.
///
/// Applies @p f to an accumulator and each element of @p range in order,
/// where each step returns an effect (e.g. @c result<Acc>). The first
/// failed step is returned immediately and no later element is visited —
/// this early exit is the difference from @c std::ranges::fold_left and
/// from @c traverse, both of which visit every element.
///
/// This is @c foldM, written as a loop over @ref short_circuit_effect
/// rather than derived from @c bind. That is a deliberate choice against
/// the classical definition
/// `foldM f z xs = foldr (\x k z -> f z x >>= k) return xs z`, and the
/// reason is strictness rather than taste: @c bind skips the *work* after a
/// failure, because it never invokes its function, but it does not stop the
/// *iteration*. Haskell's `foldM` stops the traversal only because `>>=` is
/// lazy in `k` — the unevaluated tail of the whole fold is its second
/// argument. Under strict evaluation something has to ask the effect
/// whether it has already failed, and only the fold is in a position to act
/// on the answer.
///
/// Materializing that `foldr`'s accumulated continuation is not possible
/// here: `k`'s type changes at every step, so a runtime-length range would
/// need type erasure — the same limit @ref foldable records for not deriving
/// @c fold_right from @c fold_map.
///
/// Naming the continuation recursively instead *does* work. It is
/// monomorphic, uses only @c bind, needs no @c has_value anywhere, and is
/// drop-in: substituting it for this loop compiles and constexpr-evaluates
/// the whole pipeline, every call site included. It is rejected on cost
/// rather than on feasibility. Measured under GCC 16 with the default
/// 512-frame `-fconstexpr-depth`, over the reader/elaborator/evaluator
/// pipeline: this loop holds a flat 60 frames whatever the input, while the
/// recursive form spends 8 frames per element and reaches 306 — 60% of the
/// budget — on a 32-element list. Both fit today, so this is a margin
/// argument and not a correctness one: it keeps a constant where the
/// alternative scales with user data, against a ceiling that is a compiler
/// flag the library does not control.
/// docs/backlog/BL-0004-monad-typeclass.md carries the full measurement.
///
/// @tparam Range An input range.
/// @tparam Acc   Accumulator type.
/// @tparam F     Callable with signature @c Effect(Acc, Element) where
///               @c Effect models @ref short_circuit_effect and is
///               constructible from @c Acc.
/// @param  range The elements to fold, visited left to right.
/// @param  init  The initial accumulator value.
/// @param  f     The effectful step function.
/// @return The first failed step's effect, or a successful effect holding
///         the final accumulator.
// 274e7d74-a6be-421c-8ecf-d0b4aeba74f6
template <std::ranges::input_range Range, class Acc, class F>
    requires short_circuit_effect<std::remove_cvref_t<std::invoke_result_t<
                 F &, Acc, std::ranges::range_reference_t<Range const>>>> &&
             std::constructible_from<
                 std::remove_cvref_t<std::invoke_result_t<
                     F &, Acc, std::ranges::range_reference_t<Range const>>>,
                 Acc>
constexpr auto fold_left_short(Range const &range, Acc init, F f) {
    using effect_type = std::remove_cvref_t<std::invoke_result_t<
        F &, Acc, std::ranges::range_reference_t<Range const>>>;
    for (auto const &element : range) { // substrate generic algorithm
        auto step = std::invoke(f, std::move(init), element);
        if (!step.has_value()) {
            return step;
        }
        init = step.value();
    }
    return effect_type{std::move(init)};
}
// 274e7d74-a6be-421c-8ecf-d0b4aeba74f6 end

} // namespace smd::cl::foundation

#endif
