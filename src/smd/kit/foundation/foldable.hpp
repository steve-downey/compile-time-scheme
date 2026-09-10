// src/smd/kit/foundation/foldable.hpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Extracted in step R8 from smd::cl::foundation::foldable. Decision 2's
// original cut left this in cl on the grounds that neither smdscheme nor
// compile-time-forth had a Foldable vocabulary to need it. That reasoning
// was overturned: the absence reflects the algebraic refactoring never
// having been done on the Forth side, not a fact about this code's
// generality. The corrected standard is docs/cl-rebuild-plan.md §5's own
// test — generic in shape and free of language-specific types — which this
// file passes outright: it is a CRTP base and a set of CPOs over an
// abstract container and monoid, naming nothing from a parser, a reader,
// or an AST.
#ifndef SRC_SMD_KIT_FOUNDATION_FOLDABLE_HPP
#define SRC_SMD_KIT_FOUNDATION_FOLDABLE_HPP

#include <smd/kit/foundation/monoid.hpp>
#include <smd/kit/foundation/typeclass_base.hpp>

#include <type_traits>
#include <utility>

namespace smd::kit::foundation {

/// CRTP base that derives Foldable operations from the @c fold_map primitive.
///
/// @c fold_map is the semantic centre. @c Impl must provide:
/// - @c fold_map(f, container, monoid) — maps each element through @p f into
///   the monoid's carrier and combines the images in the instance's
///   documented traversal order.
/// - @c fold_right(f, init, container) — the right fold, as a primitive.
///   Deriving it from @c fold_map would need either a function-composition
///   monoid (not practical under constexpr with fixed capacities) or a
///   materialized reversal (needs a capacity this generic base cannot know),
///   so it is the one documented exception to "derived where practical".
///
/// Derived here: @c fold_left, @c length, @c empty, @c to_vector.
///
/// Traversal order is part of the instance contract and must be documented
/// by each instance; the derivations below inherit whatever order the
/// instance's @c fold_map documents.
///
/// @tparam Impl Concrete implementation providing @c fold_map and
///              @c fold_right.
template <class Impl>
struct derive_foldable : protected Impl {
    /// Maps each element of @p container through @p f into the monoid's
    /// carrier and combines the images in the instance's documented
    /// traversal order.
    template <class F, class T, class M>
    constexpr auto fold_map(this auto &&self, F &&f, T const &container,
                            M const &m)
        requires requires(Impl const &impl) {
            impl.fold_map(std::forward<F>(f), container, m);
        }
    {
        return impl_of(self).fold_map(std::forward<F>(f), container, m);
    }

    /// Right fold, the instance's second primitive.
    template <class F, class Acc, class T>
    constexpr auto fold_right(this auto &&self, F &&f, Acc init,
                              T const &container)
        requires requires(Impl const &impl) {
            impl.fold_right(std::forward<F>(f), init, container);
        }
    {
        return impl_of(self).fold_right(std::forward<F>(f), std::move(init),
                                        container);
    }

    /// Left fold, derived from @c fold_map: each element updates an
    /// accumulator captured by reference, sequenced by the instance's
    /// documented traversal order under the effect-only @ref unit_monoid.
    ///
    /// @tparam F   Callable with signature @c Acc(Acc, Element).
    /// @tparam Acc Accumulator type.
    /// @tparam T   Container type.
    // 2c04510b-930c-4d2e-be6c-8733f844baa1
    template <class F, class Acc, class T>
    constexpr auto fold_left(this auto &&self, F &&f, Acc init,
                             T const &container) -> Acc
        requires requires(Impl const &impl) {
            impl.fold_left(std::forward<F>(f), init, container);
        } || requires {
            // self, not impl: a non-capturing marker, because a capturing
            // lambda naming a function parameter inside a trailing
            // requires-clause is ill-formed. fold_map is generic in its
            // callable, so only the return-type shape matters here.
            self.fold_map([](auto const &) { return unit{}; }, container,
                          unit_monoid);
        }
    {
        if constexpr (requires {
                          impl_of(self).fold_left(std::forward<F>(f), init,
                                                  container);
                      }) {
            return impl_of(self).fold_left(std::forward<F>(f), std::move(init),
                                           container);
        } else {
            self.fold_map(
                [&](auto const &element) {
                    init = f(std::move(init), element);
                    return unit{};
                },
                container, unit_monoid);
            return init;
        }
    }
    // 2c04510b-930c-4d2e-be6c-8733f844baa1 end

    /// Number of elements, derived from @c fold_map into @ref sum_monoid.
    template <class T>
    [[nodiscard]] constexpr auto length(this auto &&self, T const &container)
        -> int
        requires requires(Impl const &impl) { impl.length(container); } ||
                 requires {
                     self.fold_map([](auto const &) { return 1; }, container,
                                   sum_monoid<int>);
                 }
    {
        if constexpr (requires { impl_of(self).length(container); }) {
            return impl_of(self).length(container);
        } else {
            return self.fold_map([](auto const &) { return 1; }, container,
                                 sum_monoid<int>);
        }
    }

    /// True when @p container holds no elements. Derived from @c length,
    /// which is itself derived; a native @c Impl::empty is preferred, and an
    /// instance that can answer without counting should supply one.
    ///
    /// This is Foldable's @c empty — the predicate, the reading C++ already
    /// has for the name in @c std::empty, @c std::ranges::empty and every
    /// container's own member. Alternative's identity element is @c zero,
    /// which is the other way round from the FP convention and deliberate:
    /// see alternative.hpp.
    template <class T>
    [[nodiscard]] constexpr auto empty(this auto &&self, T const &container)
        -> bool
        requires requires(Impl const &impl) { impl.empty(container); } ||
                 requires { self.length(container); }
    {
        if constexpr (requires { impl_of(self).empty(container); }) {
            return impl_of(self).empty(container);
        } else {
            return self.length(container) == 0;
        }
    }

    /// Copies the elements, in the instance's documented traversal order,
    /// into a default-constructed @p Out (any type with @c push_back).
    /// Derived from @c fold_left.
    ///
    /// @tparam Out The output container type (explicit template argument).
    template <class Out, class T>
    [[nodiscard]] constexpr auto to_vector(this auto &&self, T const &container)
        -> Out
        requires requires(Impl const &impl) {
            impl.template to_vector<Out>(container);
        } || requires {
            // self, not impl: fold_left is this base's own derived member
            // for every instance that does not supply one, so a clause
            // naming impl would disagree with the body and the member would
            // vanish from overload resolution with no diagnostic.
            self.fold_left([](Out acc, auto const &) { return acc; }, Out{},
                           container);
        }
    {
        if constexpr (requires {
                          impl_of(self).template to_vector<Out>(container);
                      }) {
            return impl_of(self).template to_vector<Out>(container);
        } else {
            return self.fold_left(
                [](Out acc, auto const &element) {
                    acc.push_back(element);
                    return acc;
                },
                Out{}, container);
        }
    }

  private:
    template <class Self>
    static constexpr auto impl_of(Self &&self) -> decltype(auto) {
        return static_cast<impl_ref_t<Impl, Self>>(self);
    }
};

/// Typeclass lookup variable for Foldable; specialize for each container.
///
/// Default is @c std::false_type{}, producing a compile error if a Foldable
/// operation is called for an unregistered type.
template <class T>
inline constexpr auto foldable = std::false_type{};

/// Restricted @c Impl concept for Foldable: satisfied when @p Impl supplies
/// the minimal complete basis @ref derive_foldable needs — @c fold_map and
/// @c fold_right, the latter being the documented exception to "derived
/// where practical".
///
/// This is the MINIMAL pragma to @ref foldable_object's class declaration;
/// @c fold_left, @c length, @c empty and @c to_vector are all derived and
/// belong to that concept alone.
template <class Impl, class Context>
concept foldable_impl =
    requires(Impl const &impl, Context const &context,
             element_type_t<Context> const &element) {
        impl.fold_map([](auto const &) { return unit{}; }, context,
                      unit_monoid);
        impl.fold_right(detail::probe_witness2<element_type_t<Context>>{},
                        element, context);
    };

/// Deep object concept for a Foldable object over @p Context: satisfied when
/// @p Obj provides the whole object surface — @c fold_map and @c fold_right,
/// plus the derived @c fold_left, @c length and @c empty.
///
/// @c to_vector is not probed: it takes its output container as an explicit
/// template argument, so probing it would pin an arbitrary output type into
/// the concept rather than check the operation.
template <class Obj, class Context>
concept foldable_object =
    requires(Obj const &obj, Context const &context,
             element_type_t<Context> const &element) {
        obj.fold_map([](auto const &) { return unit{}; }, context,
                     unit_monoid);
        obj.fold_right(detail::probe_witness2<element_type_t<Context>>{},
                       element, context);
        obj.fold_left(detail::probe_witness2<element_type_t<Context>>{},
                      element, context);
        obj.length(context);
        obj.empty(context);
    };

/// Operation object for @c fold_map.
///
/// Deduces the container type from the second argument and dispatches
/// through @c foldable<T>. The NTTP @c TC may be pinned explicitly.
struct fold_map_fn {
    /// Maps each element of @p container through @p f and combines the
    /// images with @p m, in the instance's documented traversal order.
    ///
    /// @tparam F  Callable type.
    /// @tparam T  Container type (deduced, used for typeclass lookup).
    /// @tparam M  Monoid instance object type.
    /// @tparam TC Typeclass instance (NTTP, defaults to lookup).
    template <class F, class T, class M,
              const auto &TC = foldable<std::remove_cvref_t<T>>>
    constexpr auto operator()(F &&f, T const &container, M const &m) const {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Foldable instance for this type. Specialize "
                      "smd::kit::foundation::foldable<T> with an object "
                      "providing fold_map(f, container, monoid) and "
                      "fold_right(f, init, container).");
        return TC.fold_map(std::forward<F>(f), container, m);
    }
};

/// Global operation object for Foldable's @c fold_map.
inline constexpr fold_map_fn fold_map{};

/// Operation object for the derived @c fold_left operation.
struct fold_left_fn {
    /// Left-folds @p container with @p f from @p init.
    template <class F, class Acc, class T,
              const auto &TC = foldable<std::remove_cvref_t<T>>>
    constexpr auto operator()(F &&f, Acc init, T const &container) const
        -> Acc {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Foldable instance for this type. Specialize "
                      "smd::kit::foundation::foldable<T>.");
        return TC.fold_left(std::forward<F>(f), std::move(init), container);
    }
};

/// Global operation object for Foldable's @c fold_left.
inline constexpr fold_left_fn fold_left{};

/// Operation object for the @c fold_right primitive.
struct fold_right_fn {
    /// Right-folds @p container with @p f from @p init.
    template <class F, class Acc, class T,
              const auto &TC = foldable<std::remove_cvref_t<T>>>
    constexpr auto operator()(F &&f, Acc init, T const &container) const
        -> Acc {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Foldable instance for this type. Specialize "
                      "smd::kit::foundation::foldable<T>.");
        return TC.fold_right(std::forward<F>(f), std::move(init), container);
    }
};

/// Global operation object for Foldable's @c fold_right.
inline constexpr fold_right_fn fold_right{};

/// Operation object for the derived @c length operation.
struct length_fn {
    /// Returns the number of elements in @p container.
    template <class T, const auto &TC = foldable<std::remove_cvref_t<T>>>
    constexpr auto operator()(T const &container) const -> int {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Foldable instance for this type. Specialize "
                      "smd::kit::foundation::foldable<T>.");
        return TC.length(container);
    }
};

/// Global operation object for Foldable's @c length.
inline constexpr length_fn length{};

/// Operation object for the derived @c empty predicate.
///
/// This is the name's Foldable reading — "holds nothing" — which is the one
/// C++ already has for it. Alternative's identity element is @c zero.
struct empty_fn {
    /// True when @p container holds no elements.
    template <class T, const auto &TC = foldable<std::remove_cvref_t<T>>>
    constexpr auto operator()(T const &container) const -> bool {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Foldable instance for this type. Specialize "
                      "smd::kit::foundation::foldable<T>.");
        return TC.empty(container);
    }
};

/// Global operation object for Foldable's @c empty.
inline constexpr empty_fn empty{};

} // namespace smd::kit::foundation

#endif
