// src/smd/kit/foundation/alternative.hpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Extracted in step R8 from smd::cl::foundation::alternative, itself the
// reviewed union of two independently-drifted copies:
// src/smd/smdscheme/foundation/alternative.hpp (compile-time-scheme, at
// iteration/smdscheme-final) and src/smd/forth/foundation/alternative.hpp
// (compile-time-forth). Those two copies differed only in include guard,
// namespace, and an "adapted by copy" comment — the zero-drift signal decision
// R8 acts on.
#ifndef SRC_SMD_KIT_FOUNDATION_ALTERNATIVE_HPP
#define SRC_SMD_KIT_FOUNDATION_ALTERNATIVE_HPP

#include <smd/kit/foundation/typeclass_base.hpp>

#include <type_traits>
#include <utility>

namespace smd::kit::foundation {

/// CRTP base that derives @c combine from the @c zero and @c alt primitives.
///
/// @c Impl must provide:
/// - @c zero() — the identity element for @c alt (e.g., an empty writer).
/// - @c alt(a, b) — combines two alternatives (e.g., concatenates writer logs).
///
/// The identity is @c zero, not @c empty, because one namespace holds the
/// whole typeclass family and a name in it has to mean one thing. @c empty
/// is Foldable's predicate — the reading @c std::empty, @c std::ranges::empty
/// and every container's own member already have in C++ — so Alternative's
/// identity takes the other name. That is the opposite of the FP convention,
/// where Haskell, PureScript and Cats all give @c empty to Alternative and
/// call the Foldable predicate something else, and it is deliberate:
/// @c zero<C>() sits naturally beside @c pure<C>(v), both building a context
/// and both having to name it.
///
/// @tparam Impl Concrete implementation providing @c zero and @c alt.
template <class Impl>
struct derive_alternative : protected Impl {
    /// The identity element for @c alt.
    template <class Self>
    constexpr auto zero(this Self &&self)
        requires requires(Impl const &impl) { impl.zero(); }
    {
        return impl_of(self).zero();
    }

    /// Combines two alternatives.
    template <class A, class B>
    constexpr auto alt(this auto &&self, A &&a, B &&b)
        requires requires(Impl const &impl) {
            impl.alt(std::forward<A>(a), std::forward<B>(b));
        }
    {
        return impl_of(self).alt(std::forward<A>(a), std::forward<B>(b));
    }

    /// Combines @p a and @p b using the @c alt primitive. A native
    /// @c Impl::combine is preferred.
    /// For writer-like types this concatenates the logs and takes @p b's value.
    template <class A, class B>
    constexpr auto combine(this auto &&self, A &&a, B &&b)
        requires requires(Impl const &impl) {
            impl.combine(std::forward<A>(a), std::forward<B>(b));
        } || requires { self.alt(std::forward<A>(a), std::forward<B>(b)); }
    {
        if constexpr (requires {
                          impl_of(self).combine(std::forward<A>(a),
                                                std::forward<B>(b));
                      }) {
            return impl_of(self).combine(std::forward<A>(a),
                                         std::forward<B>(b));
        } else {
            return self.alt(std::forward<A>(a), std::forward<B>(b));
        }
    }

  private:
    template <class Self>
    static constexpr auto impl_of(Self &&self) -> decltype(auto) {
        return static_cast<impl_ref_t<Impl, Self>>(self);
    }
};

/// Typeclass lookup variable for Alternative; specialize for each type.
///
/// Default is @c std::false_type{}, producing a compile error if @ref alt or
/// @ref empty is called for an unregistered type.
template <class T>
inline constexpr auto alternative = std::false_type{};

/// Restricted @c Impl concept for Alternative: satisfied when @p Impl
/// supplies the minimal complete basis @ref derive_alternative needs —
/// @c zero and @c alt.
///
/// This is the MINIMAL pragma to @ref alternative_object's class
/// declaration; @c combine is derived and belongs to that concept alone.
template <class Impl, class Context>
concept alternative_impl = requires(Impl const &impl, Context const &context) {
    impl.zero();
    impl.alt(context, context);
};

/// Deep object concept for an Alternative object over @p Context: satisfied
/// when @p Obj provides the whole object surface — @c zero and @c alt, plus
/// the derived @c combine.
template <class Obj, class Context>
concept alternative_object = requires(Obj const &obj, Context const &context) {
    obj.zero();
    obj.alt(context, context);
    obj.combine(context, context);
};

/// Operation object for the @c alt operation.
///
/// Deduces the type from the first argument and dispatches through
/// @c alternative<A>. The NTTP @c TC may be pinned explicitly.
struct alt_fn {
    /// Combines two alternative values.
    ///
    /// @tparam A   First operand type (used for typeclass lookup).
    /// @tparam B   Second operand type.
    /// @tparam TC  Typeclass instance (NTTP, defaults to lookup).
    template <class A, class B,
              const auto &TC = alternative<std::remove_cvref_t<A>>>
    constexpr auto operator()(A &&a, B &&b) const {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Alternative instance for this type. Specialize "
                      "smd::kit::foundation::alternative<T> with an object "
                      "providing zero() and alt(a, b).");
        return TC.alt(std::forward<A>(a), std::forward<B>(b));
    }
};

/// Global operation object for Alternative's @c alt.
inline constexpr alt_fn alt{};

/// Operation object for the @c zero operation — Alternative's identity
/// element, the unit of @c alt.
///
/// The explicit @c T template argument selects the typeclass instance; there
/// is no argument from which to deduce it.
struct zero_fn {
    /// Returns the identity element for @c alt of type @p T.
    ///
    /// @tparam T   The alternative type (explicit template argument required).
    /// @tparam TC  Typeclass instance (NTTP, defaults to lookup).
    template <class T, const auto &TC = alternative<std::remove_cvref_t<T>>>
    constexpr auto operator()() const {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Alternative instance for this type. Specialize "
                      "smd::kit::foundation::alternative<T> with an object "
                      "providing zero() and alt(a, b).");
        return TC.zero();
    }
};

/// Global operation object for Alternative's @c zero.
inline constexpr zero_fn zero{};

/// Operation object for the derived @c combine operation.
struct combine_fn {
    /// Combines two alternative values through @c alt.
    template <class A, class B,
              const auto &TC = alternative<std::remove_cvref_t<A>>>
    constexpr auto operator()(A &&a, B &&b) const {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Alternative instance for this type. Specialize "
                      "smd::kit::foundation::alternative<T> with an object "
                      "providing zero() and alt(a, b).");
        return TC.combine(std::forward<A>(a), std::forward<B>(b));
    }
};

/// Global operation object for Alternative's @c combine.
inline constexpr combine_fn combine{};

} // namespace smd::kit::foundation

#endif
