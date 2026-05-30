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

template <class Function, class... BoundArgs>
struct terminating_partial {
    Function function;
    std::tuple<BoundArgs...> bound_args;

    template <class NextArg>
    constexpr auto operator()(NextArg &&next_arg) {
        return invoke_or_extend(std::forward<NextArg>(next_arg),
                                std::index_sequence_for<BoundArgs...>{});
    }

    template <class NextArg>
    constexpr auto operator()(NextArg &&next_arg) const {
        return invoke_or_extend_const(
            std::forward<NextArg>(next_arg),
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
                std::tuple_cat(
                    std::move(bound_args),
                    std::tuple<std::remove_cvref_t<NextArg>>{
                        std::forward<NextArg>(next_arg)})};
        }
    }

    template <class NextArg, std::size_t... Idx>
    constexpr auto invoke_or_extend_const(
        NextArg &&next_arg, std::index_sequence<Idx...>) const {
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
                std::tuple_cat(
                    bound_args,
                    std::tuple<std::remove_cvref_t<NextArg>>{
                        std::forward<NextArg>(next_arg)})};
        }
    }
};

template <class Function>
constexpr auto make_terminating_partial(Function &&function) {
    using stored = std::remove_cvref_t<Function>;
    return terminating_partial<stored>{std::forward<Function>(function),
                                       std::tuple<>{}};
}

} // namespace detail

template <class Impl>
struct applicative : protected Impl {
    using Impl::apply;
    using Impl::pure;

    template <class Function, class FirstArg, class... RestArgs>
    constexpr auto invoke(this auto &&self, Function &&function,
                          FirstArg &&first_arg, RestArgs &&...rest_args) {
        using Self = std::remove_reference_t<decltype(self)>;
        using ImplBase =
            std::conditional_t<std::is_const_v<Self>, const Impl, Impl>;

        if constexpr (requires(ImplBase &impl) {
                          impl.invoke(
                              std::forward<Function>(function),
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
                self.ap(std::move(lifted),
                        std::forward<FirstArg>(first_arg)),
                std::forward<RestArgs>(rest_args)...);
        }
    }

    template <class Function, class A, class B>
    constexpr auto lift_a2(this auto &&self, Function &&function, A &&a,
                           B &&b) {
        return self.invoke(std::forward<Function>(function),
                           std::forward<A>(a), std::forward<B>(b));
    }

    template <class FunctionInContext, class ArgInContext>
    constexpr auto ap(this auto &&self, FunctionInContext &&function,
                      ArgInContext &&argument) {
        return self.apply(std::forward<FunctionInContext>(function),
                          std::forward<ArgInContext>(argument));
    }

    template <class FirstArg, class SecondArg>
    constexpr auto discard_first(this auto &&self, FirstArg &&first,
                                 SecondArg &&second) {
        return self.invoke(
            [](const auto &, auto &&rhs) {
                return std::forward<decltype(rhs)>(rhs);
            },
            std::forward<FirstArg>(first),
            std::forward<SecondArg>(second));
    }

    template <class FirstArg, class SecondArg>
    constexpr auto discard_second(this auto &&self, FirstArg &&first,
                                  SecondArg &&second) {
        return self.invoke(
            [](auto &&lhs, const auto &) {
                return std::forward<decltype(lhs)>(lhs);
            },
            std::forward<FirstArg>(first),
            std::forward<SecondArg>(second));
    }

  private:
    template <class Accumulated>
    constexpr auto apply_chain(this auto &&, Accumulated &&accumulated) {
        return std::forward<Accumulated>(accumulated);
    }

    template <class Accumulated, class NextArg, class... RestArgs>
    constexpr auto apply_chain(this auto &&self, Accumulated &&accumulated,
                               NextArg &&next_arg,
                               RestArgs &&...rest_args) {
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

template <class T>
inline constexpr auto applicative_typeclass = std::false_type{};

struct invoke_fn {
    template <class Function, class FirstArg, class... RestArgs,
              const auto &TC = applicative_typeclass<
                  std::remove_cvref_t<FirstArg>>>
    constexpr auto operator()(Function &&function, FirstArg &&first_arg,
                              RestArgs &&...rest_args) const {
        using tc_type = std::remove_cvref_t<decltype(TC)>;
        return tc_type{}.invoke(std::forward<Function>(function),
                                std::forward<FirstArg>(first_arg),
                                std::forward<RestArgs>(rest_args)...);
    }
};

inline constexpr invoke_fn invoke{};

} // namespace smd::smdscheme::foundation

#endif
