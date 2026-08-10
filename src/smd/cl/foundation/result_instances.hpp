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
#include <smd/cl/foundation/monad.hpp>
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

/// Monad @c Impl for @ref result.
///
/// @c bind applies @p f to the success value, or passes the error through
/// unchanged. This was `foundation::and_then` in `result.hpp` until the
/// typeclass existed to hold it; it lives here because a datatype does not
/// carry its own adaptation (docs/CODING_RULES.md, Typeclass Design).
///
/// Decision D15 rules out the `if (!r.has_value()) return r;` ladder and
/// names two replacements: @c traverse where there is a structure to thread
/// the effect through, and @c fold_left_short where a sequence must stop
/// early. This is the third shape, and the one neither covers — a single
/// step whose input is the previous step's output, with no structure and no
/// sequence.
struct result_monad_impl {
    template <class V>
    constexpr auto pure(this auto &&, V &&value)
        -> result<std::remove_cvref_t<V>> {
        return result<std::remove_cvref_t<V>>{std::forward<V>(value)};
    }

    template <class T, class F>
    constexpr auto bind(this auto &&, result<T> const &step, F &&f) {
        using out_type =
            std::remove_cvref_t<std::invoke_result_t<F &, T const &>>;
        if (!step.has_value()) {
            return out_type{step.error()};
        }
        return std::invoke(std::forward<F>(f), step.value());
    }
};

/// Monad instance map for @ref result.
struct result_monad_map : monad<result_monad_impl> {
    using result_monad_impl::bind;
    using result_monad_impl::pure;
};

/// Applicative @c Impl for @ref result.
///
/// @c pure embeds a value as success. @c apply is monad-derived, per
/// <tt>docs/CODING_RULES.md</tt>'s Semantic Defaults: the leftmost error
/// wins and the function is applied only if both operands succeed, which
/// falls out of nested @c bind rather than being written as branches a
/// second time.
///
/// Both operands are already-evaluated values, so this discards after an
/// error but cannot prevent the work that produced the operands — stopping
/// early is @c fold_left_short's job
/// (<smd/cl/foundation/fold_left_short.hpp>). Deriving @c apply from
/// @c bind does not change that: @c bind can skip work only when it is
/// handed a function not to call, and @c apply is handed two finished
/// values.
struct result_applicative_impl {
    template <class V>
    constexpr auto pure(this auto &&, V &&value)
        -> result<std::remove_cvref_t<V>> {
        return result<std::remove_cvref_t<V>>{std::forward<V>(value)};
    }

    template <class FR, class AR>
    constexpr auto apply(this auto &&, FR const &function_result,
                         AR const &argument_result) {
        return result_monad_map{}.apply(function_result, argument_result);
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

/// Registers the Monad instance for @ref result.
template <class T>
inline constexpr auto monad_typeclass<result<T>> = result_monad_map{};

/// Registers the Applicative instance for @ref result.
template <class T>
inline constexpr auto applicative_typeclass<result<T>> =
    result_applicative_map{};

} // namespace smd::cl::foundation

#endif
