// src/smd/kit/foundation/applicative.hpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Extracted in step R8 from smd::cl::foundation::applicative, itself the
// reviewed union of two independently-drifted copies:
// src/smd/smdscheme/foundation/applicative.hpp (compile-time-scheme, at
// iteration/smdscheme-final) and src/smd/forth/foundation/applicative.hpp
// (compile-time-forth). Those two copies differed only in include guard,
// namespace, and an "adapted by copy" comment — the zero-drift signal decision
// R8 acts on.
#ifndef SRC_SMD_KIT_FOUNDATION_APPLICATIVE_HPP
#define SRC_SMD_KIT_FOUNDATION_APPLICATIVE_HPP

#include <smd/kit/foundation/typeclass_base.hpp>

#include <concepts>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace smd::kit::foundation {

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
struct derive_applicative : protected Impl {
    /// Embeds a plain value into the applicative context.
    template <class V>
    constexpr auto pure(this auto &&self, V &&value)
        requires requires(Impl const &impl) {
            impl.pure(std::forward<V>(value));
        }
    {
        return impl_of(self).pure(std::forward<V>(value));
    }

    /// Applies a contextualized function to a contextualized argument.
    template <class FunctionInContext, class ArgInContext>
    constexpr auto apply(this auto &&self, FunctionInContext &&function,
                         ArgInContext &&argument)
        requires requires(Impl const &impl) {
            impl.apply(std::forward<FunctionInContext>(function),
                       std::forward<ArgInContext>(argument));
        }
    {
        return impl_of(self).apply(std::forward<FunctionInContext>(function),
                                   std::forward<ArgInContext>(argument));
    }

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
        if constexpr (requires {
                          impl_of(self).invoke(
                              std::forward<Function>(function),
                              std::forward<FirstArg>(first_arg),
                              std::forward<RestArgs>(rest_args)...);
                      }) {
            return impl_of(self).invoke(std::forward<Function>(function),
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
    constexpr auto lift_a2(this auto &&self, Function &&function, A &&a, B &&b)
        requires requires(Impl const &impl) {
            impl.lift_a2(std::forward<Function>(function), std::forward<A>(a),
                         std::forward<B>(b));
        } || requires {
            // self, not impl: invoke may be this base's own synthesized
            // member rather than anything Impl supplies, and a clause that
            // named impl would disagree with the body below — which makes
            // the member vanish from overload resolution with no
            // diagnostic.
            self.invoke(detail::probe_witness2<int>{}, std::forward<A>(a),
                        std::forward<B>(b));
        }
    {
        if constexpr (requires {
                          impl_of(self).lift_a2(
                              std::forward<Function>(function),
                              std::forward<A>(a), std::forward<B>(b));
                      }) {
            return impl_of(self).lift_a2(std::forward<Function>(function),
                                         std::forward<A>(a),
                                         std::forward<B>(b));
        } else {
            return self.invoke(std::forward<Function>(function),
                               std::forward<A>(a), std::forward<B>(b));
        }
    }

    /// Alias for the @c apply primitive; applies a contextualized function
    /// to a contextualized argument.
    template <class FunctionInContext, class ArgInContext>
    constexpr auto ap(this auto &&self, FunctionInContext &&function,
                      ArgInContext &&argument)
        requires requires(Impl const &impl) {
            impl.ap(std::forward<FunctionInContext>(function),
                    std::forward<ArgInContext>(argument));
        } || requires {
            self.apply(std::forward<FunctionInContext>(function),
                       std::forward<ArgInContext>(argument));
        }
    {
        if constexpr (requires {
                          impl_of(self).ap(
                              std::forward<FunctionInContext>(function),
                              std::forward<ArgInContext>(argument));
                      }) {
            return impl_of(self).ap(std::forward<FunctionInContext>(function),
                                    std::forward<ArgInContext>(argument));
        } else {
            return self.apply(std::forward<FunctionInContext>(function),
                              std::forward<ArgInContext>(argument));
        }
    }

    /// Sequences two effectful values, discarding the first value and
    /// returning the second. Logs/effects from both are preserved.
    template <class FirstArg, class SecondArg>
    constexpr auto discard_first(this auto &&self, FirstArg &&first,
                                 SecondArg &&second)
        requires requires(Impl const &impl) {
            impl.discard_first(std::forward<FirstArg>(first),
                               std::forward<SecondArg>(second));
        } || requires {
            self.invoke(detail::probe_witness2<int>{},
                        std::forward<FirstArg>(first),
                        std::forward<SecondArg>(second));
        }
    {
        if constexpr (requires {
                          impl_of(self).discard_first(
                              std::forward<FirstArg>(first),
                              std::forward<SecondArg>(second));
                      }) {
            return impl_of(self).discard_first(std::forward<FirstArg>(first),
                                               std::forward<SecondArg>(second));
        } else {
            return self.invoke(
                [](const auto &, auto &&rhs) {
                    return std::forward<decltype(rhs)>(rhs);
                },
                std::forward<FirstArg>(first), std::forward<SecondArg>(second));
        }
    }

    /// Sequences two effectful values, discarding the second value and
    /// returning the first. Logs/effects from both are preserved.
    template <class FirstArg, class SecondArg>
    constexpr auto discard_second(this auto &&self, FirstArg &&first,
                                  SecondArg &&second)
        requires requires(Impl const &impl) {
            impl.discard_second(std::forward<FirstArg>(first),
                                std::forward<SecondArg>(second));
        } || requires {
            self.invoke(detail::probe_witness2<int>{},
                        std::forward<FirstArg>(first),
                        std::forward<SecondArg>(second));
        }
    {
        if constexpr (requires {
                          impl_of(self).discard_second(
                              std::forward<FirstArg>(first),
                              std::forward<SecondArg>(second));
                      }) {
            return impl_of(self).discard_second(
                std::forward<FirstArg>(first), std::forward<SecondArg>(second));
        } else {
            return self.invoke(
                [](auto &&lhs, const auto &) {
                    return std::forward<decltype(lhs)>(lhs);
                },
                std::forward<FirstArg>(first), std::forward<SecondArg>(second));
        }
    }

  private:
    template <class Self>
    static constexpr auto impl_of(Self &&self) -> decltype(auto) {
        return static_cast<impl_ref_t<Impl, Self>>(self);
    }

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
inline constexpr auto applicative = std::false_type{};

/// Restricted @c Impl concept for Applicative: satisfied when @p Impl
/// supplies the minimal complete basis @ref derive_applicative needs —
/// @c pure and @c apply.
///
/// This is the MINIMAL pragma to @ref applicative_object's class
/// declaration. @c invoke, @c lift_a2, @c ap, @c discard_first and
/// @c discard_second are all derived and belong to @ref applicative_object
/// alone.
template <class Impl, class Context>
concept applicative_impl = requires(Impl const &impl, Context const &context,
                                    element_type_t<Context> const &element) {
    impl.pure(element);
    impl.apply(impl.pure(detail::probe_witness<element_type_t<Context>>{}),
               context);
};

/// Deep object concept for an Applicative object over @p Context: satisfied
/// when @p Obj provides the whole object surface — @c pure and @c apply,
/// plus the derived @c invoke, @c lift_a2, @c ap, @c discard_first and
/// @c discard_second.
///
/// The derived operations are probed with representative witness callables:
/// the check is a witness that the operation exists, not a proof that it
/// exists for every callable, which is what the failure mode at issue — an
/// operation missing entirely — actually needs.
template <class Obj, class Context>
concept applicative_object = requires(Obj const &obj, Context const &context,
                                      element_type_t<Context> const &element) {
    obj.pure(element);
    obj.apply(obj.pure(detail::probe_witness<element_type_t<Context>>{}),
              context);
    obj.ap(obj.pure(detail::probe_witness<element_type_t<Context>>{}), context);
    obj.invoke(detail::probe_witness<element_type_t<Context>>{}, context);
    obj.lift_a2(detail::probe_witness2<element_type_t<Context>>{}, context,
                context);
    obj.discard_first(context, context);
    obj.discard_second(context, context);
};

/// Operation object for the @c invoke operation.
///
/// Deduces the applicative context type from the first effectful argument and
/// dispatches through @c applicative<FirstArg>. The NTTP @c TC may be pinned
/// explicitly for testing or alternate dispatch.
struct invoke_fn {
    /// Applies @p function to one or more effectful arguments.
    ///
    /// @tparam Function  A plain callable type.
    /// @tparam FirstArg  First effectful argument type (used for typeclass
    /// lookup).
    /// @tparam RestArgs  Additional effectful argument types.
    /// @tparam TC        Typeclass instance (NTTP, defaults to lookup).
    template <class Function, class FirstArg, class... RestArgs,
              const auto &TC = applicative<std::remove_cvref_t<FirstArg>>>
    constexpr auto operator()(Function &&function, FirstArg &&first_arg,
                              RestArgs &&...rest_args) const {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Applicative instance for this type. Specialize "
                      "smd::kit::foundation::applicative<T> with an object "
                      "providing pure(value) and apply(f_ctx, a_ctx).");
        return TC.invoke(std::forward<Function>(function),
                         std::forward<FirstArg>(first_arg),
                         std::forward<RestArgs>(rest_args)...);
    }
};

/// Global operation object for Applicative's @c invoke.
inline constexpr invoke_fn invoke{};

/// Operation object for the derived @c ap operation.
struct ap_fn {
    /// Applies a contextualized function to a contextualized argument.
    ///
    /// @tparam FunctionInContext First operand type (what keys the lookup).
    /// @tparam ArgInContext      Second operand type.
    /// @tparam TC                Typeclass instance (NTTP, defaults to lookup).
    template <
        class FunctionInContext, class ArgInContext,
        const auto &TC = applicative<std::remove_cvref_t<FunctionInContext>>>
    constexpr auto operator()(FunctionInContext &&function,
                              ArgInContext &&argument) const {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Applicative instance for this type. Specialize "
                      "smd::kit::foundation::applicative<T> with an object "
                      "providing pure(value) and apply(f_ctx, a_ctx).");
        return TC.ap(std::forward<FunctionInContext>(function),
                     std::forward<ArgInContext>(argument));
    }
};

/// Global operation object for Applicative's @c ap.
inline constexpr ap_fn ap{};

/// Operation object for the derived @c discard_first operation.
struct discard_first_fn {
    /// Sequences two effectful values, yielding the second's value.
    template <class FirstArg, class SecondArg,
              const auto &TC = applicative<std::remove_cvref_t<FirstArg>>>
    constexpr auto operator()(FirstArg &&first, SecondArg &&second) const {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Applicative instance for this type. Specialize "
                      "smd::kit::foundation::applicative<T> with an object "
                      "providing pure(value) and apply(f_ctx, a_ctx).");
        return TC.discard_first(std::forward<FirstArg>(first),
                                std::forward<SecondArg>(second));
    }
};

/// Global operation object for Applicative's @c discard_first.
inline constexpr discard_first_fn discard_first{};

/// Operation object for the derived @c discard_second operation.
struct discard_second_fn {
    /// Sequences two effectful values, yielding the first's value.
    template <class FirstArg, class SecondArg,
              const auto &TC = applicative<std::remove_cvref_t<FirstArg>>>
    constexpr auto operator()(FirstArg &&first, SecondArg &&second) const {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Applicative instance for this type. Specialize "
                      "smd::kit::foundation::applicative<T> with an object "
                      "providing pure(value) and apply(f_ctx, a_ctx).");
        return TC.discard_second(std::forward<FirstArg>(first),
                                 std::forward<SecondArg>(second));
    }
};

/// Global operation object for Applicative's @c discard_second.
inline constexpr discard_second_fn discard_second{};

} // namespace smd::kit::foundation

#endif
