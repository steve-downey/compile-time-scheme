// src/smd/cl/foundation/topo_fold.hpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// New in this tree: the topological folds. A column whose elements name
// only earlier positions is a recursive structure that has already been
// flattened, and a fold over it in index order is the recursion — no
// stack, no visitor, no bound on nesting depth. The tree that stores
// itself this way arrives two steps from now; the substrate commits to
// the shape first.
#ifndef SRC_SMD_CL_FOUNDATION_TOPO_FOLD_HPP
#define SRC_SMD_CL_FOUNDATION_TOPO_FOLD_HPP

#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/static_vector.hpp>

#include <algorithm>
#include <cassert>
#include <concepts>
#include <functional>
#include <ranges>
#include <type_traits>

namespace smd::cl::foundation {

/// Ascending topological fold: computes one result per element of a
/// column in which an element may name only earlier positions.
///
/// For each position in order, the algebra is applied to the results
/// computed so far and the element at that position, and its value is
/// appended:
///
///     done.push_back(alg(done, layers[i]))
///
/// An element's children are wherever the caller's layer type says they
/// are; the contract is only that every position an element names is
/// less than its own, so by the time the algebra runs, `done[child]` is
/// final. The lookup is O(1) random access into the fold's own
/// accumulator — never a recursive call — which is why nothing here
/// needs a stack. Returns the full results column; the caller knows
/// which entry is the root.
///
/// @tparam A        The algebra's carrier (explicit; it cannot be
///                  deduced, because the algebra's first argument is the
///                  column of @p A being built).
/// @tparam Capacity The result column's capacity (explicit; the input
///                  range does not carry one, per D14).
/// @tparam Layers   An input range of layers.
/// @tparam Alg      Callable: `A(static_vector<A, Capacity> const&,
///                  layer)`.
/// @pre the range's length is <= @p Capacity
/// @pre every position an element names is less than its own
// 75e695f0-6275-4829-821e-b1b16566d78e
template <class A, int Capacity, std::ranges::input_range Layers, class Alg>
    requires std::convertible_to<
        std::invoke_result_t<Alg &, static_vector<A, Capacity> const &,
                             std::ranges::range_reference_t<Layers const>>,
        A>
[[nodiscard]] constexpr auto fold_up(Layers const &layers, Alg alg)
    -> static_vector<A, Capacity> {
    static_vector<A, Capacity> done;
    for (auto const &layer : layers) { // substrate generic algorithm
        done.push_back(std::invoke(alg, done, layer));
    }
    return done;
}
// 75e695f0-6275-4829-821e-b1b16566d78e end

/// Short-circuiting ascending topological fold: the algebra returns
/// `result<A>`, the first failure is returned immediately, and no later
/// element is visited.
///
/// The early exit is the difference from @ref fold_up, exactly as
/// @ref fold_left_short's early exit is its difference from a plain
/// fold: use this where later work must be skipped, not merely
/// discarded — an arena being filled has no room for values that are
/// already doomed.
///
/// @tparam A        The algebra's carrier (explicit).
/// @tparam Capacity The result column's capacity (explicit).
/// @tparam Layers   An input range of layers.
/// @tparam Alg      Callable: `result<A>(static_vector<A, Capacity>
///                  const&, layer)`.
/// @return The full results column, or the leftmost failure.
/// @pre as @ref fold_up
template <class A, int Capacity, std::ranges::input_range Layers, class Alg>
    requires std::same_as<std::remove_cvref_t<std::invoke_result_t<
                              Alg &, static_vector<A, Capacity> const &,
                              std::ranges::range_reference_t<Layers const>>>,
                          result<A>>
[[nodiscard]] constexpr auto fold_up_short(Layers const &layers, Alg alg)
    -> result<static_vector<A, Capacity>> {
    static_vector<A, Capacity> done;
    for (auto const &layer : layers) { // substrate generic algorithm
        auto step = std::invoke(alg, done, layer);
        if (!step.has_value()) {
            return step.error();
        }
        done.push_back(std::move(step).value());
    }
    return done;
}

/// Descending topological fold: computes one result per element of a
/// parent column, where each element's value may depend on its parent's.
///
/// This is @ref fold_up's dual. An ascending fold computes synthesized
/// values, children first; a descending fold computes inherited ones,
/// parents first. The parent column names each element's parent by
/// position (or -1 for none), and every parent's position exceeds its
/// children's — the same children-before-parent contract read the other
/// way — so descending order finalizes every parent before any child
/// asks:
///
///     done[i] = step(parent(i) < 0 ? nullptr : &done[parent(i)], i)
///
/// The step receives the element's own position because an inherited
/// value usually depends on where the element sits, not only on what its
/// parent concluded; the caller's other columns are a capture away.
///
/// Unlike @ref fold_up's, this precondition is checkable here and is
/// checked: a parent column is a range of positions, so the fold can see
/// what it requires. @ref fold_up is handed layers whose child positions
/// only the algebra knows how to find, so its own precondition can not be
/// stated in terms of anything it holds — which is why the tree that
/// satisfies it carries the check instead
/// (@c tagged_tree::is_children_before_parent).
///
/// @tparam A        The step's carrier (explicit; default-constructible,
///                  because the column is materialized before it is
///                  filled).
/// @tparam Capacity The result column's capacity (explicit).
/// @tparam Parents  A random-access range of integral parent positions.
/// @tparam Step     Callable: `A(A const* parent, int index)`; the
///                  pointer is null where there is no parent.
/// @pre the range's length is <= @p Capacity
/// @pre every parent position exceeds its element's own, or is negative
// 82d064a1-82e1-40f3-8ba7-3be54fe483c2
template <class A, int Capacity, std::ranges::random_access_range Parents,
          class Step>
    requires std::integral<std::remove_cvref_t<
                 std::ranges::range_reference_t<Parents const>>> &&
             std::convertible_to<std::invoke_result_t<Step &, A const *, int>,
                                 A>
[[nodiscard]] constexpr auto fold_down(Parents const &parents, Step step)
    -> static_vector<A, Capacity> {
    int const count = static_cast<int>(std::ranges::distance(parents));
    assert(std::ranges::all_of(
        std::views::enumerate(parents), [](auto const &entry) {
            auto const &[index, parent] = entry;
            return static_cast<int>(parent) < 0 ||
                   static_cast<int>(parent) > static_cast<int>(index);
        }));
    auto done = static_vector<A, Capacity>::filled(count, A{});
    for (int index = count - 1; index >= 0; --index) { // substrate generic
                                                       // algorithm
        int const parent = static_cast<int>(parents[index]);
        done[index] =
            std::invoke(step, parent < 0 ? nullptr : &done[parent], index);
    }
    return done;
}
// 82d064a1-82e1-40f3-8ba7-3be54fe483c2 end

} // namespace smd::cl::foundation

#endif
