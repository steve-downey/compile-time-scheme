// src/smd/cl/foundation/foldable.hpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// New in this tree: the Foldable typeclass, required by
// docs/CODING_RULES.md's Foldable rules and present in neither prior copy
// of the foundation.
#ifndef SRC_SMD_CL_FOUNDATION_FOLDABLE_HPP
#define SRC_SMD_CL_FOUNDATION_FOLDABLE_HPP

#include <smd/cl/foundation/monoid.hpp>

#include <type_traits>
#include <utility>

namespace smd::cl::foundation {

/// CRTP base that derives Foldable operations from the @c fold_map primitive.
///
/// @c fold_map is the semantic centre. @c Impl must provide:
/// - @c fold_map(f, container, monoid) — maps each element through @p f into
///   the monoid's carrier and combines the images in the instance's
///   documented traversal order.
/// - @c fold_right(f, init, container) — the right fold, as a primitive.
///   Deriving it from @c fold_map would need either a function-composition
///   monoid (not practical under constexpr with fixed capacities) or a
///   materialized reversal (needs a capacity this generic base cannot know),
///   so it is the one documented exception to "derived where practical".
///
/// Derived here: @c fold_left, @c length, @c to_vector.
///
/// Traversal order is part of the instance contract and must be documented
/// by each instance; the derivations below inherit whatever order the
/// instance's @c fold_map documents.
///
/// @tparam Impl Concrete implementation providing @c fold_map and
///              @c fold_right.
template <class Impl>
struct foldable : protected Impl {
    using Impl::fold_map;
    using Impl::fold_right;

    /// Left fold, derived from @c fold_map: each element updates an
    /// accumulator captured by reference, sequenced by the instance's
    /// documented traversal order under the effect-only @ref unit_monoid.
    ///
    /// @tparam F   Callable with signature @c Acc(Acc, Element).
    /// @tparam Acc Accumulator type.
    /// @tparam T   Container type.
    // 2c04510b-930c-4d2e-be6c-8733f844baa1
    template <class F, class Acc, class T>
    constexpr auto fold_left(this auto &&self, F &&f, Acc init,
                             T const &container) -> Acc {
        self.fold_map(
            [&](auto const &element) {
                init = f(std::move(init), element);
                return unit{};
            },
            container, unit_monoid);
        return init;
    }
    // 2c04510b-930c-4d2e-be6c-8733f844baa1 end

    /// Number of elements, derived from @c fold_map into @ref sum_monoid.
    template <class T>
    [[nodiscard]] constexpr auto length(this auto &&self, T const &container)
        -> int {
        return self.fold_map([](auto const &) { return 1; }, container,
                             sum_monoid);
    }

    /// Copies the elements, in the instance's documented traversal order,
    /// into a default-constructed @p Out (any type with @c push_back).
    /// Derived from @c fold_left.
    ///
    /// @tparam Out The output container type (explicit template argument).
    template <class Out, class T>
    [[nodiscard]] constexpr auto to_vector(this auto &&self, T const &container)
        -> Out {
        return self.fold_left(
            [](Out acc, auto const &element) {
                acc.push_back(element);
                return acc;
            },
            Out{}, container);
    }
};

/// Typeclass lookup variable for Foldable; specialize for each container.
///
/// Default is @c std::false_type{}, producing a compile error if a Foldable
/// operation is called for an unregistered type.
template <class T>
inline constexpr auto foldable_typeclass = std::false_type{};

/// Customization-point object for @c fold_map.
///
/// Deduces the container type from the second argument and dispatches
/// through @c foldable_typeclass<T>. The NTTP @c TC may be pinned explicitly.
struct fold_map_fn {
    /// Maps each element of @p container through @p f and combines the
    /// images with @p m, in the instance's documented traversal order.
    ///
    /// @tparam F  Callable type.
    /// @tparam T  Container type (deduced, used for typeclass lookup).
    /// @tparam M  Monoid instance object type.
    /// @tparam TC Typeclass instance (NTTP, defaults to lookup).
    template <class F, class T, class M,
              const auto &TC = foldable_typeclass<std::remove_cvref_t<T>>>
    constexpr auto operator()(F &&f, T const &container, M const &m) const {
        using tc_type = std::remove_cvref_t<decltype(TC)>;
        return tc_type{}.fold_map(std::forward<F>(f), container, m);
    }
};

/// Global CPO for Foldable's @c fold_map operation.
inline constexpr fold_map_fn fold_map{};

/// Customization-point object for the derived @c fold_left operation.
struct fold_left_fn {
    /// Left-folds @p container with @p f from @p init.
    template <class F, class Acc, class T,
              const auto &TC = foldable_typeclass<std::remove_cvref_t<T>>>
    constexpr auto operator()(F &&f, Acc init, T const &container) const
        -> Acc {
        using tc_type = std::remove_cvref_t<decltype(TC)>;
        return tc_type{}.fold_left(std::forward<F>(f), std::move(init),
                                   container);
    }
};

/// Global CPO for Foldable's @c fold_left operation.
inline constexpr fold_left_fn fold_left{};

/// Customization-point object for the @c fold_right primitive.
struct fold_right_fn {
    /// Right-folds @p container with @p f from @p init.
    template <class F, class Acc, class T,
              const auto &TC = foldable_typeclass<std::remove_cvref_t<T>>>
    constexpr auto operator()(F &&f, Acc init, T const &container) const
        -> Acc {
        using tc_type = std::remove_cvref_t<decltype(TC)>;
        return tc_type{}.fold_right(std::forward<F>(f), std::move(init),
                                    container);
    }
};

/// Global CPO for Foldable's @c fold_right operation.
inline constexpr fold_right_fn fold_right{};

/// Customization-point object for the derived @c length operation.
struct length_fn {
    /// Returns the number of elements in @p container.
    template <class T,
              const auto &TC = foldable_typeclass<std::remove_cvref_t<T>>>
    constexpr auto operator()(T const &container) const -> int {
        using tc_type = std::remove_cvref_t<decltype(TC)>;
        return tc_type{}.length(container);
    }
};

/// Global CPO for Foldable's @c length operation.
inline constexpr length_fn length{};

} // namespace smd::cl::foundation

#endif
