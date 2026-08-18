// src/smd/kit/foundation/identity.hpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Extracted in step R8 from smd::cl::foundation::identity. Decision 2's
// original cut left this in cl on the grounds that neither smdscheme nor
// compile-time-forth had a use for the identity applicative. That reasoning
// was overturned: the absence reflects the algebraic refactoring never
// having been done on the Forth side, not a fact about this code's
// generality. The corrected standard is docs/cl-rebuild-plan.md §5's own
// test — generic in shape and free of language-specific types — which this
// file passes outright. Moving it here also dissolves an awkwardness the
// initial extraction had to work around: functor_typeclass and
// applicative_typeclass already live in this namespace, so identity's own
// instances are now ordinary same-namespace specializations rather than a
// separately-reopened block naming smd::cl::foundation::identity by
// qualification.
#ifndef SRC_SMD_KIT_FOUNDATION_IDENTITY_HPP
#define SRC_SMD_KIT_FOUNDATION_IDENTITY_HPP

#include <smd/kit/foundation/applicative.hpp>
#include <smd/kit/foundation/functor.hpp>
#include <smd/kit/foundation/monad.hpp>

#include <functional>
#include <type_traits>
#include <utility>

namespace smd::kit::foundation {

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

/// Monad @c Impl for @ref identity: there is no failure to skip, so @c bind
/// is function application under the wrapper.
///
/// The trivial monad is worth registering rather than only describing. It
/// gives the laws in <smd/kit/foundation/monad.test.cpp> a second instance
/// to hold against, which is what makes them tests of the typeclass rather
/// than of @ref result, and it is the effect @c traverse already uses to say
/// "no effect at all".
struct identity_monad_impl {
    template <class V>
    constexpr auto pure(this auto &&, V &&value)
        -> identity<std::remove_cvref_t<V>> {
        return identity<std::remove_cvref_t<V>>{std::forward<V>(value)};
    }

    template <class T, class F>
    constexpr auto bind(this auto &&, identity<T> const &id, F &&f) {
        return std::invoke(std::forward<F>(f), id.value);
    }
};

/// Monad instance map for @ref identity.
struct identity_monad_map : monad<identity_monad_impl> {
    using identity_monad_impl::bind;
    using identity_monad_impl::pure;
};

/// Registers the Functor instance for @ref identity.
template <class T>
inline constexpr auto functor_typeclass<identity<T>> = identity_functor_map{};

/// Registers the Monad instance for @ref identity.
template <class T>
inline constexpr auto monad_typeclass<identity<T>> = identity_monad_map{};

/// Registers the Applicative instance for @ref identity.
template <class T>
inline constexpr auto applicative_typeclass<identity<T>> =
    identity_applicative_map{};

} // namespace smd::kit::foundation

#endif
