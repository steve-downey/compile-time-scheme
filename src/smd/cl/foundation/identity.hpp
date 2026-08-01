// src/smd/cl/foundation/identity.hpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// New in this tree: the identity applicative, the trivial effect that the
// Traversable identity law is stated against. This header is the designated
// adapter location for identity's own typeclass instances.
#ifndef SRC_SMD_CL_FOUNDATION_IDENTITY_HPP
#define SRC_SMD_CL_FOUNDATION_IDENTITY_HPP

#include <smd/cl/foundation/applicative.hpp>
#include <smd/cl/foundation/functor.hpp>

#include <functional>
#include <type_traits>
#include <utility>

namespace smd::cl::foundation {

/// The identity effect: a value and nothing else. Traversing a structure
/// with @c identity as the applicative performs no effect at all, which is
/// exactly what the Traversable identity law needs to talk about.
///
/// @tparam T The carried type.
template <class T>
struct identity {
    /// The carried type, named for effect-generic code such as @c traverse
    /// instances.
    using value_type = T;

    T value; ///< The carried value.

    // HIDDEN FRIEND
    friend constexpr auto operator==(identity const &, identity const &)
        -> bool = default;
};

/// Functor @c Impl for @ref identity: applies the function to the value.
struct identity_functor_impl {
    template <class F, class T>
    constexpr auto fmap(this auto &&, F &&f, identity<T> const &id) {
        using B = std::remove_cvref_t<std::invoke_result_t<F &, T const &>>;
        return identity<B>{std::invoke(std::forward<F>(f), id.value)};
    }
};

/// Functor instance map for @ref identity.
struct identity_functor_map : functor<identity_functor_impl> {
    using identity_functor_impl::fmap;
};

/// Applicative @c Impl for @ref identity: @c pure wraps, @c apply applies.
struct identity_applicative_impl {
    template <class V>
    constexpr auto pure(this auto &&, V &&value)
        -> identity<std::remove_cvref_t<V>> {
        return identity<std::remove_cvref_t<V>>{std::forward<V>(value)};
    }

    template <class FI, class AI>
    constexpr auto apply(this auto &&, FI &&function_id, AI &&argument_id) {
        using B = std::remove_cvref_t<std::invoke_result_t<
            decltype((function_id.value)), decltype((argument_id.value))>>;
        return identity<B>{std::invoke(std::forward<FI>(function_id).value,
                                       std::forward<AI>(argument_id).value)};
    }
};

/// Applicative instance map for @ref identity.
struct identity_applicative_map : applicative<identity_applicative_impl> {
    using identity_applicative_impl::apply;
    using identity_applicative_impl::pure;
};

/// Registers the Functor instance for @ref identity.
template <class T>
inline constexpr auto functor_typeclass<identity<T>> = identity_functor_map{};

/// Registers the Applicative instance for @ref identity.
template <class T>
inline constexpr auto applicative_typeclass<identity<T>> =
    identity_applicative_map{};

} // namespace smd::cl::foundation

#endif
