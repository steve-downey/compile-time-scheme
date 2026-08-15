// src/smd/kit/foundation/functor.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Extracted in step R8 from smd::cl::foundation::functor, itself the
// reviewed union of two independently-drifted copies:
// src/smd/smdscheme/foundation/functor.hpp (compile-time-scheme) and
// src/smd/forth/foundation/functor.hpp (compile-time-forth). Those two
// copies differed only in include guard, namespace, and an "adapted by
// copy" comment — the zero-drift signal decision R8 acts on.
#ifndef SRC_SMD_KIT_FOUNDATION_FUNCTOR_HPP
#define SRC_SMD_KIT_FOUNDATION_FUNCTOR_HPP

#include <type_traits>
#include <utility>

namespace smd::kit::foundation {

/// CRTP base that derives the @c replace operation from @c fmap.
///
/// @c Impl must provide @c fmap(function, container). Inheriting from
/// @c functor<Impl> exposes @c fmap and adds @c replace as a derived member.
///
/// @tparam Impl Concrete implementation providing the @c fmap primitive.
template <class Impl>
struct functor : protected Impl {
    using Impl::fmap;

    /// Replaces every element of @p value with @p replacement, discarding
    /// the original elements. Derived from @c fmap.
    ///
    /// @tparam T          Container type.
    /// @tparam U          Replacement value type.
    /// @param  value       The container whose elements are replaced.
    /// @param  replacement The value to substitute for each element.
    template <class T, class U>
    constexpr auto replace(this auto &&self, T &&value, U &&replacement) {
        return self.fmap([replacement = std::forward<U>(replacement)](
                             const auto &) { return replacement; },
                         std::forward<T>(value));
    }
};

/// Typeclass lookup variable for Functor; specialize for each container type.
///
/// Default value is @c std::false_type{}, which triggers a static_assert if
/// @ref fmap is called with an unregistered type.
template <class T>
inline constexpr auto functor_typeclass = std::false_type{};

/// Customization-point object for @c fmap.
///
/// Deduces the container type @p T from the second argument and dispatches
/// through @c functor_typeclass<T>. Callers may pin the typeclass instance
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
              const auto &TC = functor_typeclass<std::remove_cvref_t<T>>>
    constexpr auto operator()(F &&f, T &&value) const {
        using tc_type = std::remove_cvref_t<decltype(TC)>;
        return tc_type{}.fmap(std::forward<F>(f), std::forward<T>(value));
    }
};

/// Global CPO for Functor's @c fmap operation.
inline constexpr fmap_fn fmap{};

} // namespace smd::kit::foundation

#endif
