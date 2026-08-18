// src/smd/kit/foundation/result_instances.hpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Extracted in step R8 from smd::cl::foundation::result_instances, moved
// alongside foldable/traversable/monoid/identity once decision 2 was
// corrected (docs/compiler_architecture.org § "The kit: a real second
// target with one real client"). result and its Functor/Applicative
// typeclasses now all live in this namespace, so the specializations below
// are ordinary same-namespace specializations — the initial extraction's
// workaround (reopening smd::kit::foundation from a file that stayed under
// cl to name a cl-qualified type) is gone, not merely hidden, because the
// type moved rather than just the typeclass.
#ifndef SRC_SMD_KIT_FOUNDATION_RESULT_INSTANCES_HPP
#define SRC_SMD_KIT_FOUNDATION_RESULT_INSTANCES_HPP

#include <smd/kit/foundation/applicative.hpp>
#include <smd/kit/foundation/functor.hpp>
#include <smd/kit/foundation/monad.hpp>
#include <smd/kit/foundation/result.hpp>

#include <functional>
#include <type_traits>
#include <utility>

namespace smd::kit::foundation {

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
/// unchanged. This is the body @c and_then carried in @c result.hpp until
/// the typeclass existed to hold it; a datatype does not carry its own
/// typeclass adaptation (docs/CODING_RULES.md, Typeclass Design), so it
/// lives here and @c and_then below is now a spelling of it.
///
/// Decision D15 (docs/cl-rebuild-plan.md) rules out the
/// `if (!r.has_value()) return r;` ladder and names two replacements:
/// @c traverse where there is a structure to thread the effect through, and
/// @c fold_left_short where a sequence must stop early. This is the third
/// shape, the one neither covers — a single step whose input is the previous
/// step's output, with no structure and no sequence.
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

/// Registers the Monad instance for @ref result.
template <class T>
inline constexpr auto monad_typeclass<result<T>> = result_monad_map{};

/// Monadic sequencing for @ref result, under the datatype's own name:
/// applies @p f to the success value of @p step, or passes @p step's error
/// through unchanged.
///
/// This is @ref result_monad_impl's @c bind reached through the @c bind CPO,
/// and exists as a separate name on purpose. A typeclass is what lets the
/// generic operation and the domain API be spelled differently: generic code
/// says @c bind and dispatches on @c monad_typeclass, while a caller holding
/// a concrete @ref result says @c and_then and gets it by argument-dependent
/// lookup, which does not apply to the CPO because a CPO is an object.
///
/// It lives beside the registration rather than in @c result.hpp, and that
/// is a correctness requirement rather than a filing preference: the @c bind
/// call below instantiates the default template argument
/// @c monad_typeclass<result<T>>, which would select the primary
/// @c std::false_type in any translation unit that had not yet seen the
/// specialization above. Specializing a variable template after a use that
/// would have chosen it differently is ill-formed, no diagnostic required.
///
/// @tparam T The step's success type.
/// @tparam F Callable with signature @c result<U>(T const &).
template <class T, class F>
[[nodiscard]] constexpr auto and_then(result<T> const &step, F &&f) {
    return bind(step, std::forward<F>(f));
}

/// Applicative @c Impl for @ref result.
///
/// @c pure embeds a value as success. @c apply is monad-derived, which is
/// what <tt>docs/CODING_RULES.md</tt>'s Semantic Defaults ask for: the
/// leftmost error wins and the function is applied only if both operands
/// succeed, and both of those fall out of nested @c bind rather than being
/// written as branches a second time.
///
/// Both operands are already-evaluated values, so this discards after an
/// error but cannot prevent the work that produced the operands — stopping
/// early is @c fold_left_short's job
/// (<smd/kit/foundation/fold_left_short.hpp>). Deriving @c apply from
/// @c bind does not change that: @c bind skips work only when it is handed a
/// function not to call, and @c apply is handed two finished values.
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

/// Registers the Applicative instance for @ref result.
template <class T>
inline constexpr auto applicative_typeclass<result<T>> =
    result_applicative_map{};

} // namespace smd::kit::foundation

#endif
