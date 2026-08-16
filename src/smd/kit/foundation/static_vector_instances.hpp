// src/smd/kit/foundation/static_vector_instances.hpp                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Extracted in step R8 from smd::cl::foundation::static_vector_instances,
// moved alongside foldable/traversable/monoid/identity once decision 2 was
// corrected (docs/compiler_architecture.org § "The kit: a real second
// target with one real client"). static_vector and its Functor/Foldable/
// Traversable typeclasses now all live in this namespace, so the
// specializations below are ordinary same-namespace specializations — the
// initial extraction's workaround (reopening smd::kit::foundation from a
// file that stayed under cl to name a cl-qualified type) is gone, not
// merely hidden, because the type moved rather than just the typeclasses.
//
// Traversal order contract for every instance in this header: elements are
// visited in index order, first to last. For traverse, effects are sequenced
// in that same order.
#ifndef SRC_SMD_KIT_FOUNDATION_STATIC_VECTOR_INSTANCES_HPP
#define SRC_SMD_KIT_FOUNDATION_STATIC_VECTOR_INSTANCES_HPP

#include <smd/kit/foundation/applicative.hpp>
#include <smd/kit/foundation/foldable.hpp>
#include <smd/kit/foundation/functor.hpp>
#include <smd/kit/foundation/static_vector.hpp>
#include <smd/kit/foundation/traversable.hpp>

#include <functional>
#include <type_traits>
#include <utility>

namespace smd::kit::foundation {

/// Functor @c Impl for @ref static_vector: maps @p f over the elements in
/// index order, preserving capacity and length.
struct static_vector_functor_impl {
    template <class F, class T, int Capacity>
    constexpr auto fmap(this auto &&, F &&f,
                        static_vector<T, Capacity> const &values) {
        using B = std::remove_cvref_t<std::invoke_result_t<F &, T const &>>;
        static_vector<B, Capacity> mapped;
        for (auto const &element : values) { // substrate generic algorithm
            mapped.push_back(std::invoke(f, element));
        }
        return mapped;
    }
};

/// Functor instance map for @ref static_vector.
struct static_vector_functor_map : functor<static_vector_functor_impl> {
    using static_vector_functor_impl::fmap;
};

/// Foldable @c Impl for @ref static_vector.
///
/// Traversal order: @c fold_map combines images in index order, first to
/// last; @c fold_right combines from the last element back to the first.
struct static_vector_foldable_impl {
    template <class F, class T, int Capacity, class M>
    constexpr auto fold_map(this auto &&, F &&f,
                            static_vector<T, Capacity> const &values,
                            M const &m) {
        auto acc = m.empty();
        for (auto const &element : values) { // substrate generic algorithm
            acc = m.combine(std::move(acc), std::invoke(f, element));
        }
        return acc;
    }

    template <class F, class Acc, class T, int Capacity>
    constexpr auto fold_right(this auto &&, F &&f, Acc init,
                              static_vector<T, Capacity> const &values) -> Acc {
        // substrate generic algorithm: reverse traversal is this instance's
        // primitive (see foldable.hpp for why it is not derived).
        for (int i = values.size() - 1; i >= 0; --i) {
            init = f(values[i], std::move(init));
        }
        return init;
    }
};

/// Foldable instance map for @ref static_vector.
struct static_vector_foldable_map : foldable<static_vector_foldable_impl> {
    using static_vector_foldable_impl::fold_map;
    using static_vector_foldable_impl::fold_right;
};

/// Traversable @c Impl for @ref static_vector.
///
/// @c traverse applies the effectful @p f to every element in index order
/// (the documented Foldable order), sequencing effects left to right
/// through the effect's registered Applicative instance, and rebuilds a
/// vector of the same shape inside the effect. Every element is visited;
/// the effect's @c apply decides what surviving values look like (for
/// @c result, the leftmost error wins). For error propagation that must
/// stop visiting early, use @c fold_left_short instead.
///
/// The effect type must name its carried type as @c value_type and have a
/// registered @c applicative_typeclass instance.
struct static_vector_traversable_impl {
    template <class F, class T, int Capacity>
    constexpr auto traverse(this auto &&, F &&f,
                            static_vector<T, Capacity> const &values) {
        using effect_type =
            std::remove_cvref_t<std::invoke_result_t<F &, T const &>>;
        using B = typename effect_type::value_type;
        auto const &tc = applicative_typeclass<effect_type>;
        auto accumulated = tc.pure(static_vector<B, Capacity>{});
        auto append = [](static_vector<B, Capacity> collected, B element) {
            collected.push_back(std::move(element));
            return collected;
        };
        for (auto const &element : values) { // substrate generic algorithm
            accumulated = tc.invoke(append, std::move(accumulated),
                                    std::invoke(f, element));
        }
        return accumulated;
    }
};

/// Traversable instance map for @ref static_vector.
struct static_vector_traversable_map
    : traversable<static_vector_traversable_impl> {
    using static_vector_traversable_impl::traverse;
};

// 51f97b89-0923-4b8f-94f1-166b79f6d70d
/// Registers the Functor instance for @ref static_vector.
template <class T, int Capacity>
inline constexpr auto functor_typeclass<static_vector<T, Capacity>> =
    static_vector_functor_map{};

/// Registers the Foldable instance for @ref static_vector.
template <class T, int Capacity>
inline constexpr auto foldable_typeclass<static_vector<T, Capacity>> =
    static_vector_foldable_map{};

/// Registers the Traversable instance for @ref static_vector.
template <class T, int Capacity>
inline constexpr auto traversable_typeclass<static_vector<T, Capacity>> =
    static_vector_traversable_map{};
// 51f97b89-0923-4b8f-94f1-166b79f6d70d end

} // namespace smd::kit::foundation

#endif
