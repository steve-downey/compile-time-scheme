// src/smd/kit/foundation/functor.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Extracted in step R8 from smd::cl::foundation::functor, itself the
// reviewed union of two independently-drifted copies:
// src/smd/smdscheme/foundation/functor.hpp (compile-time-scheme, at
// iteration/smdscheme-final) and src/smd/forth/foundation/functor.hpp
// (compile-time-forth). Those two copies differed only in include guard,
// namespace, and an "adapted by copy" comment — the zero-drift signal decision
// R8 acts on.
#ifndef SRC_SMD_KIT_FOUNDATION_FUNCTOR_HPP
#define SRC_SMD_KIT_FOUNDATION_FUNCTOR_HPP

#include <smd/kit/foundation/typeclass_base.hpp>

#include <type_traits>
#include <utility>

namespace smd::kit::foundation {

// Functor pattern invariants:
// - An instance is a single lookup object providing fmap(f, container).
// - replace is a derived object operation, implemented from fmap.
// - Dispatch happens through a provided object or through functor<T>.
// - Lookup stays explicit through typeclass objects, never ADL overloads.

/// CRTP base that derives the @c replace operation from @c fmap.
///
/// @c Impl must provide @c fmap(function, container). Inheriting from
/// @c derive_functor<Impl> exposes @c fmap and adds @c replace as a derived
/// member.
///
/// Both members are this base's own, never @c "using @c Impl::fmap;". That
/// is the own-member rule: an external call enters this base's member, which
/// hands the next base down a @c self typed as the instance map — one level
/// deep, where addressing @c Impl still works. A using-declaration here would
/// re-form the chain and the inner base's @c impl_of would see a @c self
/// typed as the outer wrapper, whose @c Impl is an inaccessible base.
///
/// @tparam Impl Concrete implementation providing the @c fmap primitive.
template <class Impl>
struct derive_functor : protected Impl {
    /// Applies @p f to every element of @p value, returning a new container.
    ///
    /// @tparam F  Callable type.
    /// @tparam T  Container type.
    /// @param  f     Function to apply element-wise.
    /// @param  value Container to map over.
    template <class F, class T>
    constexpr auto fmap(this auto &&self, F &&f, T &&value)
        requires requires(Impl const &impl) {
            impl.fmap(std::forward<F>(f), std::forward<T>(value));
        }
    {
        return impl_of(self).fmap(std::forward<F>(f), std::forward<T>(value));
    }

    /// Replaces every element of @p value with @p replacement, discarding
    /// the original elements. Derived from @c fmap, unless @c Impl supplies
    /// a native @c replace, which is preferred — wrapping fills gaps and
    /// never shadows a better operation the instance author wrote.
    ///
    /// @tparam T          Container type.
    /// @tparam U          Replacement value type.
    /// @param  value       The container whose elements are replaced.
    /// @param  replacement The value to substitute for each element.
    template <class T, class U>
    constexpr auto replace(this auto &&self, T &&value, U &&replacement)
        requires requires(Impl const &impl) {
            impl.replace(std::forward<T>(value), std::forward<U>(replacement));
        } || requires(Impl const &impl) {
            impl.fmap([replacement = std::forward<U>(replacement)](
                          auto const &) { return replacement; },
                      std::forward<T>(value));
        }
    {
        if constexpr (requires {
                          impl_of(self).replace(std::forward<T>(value),
                                                std::forward<U>(replacement));
                      }) {
            return impl_of(self).replace(std::forward<T>(value),
                                         std::forward<U>(replacement));
        } else {
            // A one-way derivation, not one half of a mutually-derivable
            // pair, so it routes through self rather than Impl: a shadowing
            // fmap on a wrapping map is still meant to be reached.
            return self.fmap([replacement = std::forward<U>(replacement)](
                                 auto const &) { return replacement; },
                             std::forward<T>(value));
        }
    }

  private:
    template <class Self>
    static constexpr auto impl_of(Self &&self) -> decltype(auto) {
        return static_cast<impl_ref_t<Impl, Self>>(self);
    }
};

/// Typeclass lookup variable for Functor; specialize for each container type.
///
/// Default value is @c std::false_type{}, which triggers a static_assert if
/// @ref fmap is called with an unregistered type.
template <class T>
inline constexpr auto functor = std::false_type{};

/// Restricted @c Impl concept for Functor: satisfied when @p Impl supplies
/// the minimal complete basis @ref derive_functor needs — @c fmap alone,
/// probed with a representative witness callable.
///
/// This is the MINIMAL pragma to @ref functor_object's class declaration. An
/// @p Impl may satisfy this and still fail @ref functor_object, which is
/// exactly the bargain the CRTP base exists to keep. Never demand a derived
/// operation here; @c replace belongs to @ref functor_object alone.
template <class Impl, class Context>
concept functor_impl = requires(Impl const &impl, Context const &context) {
    impl.fmap(detail::probe_witness<element_type_t<Context>>{}, context);
};

/// Deep object concept for a Functor object over @p Context: satisfied when
/// @p Obj provides the whole object surface — @c fmap and the derived
/// @c replace.
///
/// Conformance is structural, so a hand-written object that never uses
/// @ref derive_functor satisfies this too; nothing here requires deriving
/// from the base. Checking the derived operations at the gate is the point:
/// a shallow concept probing only @c fmap accepts an object that has no
/// @c replace, and defers the failure to whenever code written much later
/// first reaches it, three frames deep.
template <class Obj, class Context>
concept functor_object = requires(Obj const &obj, Context const &context,
                                  element_type_t<Context> const &element) {
    obj.fmap(detail::probe_witness<element_type_t<Context>>{}, context);
    obj.replace(context, element);
};

/// Operation object for Functor's @c fmap.
///
/// Deduces the container type @p T from the second argument and dispatches
/// through @c functor<T>. Callers may pin the typeclass instance
/// via the NTTP default @c TC.
struct fmap_fn {
    /// Applies @p f to every element of @p value, returning a new container.
    ///
    /// @tparam F  Callable type.
    /// @tparam T  Container type (deduced).
    /// @tparam TC Typeclass instance (NTTP, defaults to lookup).
    /// @param f     Function to apply element-wise.
    /// @param value Container to map over.
    template <class F, class T,
              const auto &TC = functor<std::remove_cvref_t<T>>>
    constexpr auto operator()(F &&f, T &&value) const {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Functor instance for this type. Specialize "
                      "smd::kit::foundation::functor<T> with an object "
                      "providing fmap(f, container).");
        return TC.fmap(std::forward<F>(f), std::forward<T>(value));
    }
};

/// Global operation object for Functor's @c fmap.
inline constexpr fmap_fn fmap{};

/// Operation object for Functor's derived @c replace.
struct replace_fn {
    /// Replaces every element of @p value with @p replacement.
    ///
    /// @tparam T  Container type (deduced).
    /// @tparam U  Replacement value type.
    /// @tparam TC Typeclass instance (NTTP, defaults to lookup).
    template <class T, class U,
              const auto &TC = functor<std::remove_cvref_t<T>>>
    constexpr auto operator()(T &&value, U &&replacement) const {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Functor instance for this type. Specialize "
                      "smd::kit::foundation::functor<T> with an object "
                      "providing fmap(f, container).");
        return TC.replace(std::forward<T>(value), std::forward<U>(replacement));
    }
};

/// Global operation object for Functor's @c replace.
inline constexpr replace_fn replace{};

} // namespace smd::kit::foundation

#endif
