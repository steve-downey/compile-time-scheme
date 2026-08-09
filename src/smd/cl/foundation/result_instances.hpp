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
/// @c bind delegates to @ref and_then, which is this operation under the
/// datatype's own name and predates the typeclass. Keeping one
/// implementation means the existing @c and_then call sites in the reader,
/// elaborator and evaluator continue to name the same code; moving the
/// definition here, so that the datatype header stops carrying its own
/// adaptation, is a separate change (see
/// docs/backlog/BL-0004-monad-typeclass.md).
struct result_monad_impl {
    template <class V>
    constexpr auto pure(this auto &&, V &&value)
        -> result<std::remove_cvref_t<V>> {
        return result<std::remove_cvref_t<V>>{std::forward<V>(value)};
    }

    template <class T, class F>
    constexpr auto bind(this auto &&, result<T> const &step, F &&f) {
        return and_then(step, std::forward<F>(f));
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
