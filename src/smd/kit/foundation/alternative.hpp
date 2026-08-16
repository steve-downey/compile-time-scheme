// src/smd/kit/foundation/alternative.hpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Extracted in step R8 from smd::cl::foundation::alternative, itself the
// reviewed union of two independently-drifted copies:
// src/smd/smdscheme/foundation/alternative.hpp (compile-time-scheme) and
// src/smd/forth/foundation/alternative.hpp (compile-time-forth). Those two
// copies differed only in include guard, namespace, and an "adapted by
// copy" comment — the zero-drift signal decision R8 acts on.
#ifndef SRC_SMD_KIT_FOUNDATION_ALTERNATIVE_HPP
#define SRC_SMD_KIT_FOUNDATION_ALTERNATIVE_HPP

#include <type_traits>
#include <utility>

namespace smd::kit::foundation {

/// CRTP base that derives @c combine from the @c empty and @c alt primitives.
///
/// @c Impl must provide:
/// - @c empty() — the identity element for @c alt (e.g., an empty writer).
/// - @c alt(a, b) — combines two alternatives (e.g., concatenates writer logs).
///
/// @tparam Impl Concrete implementation providing @c empty and @c alt.
template <class Impl>
struct alternative : protected Impl {
    using Impl::alt;
    using Impl::empty;

    /// Combines @p a and @p b using the @c alt primitive.
    /// For writer-like types this concatenates the logs and takes @p b's value.
    template <class A, class B>
    constexpr auto combine(this auto &&self, A &&a, B &&b) {
        return self.alt(std::forward<A>(a), std::forward<B>(b));
    }
};

/// Typeclass lookup variable for Alternative; specialize for each type.
///
/// Default is @c std::false_type{}, producing a compile error if @ref alt or
/// @ref empty is called for an unregistered type.
template <class T>
inline constexpr auto alternative_typeclass = std::false_type{};

/// Customization-point object for the @c alt operation.
///
/// Deduces the type from the first argument and dispatches through
/// @c alternative_typeclass<A>. The NTTP @c TC may be pinned explicitly.
struct alt_fn {
    /// Combines two alternative values.
    ///
    /// @tparam A   First operand type (used for typeclass lookup).
    /// @tparam B   Second operand type.
    /// @tparam TC  Typeclass instance (NTTP, defaults to lookup).
    template <class A, class B,
              const auto &TC = alternative_typeclass<std::remove_cvref_t<A>>>
    constexpr auto operator()(A &&a, B &&b) const {
        using tc_type = std::remove_cvref_t<decltype(TC)>;
        return tc_type{}.alt(std::forward<A>(a), std::forward<B>(b));
    }
};

/// Global CPO for Alternative's @c alt operation.
inline constexpr alt_fn alt{};

/// Customization-point object for the @c empty operation.
///
/// The explicit @c T template argument selects the typeclass instance; there
/// is no argument from which to deduce it automatically.
struct empty_fn {
    /// Returns the identity element for @c alt of type @p T.
    ///
    /// @tparam T   The alternative type (explicit template argument required).
    /// @tparam TC  Typeclass instance (NTTP, defaults to lookup).
    template <class T,
              const auto &TC = alternative_typeclass<std::remove_cvref_t<T>>>
    constexpr auto operator()() const {
        using tc_type = std::remove_cvref_t<decltype(TC)>;
        return tc_type{}.empty();
    }
};

/// Global CPO for Alternative's @c empty operation.
inline constexpr empty_fn empty{};

} // namespace smd::kit::foundation

#endif
