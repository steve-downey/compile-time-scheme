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

#include <smd/kit/foundation/typeclass_base.hpp>

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
struct derive_traversable : protected Impl {
    /// Applies the effectful @p f to every element of @p container,
    /// collecting the results in an effect around a container of the same
    /// shape.
    template <class F, class T>
    constexpr auto traverse(this auto &&self, F &&f, T const &container)
        requires requires(Impl const &impl) {
            impl.traverse(std::forward<F>(f), container);
        }
    {
        return impl_of(self).traverse(std::forward<F>(f), container);
    }

    /// Collects a container of effects into an effect of a container,
    /// derived as @c traverse with the identity function. A native
    /// @c Impl::sequence is preferred.
    template <class T>
    constexpr auto sequence(this auto &&self, T const &container)
        requires requires(Impl const &impl) { impl.sequence(container); } ||
                 requires {
                     self.traverse([](auto const &effect) { return effect; },
                                   container);
                 }
    {
        if constexpr (requires { impl_of(self).sequence(container); }) {
            return impl_of(self).sequence(container);
        } else {
            // One-way derivation, so it routes through self: a shadowing
            // traverse on a wrapping map is still meant to be reached.
            return self.traverse([](auto const &effect) { return effect; },
                                 container);
        }
    }

  private:
    template <class Self>
    static constexpr auto impl_of(Self &&self) -> decltype(auto) {
        return static_cast<impl_ref_t<Impl, Self>>(self);
    }
};

/// Typeclass lookup variable for Traversable; specialize for each container.
///
/// Default is @c std::false_type{}, producing a compile error if @ref
/// traverse is called for an unregistered type.
template <class T>
inline constexpr auto traversable = std::false_type{};

/// Restricted @c Impl concept for Traversable: satisfied when @p Impl
/// supplies the minimal complete basis @ref derive_traversable needs —
/// @c traverse alone, probed with a witness callable that lifts an element
/// into @p Effect.
///
/// This is the MINIMAL pragma to @ref traversable_object's class
/// declaration; @c sequence is derived and belongs to that concept alone.
///
/// @tparam Impl    The primitive-providing implementation.
/// @tparam Context The container being traversed.
/// @tparam Effect  The applicative the traversal runs in.
template <class Impl, class Context, class Effect>
concept traversable_impl = requires(Impl const &impl, Context const &context) {
    impl.traverse(detail::probe_witness<Effect>{}, context);
};

/// Deep object concept for a Traversable object: satisfied when @p Obj
/// provides @c traverse over @p Context in @p Effect, and the derived
/// @c sequence over @p ContextOfEffects.
///
/// @c sequence needs a container whose elements are themselves effects, and
/// nothing in this kit can rebind @p Context's element type to name one, so
/// the caller supplies it. It defaults to @p Context, which is right when
/// the container being checked already holds effects and wrong otherwise —
/// pass it explicitly in that case. This is the one place where the deep
/// concept is weaker than the ideal: an object whose @c sequence is missing
/// is caught only when the caller names a container to catch it with.
///
/// @tparam Obj              The typeclass object.
/// @tparam Context          The container being traversed.
/// @tparam Effect           The applicative the traversal runs in.
/// @tparam ContextOfEffects A container of effects, for probing @c sequence.
template <class Obj, class Context, class Effect,
          class ContextOfEffects = Context>
concept traversable_object =
    requires(Obj const &obj, Context const &context,
             ContextOfEffects const &effects) {
        obj.traverse(detail::probe_witness<Effect>{}, context);
        obj.sequence(effects);
    };

/// Operation object for @c traverse.
///
/// Deduces the container type from the second argument and dispatches
/// through @c traversable<T>. The NTTP @c TC may be pinned
/// explicitly.
struct traverse_fn {
    /// Applies the effectful @p f to every element of @p container.
    ///
    /// @tparam F  Callable type, @c Element -> @c Effect<B>.
    /// @tparam T  Container type (deduced, used for typeclass lookup).
    /// @tparam TC Typeclass instance (NTTP, defaults to lookup).
    template <class F, class T,
              const auto &TC = traversable<std::remove_cvref_t<T>>>
    constexpr auto operator()(F &&f, T const &container) const {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Traversable instance for this type. Specialize "
                      "smd::kit::foundation::traversable<T> with an object "
                      "providing traverse(f, container).");
        return TC.traverse(std::forward<F>(f), container);
    }
};

/// Global operation object for Traversable's @c traverse.
inline constexpr traverse_fn traverse{};

/// Operation object for the derived @c sequence operation.
struct sequence_fn {
    /// Collects a container of effects into an effect of a container.
    template <class T, const auto &TC = traversable<std::remove_cvref_t<T>>>
    constexpr auto operator()(T const &container) const {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Traversable instance for this type. Specialize "
                      "smd::kit::foundation::traversable<T> with an object "
                      "providing traverse(f, container).");
        return TC.sequence(container);
    }
};

/// Global operation object for Traversable's @c sequence.
inline constexpr sequence_fn sequence{};

} // namespace smd::kit::foundation

#endif
