// src/smd/cl/foundation/result_instances.hpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// New in this tree: the designated adapter location for result's typeclass
// instances (per docs/CODING_RULES.md's Typeclass Design rules, datatype and
// adaptation are separate concerns). The result applicative is the effect
// decision D15 makes the elaborator traverse over.
#ifndef SRC_SMD_CL_FOUNDATION_RESULT_INSTANCES_HPP
#define SRC_SMD_CL_FOUNDATION_RESULT_INSTANCES_HPP

#include <smd/cl/foundation/applicative.hpp>
#include <smd/cl/foundation/functor.hpp>
#include <smd/cl/foundation/result.hpp>

#include <functional>
#include <type_traits>
#include <utility>

namespace smd::cl::foundation {

/// Functor @c Impl for @ref result: maps the success value, passes an error
/// through unchanged.
struct result_functor_impl {
    template <class F, class T>
    constexpr auto fmap(this auto &&, F &&f, result<T> const &r) {
        using B = std::remove_cvref_t<std::invoke_result_t<F &, T const &>>;
        if (r.has_value()) {
            return result<B>{std::invoke(std::forward<F>(f), r.value())};
        }
        return result<B>{r.error()};
    }
};

/// Functor instance map for @ref result.
struct result_functor_map : functor<result_functor_impl> {
    using result_functor_impl::fmap;
};

/// Applicative @c Impl for @ref result.
///
/// @c pure embeds a value as success. @c apply combines two results:
/// the leftmost error wins, and only if both operands succeed is the
/// function applied. Both operands are already-evaluated values, so this
/// discards after an error but cannot prevent the work that produced the
/// operands — stopping early is @c fold_left_short's job
/// (<smd/cl/foundation/fold_left_short.hpp>).
struct result_applicative_impl {
    template <class V>
    constexpr auto pure(this auto &&, V &&value)
        -> result<std::remove_cvref_t<V>> {
        return result<std::remove_cvref_t<V>>{std::forward<V>(value)};
    }

    template <class FR, class AR>
    constexpr auto apply(this auto &&, FR const &function_result,
                         AR const &argument_result) {
        using B = std::remove_cvref_t<std::invoke_result_t<
            typename FR::value_type const &, typename AR::value_type const &>>;
        if (!function_result.has_value()) {
            return result<B>{function_result.error()};
        }
        if (!argument_result.has_value()) {
            return result<B>{argument_result.error()};
        }
        return result<B>{
            std::invoke(function_result.value(), argument_result.value())};
    }
};

/// Applicative instance map for @ref result.
struct result_applicative_map : applicative<result_applicative_impl> {
    using result_applicative_impl::apply;
    using result_applicative_impl::pure;
};

/// Registers the Functor instance for @ref result.
template <class T>
inline constexpr auto functor_typeclass<result<T>> = result_functor_map{};

/// Registers the Applicative instance for @ref result.
template <class T>
inline constexpr auto applicative_typeclass<result<T>> =
    result_applicative_map{};

} // namespace smd::cl::foundation

#endif
