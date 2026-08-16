// src/smd/kit/foundation/traversable.hpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Extracted in step R8 from smd::cl::foundation::traversable. Decision 2's
// original cut left this in cl on the grounds that neither smdscheme nor
// compile-time-forth had a Traversable vocabulary to need it. That
// reasoning was overturned: the absence reflects the algebraic refactoring
// never having been done on the Forth side, not a fact about this code's
// generality. The corrected standard is docs/cl-rebuild-plan.md §5's own
// test — generic in shape and free of language-specific types — which this
// file passes outright: it is a CRTP base and a set of CPOs over an
// abstract container and effect, naming nothing from a parser, a reader,
// or an AST.
#ifndef SRC_SMD_KIT_FOUNDATION_TRAVERSABLE_HPP
#define SRC_SMD_KIT_FOUNDATION_TRAVERSABLE_HPP

#include <type_traits>
#include <utility>

namespace smd::kit::foundation {

/// CRTP base that derives Traversable operations from the @c traverse
/// primitive.
///
/// @c traverse is the minimal operation. @c Impl must provide:
/// - @c traverse(f, container) — applies the effectful function @p f
///   (@c Element -> @c Effect<B>) to every element and collects the results
///   in an effect around a container of the same shape.
///
/// Contract (per docs/CODING_RULES.md's Traversable rules):
/// - traversal preserves shape;
/// - traversal uses the instance's documented Foldable order;
/// - effect order is observable and is therefore part of the contract.
///
/// Derived here: @c sequence.
///
/// @tparam Impl Concrete implementation providing @c traverse.
template <class Impl>
struct traversable : protected Impl {
    using Impl::traverse;

    /// Collects a container of effects into an effect of a container,
    /// derived as @c traverse with the identity function.
    template <class T>
    constexpr auto sequence(this auto &&self, T const &container) {
        return self.traverse([](auto const &effect) { return effect; },
                             container);
    }
};

/// Typeclass lookup variable for Traversable; specialize for each container.
///
/// Default is @c std::false_type{}, producing a compile error if @ref
/// traverse is called for an unregistered type.
template <class T>
inline constexpr auto traversable_typeclass = std::false_type{};

/// Customization-point object for @c traverse.
///
/// Deduces the container type from the second argument and dispatches
/// through @c traversable_typeclass<T>. The NTTP @c TC may be pinned
/// explicitly.
struct traverse_fn {
    /// Applies the effectful @p f to every element of @p container.
    ///
    /// @tparam F  Callable type, @c Element -> @c Effect<B>.
    /// @tparam T  Container type (deduced, used for typeclass lookup).
    /// @tparam TC Typeclass instance (NTTP, defaults to lookup).
    template <class F, class T,
              const auto &TC = traversable_typeclass<std::remove_cvref_t<T>>>
    constexpr auto operator()(F &&f, T const &container) const {
        using tc_type = std::remove_cvref_t<decltype(TC)>;
        return tc_type{}.traverse(std::forward<F>(f), container);
    }
};

/// Global CPO for Traversable's @c traverse operation.
inline constexpr traverse_fn traverse{};

/// Customization-point object for the derived @c sequence operation.
struct sequence_fn {
    /// Collects a container of effects into an effect of a container.
    template <class T,
              const auto &TC = traversable_typeclass<std::remove_cvref_t<T>>>
    constexpr auto operator()(T const &container) const {
        using tc_type = std::remove_cvref_t<decltype(TC)>;
        return tc_type{}.sequence(container);
    }
};

/// Global CPO for Traversable's @c sequence operation.
inline constexpr sequence_fn sequence{};

} // namespace smd::kit::foundation

#endif
