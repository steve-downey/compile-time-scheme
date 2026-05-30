// src/smd/smdscheme/foundation/applicative.hpp                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_FOUNDATION_APPLICATIVE_HPP
#define SRC_SMD_SMDSCHEME_FOUNDATION_APPLICATIVE_HPP

#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace smd::smdscheme::foundation {

namespace detail {

/// A callable that accumulates arguments one at a time and invokes the
/// underlying function when enough have been bound.
///
/// Each call to @c operator() either invokes the function (if enough arguments
/// have been supplied) or returns a new @c terminating_partial with the
/// argument appended to the bound set. This implements Ben Deane's
/// terminating partial-application technique and is used to derive
/// @c invoke from @c pure and @c apply in @ref applicative.
///
/// @tparam Function   The function to be eventually invoked.
/// @tparam BoundArgs  Arguments already bound in this partial application.
template <class Function, class... BoundArgs>
struct terminating_partial {
    Function function;
    std::tuple<BoundArgs...> bound_args;

    /// Accepts the next argument, invoking or extending.
    template <class NextArg>
    constexpr auto operator()(NextArg &&next_arg) {
        return invoke_or_extend(std::forward<NextArg>(next_arg),
                                std::index_sequence_for<BoundArgs...>{});
    }

    /// Const overload for use in const applicative contexts.
    template <class NextArg>
    constexpr auto operator()(NextArg &&next_arg) const {
        return invoke_or_extend_const(std::forward<NextArg>(next_arg),
                                      std::index_sequence_for<BoundArgs...>{});
    }

  private:
    template <class NextArg, std::size_t... Idx>
    constexpr auto invoke_or_extend(NextArg &&next_arg,
                                    std::index_sequence<Idx...>) {
        if constexpr (std::invocable<Function &, BoundArgs &..., NextArg>) {
            return std::invoke(function, std::get<Idx>(bound_args)...,
                               std::forward<NextArg>(next_arg));
        } else {
            using next_partial =
                terminating_partial<Function, BoundArgs...,
                                    std::remove_cvref_t<NextArg>>;
            return next_partial{
                function,
                std::tuple_cat(std::move(bound_args),
                               std::tuple<std::remove_cvref_t<NextArg>>{
                                   std::forward<NextArg>(next_arg)})};
        }
    }

    template <class NextArg, std::size_t... Idx>
    constexpr auto invoke_or_extend_const(NextArg &&next_arg,
                                          std::index_sequence<Idx...>) const {
        if constexpr (std::invocable<const Function &, const BoundArgs &...,
                                     NextArg>) {
            return std::invoke(function, std::get<Idx>(bound_args)...,
                               std::forward<NextArg>(next_arg));
        } else {
            using next_partial =
                terminating_partial<Function, BoundArgs...,
                                    std::remove_cvref_t<NextArg>>;
            return next_partial{
                function,
                std::tuple_cat(bound_args,
                               std::tuple<std::remove_cvref_t<NextArg>>{
                                   std::forward<NextArg>(next_arg)})};
        }
    }
};

/// Wraps @p function in a zero-argument @c terminating_partial.
template <class Function>
constexpr auto make_terminating_partial(Function &&function) {
    using stored = std::remove_cvref_t<Function>;
    return terminating_partial<stored>{std::forward<Function>(function),
                                       std::tuple<>{}};
}

} // namespace detail

/// CRTP base that derives applicative operations from @c pure and @c apply.
///
/// @c Impl must provide:
/// - @c pure(value) — embeds a plain value into the applicative context.
/// - @c apply(f_ctx, a_ctx) — applies a contextualized function to a
///   contextualized argument.
///
/// All other operations (@c invoke, @c lift_a2, @c ap, @c discard_first,
/// @c discard_second) are derived using @ref detail::terminating_partial.
///
/// @tparam Impl Concrete implementation providing @c pure and @c apply.
template <class Impl>
struct applicative : protected Impl {
    using Impl::apply;
    using Impl::pure;

    /// Lifts @p function and applies it to one or more contextualized
    /// arguments left-to-right.
    ///
    /// If @c Impl provides its own @c invoke, that overload is preferred,
    /// allowing instances to supply custom multi-arg semantics (e.g.,
    /// shape-aware applicatives). Otherwise, the derivation proceeds via
    /// @c pure(partial(f)) followed by chained @c ap calls.
    ///
    /// @tparam Function  A plain callable type.
    /// @tparam FirstArg  First effectful argument type.
    /// @tparam RestArgs  Additional effectful argument types.
    template <class Function, class FirstArg, class... RestArgs>
    constexpr auto invoke(this auto &&self, Function &&function,
                          FirstArg &&first_arg, RestArgs &&...rest_args) {
        using Self = std::remove_reference_t<decltype(self)>;
        using ImplBase =
            std::conditional_t<std::is_const_v<Self>, const Impl, Impl>;

        if constexpr (requires(ImplBase &impl) {
                          impl.invoke(std::forward<Function>(function),
                                      std::forward<FirstArg>(first_arg),
                                      std::forward<RestArgs>(rest_args)...);
                      }) {
            return static_cast<ImplBase &>(self).invoke(
                std::forward<Function>(function),
                std::forward<FirstArg>(first_arg),
                std::forward<RestArgs>(rest_args)...);
        } else {
            auto lifted = self.pure(detail::make_terminating_partial(
                std::forward<Function>(function)));
            return self.apply_chain(
                self.ap(std::move(lifted), std::forward<FirstArg>(first_arg)),
                std::forward<RestArgs>(rest_args)...);
        }
    }

    /// Lifts a binary function and applies it to two effectful arguments.
    /// Equivalent to @c invoke(function, a, b).
    template <class Function, class A, class B>
    constexpr auto lift_a2(this auto &&self, Function &&function, A &&a,
                           B &&b) {
        return self.invoke(std::forward<Function>(function), std::forward<A>(a),
                           std::forward<B>(b));
    }

    /// Alias for the @c apply primitive; applies a contextualized function
    /// to a contextualized argument.
    template <class FunctionInContext, class ArgInContext>
    constexpr auto ap(this auto &&self, FunctionInContext &&function,
                      ArgInContext &&argument) {
        return self.apply(std::forward<FunctionInContext>(function),
                          std::forward<ArgInContext>(argument));
    }

    /// Sequences two effectful values, discarding the first value and
    /// returning the second. Logs/effects from both are preserved.
    template <class FirstArg, class SecondArg>
    constexpr auto discard_first(this auto &&self, FirstArg &&first,
                                 SecondArg &&second) {
        return self.invoke(
            [](const auto &, auto &&rhs) {
                return std::forward<decltype(rhs)>(rhs);
            },
            std::forward<FirstArg>(first), std::forward<SecondArg>(second));
    }

    /// Sequences two effectful values, discarding the second value and
    /// returning the first. Logs/effects from both are preserved.
    template <class FirstArg, class SecondArg>
    constexpr auto discard_second(this auto &&self, FirstArg &&first,
                                  SecondArg &&second) {
        return self.invoke(
            [](auto &&lhs, const auto &) {
                return std::forward<decltype(lhs)>(lhs);
            },
            std::forward<FirstArg>(first), std::forward<SecondArg>(second));
    }

  private:
    template <class Accumulated>
    constexpr auto apply_chain(this auto &&, Accumulated &&accumulated) {
        return std::forward<Accumulated>(accumulated);
    }

    template <class Accumulated, class NextArg, class... RestArgs>
    constexpr auto apply_chain(this auto &&self, Accumulated &&accumulated,
                               NextArg &&next_arg, RestArgs &&...rest_args) {
        auto next = self.ap(std::forward<Accumulated>(accumulated),
                            std::forward<NextArg>(next_arg));
        if constexpr (sizeof...(RestArgs) == 0) {
            return next;
        } else {
            return self.apply_chain(std::move(next),
                                    std::forward<RestArgs>(rest_args)...);
        }
    }
};

/// Typeclass lookup variable for Applicative; specialize for each type.
///
/// Default is @c std::false_type{}, producing a compile error if @ref invoke
/// is called for an unregistered type.
template <class T>
inline constexpr auto applicative_typeclass = std::false_type{};

/// Customization-point object for the @c invoke operation.
///
/// Deduces the applicative context type from the first argument and dispatches
/// through @c applicative_typeclass<FirstArg>. The NTTP @c TC may be pinned
/// explicitly for testing or alternate dispatch.
struct invoke_fn {
    /// Applies @p function to one or more effectful arguments.
    ///
    /// @tparam Function  A plain callable type.
    /// @tparam FirstArg  First effectful argument type (used for typeclass
    /// lookup).
    /// @tparam RestArgs  Additional effectful argument types.
    /// @tparam TC        Typeclass instance (NTTP, defaults to lookup).
    template <
        class Function, class FirstArg, class... RestArgs,
        const auto &TC = applicative_typeclass<std::remove_cvref_t<FirstArg>>>
    constexpr auto operator()(Function &&function, FirstArg &&first_arg,
                              RestArgs &&...rest_args) const {
        using tc_type = std::remove_cvref_t<decltype(TC)>;
        return tc_type{}.invoke(std::forward<Function>(function),
                                std::forward<FirstArg>(first_arg),
                                std::forward<RestArgs>(rest_args)...);
    }
};

/// Global CPO for Applicative's @c invoke operation.
inline constexpr invoke_fn invoke{};

} // namespace smd::smdscheme::foundation

#endif
