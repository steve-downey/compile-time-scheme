// src/smd/cl/foundation/monad.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// New in this tree: the Monad typeclass. docs/CODING_RULES.md's Semantic
// Defaults have required monad-derived Applicative semantics all along
// against a Monad that did not exist, and result's `and_then` has been bind
// under another name since R3.
#ifndef SRC_SMD_CL_FOUNDATION_MONAD_HPP
#define SRC_SMD_CL_FOUNDATION_MONAD_HPP

#include <functional>
#include <type_traits>
#include <utility>

namespace smd::cl::foundation {

/// CRTP base that derives Monad operations from @c bind and @c pure.
///
/// @c Impl must provide:
/// - @c pure(value) — embeds a plain value into the monadic context. As in
///   @ref applicative, this is implementor-facing: it cannot be dispatched
///   from its argument, so there is no global CPO for it.
/// - @c bind(m, f) — sequences a monadic value with a function returning the
///   next monadic value. @p f is invoked only when @p m succeeded, which is
///   what makes @c bind, and not @c apply, the place effects can be skipped.
///
/// Derived here: @c join, @c then, @c apply.
///
/// @c apply is the monad-derived Applicative that
/// <tt>docs/CODING_RULES.md</tt>'s Semantic Defaults mandate: an instance
/// that is both may define its Applicative primitive by calling this rather
/// than writing the branches out a second time.
///
/// What this typeclass deliberately does not have is a way to ask an effect
/// whether it has already failed. That question is @c short_circuit_effect
/// in <smd/cl/foundation/fold_left_short.hpp>, and it is not a Monad
/// operation; see that header for why the fold there is not derived from
/// @c bind.
///
/// @tparam Impl Concrete implementation providing @c bind and @c pure.
template <class Impl>
struct monad : protected Impl {
    using Impl::bind;
    using Impl::pure;

    /// Collapses one layer of nesting, derived as @c bind with the identity
    /// function.
    ///
    /// @tparam MM A monadic value whose value type is itself monadic.
    template <class MM>
    constexpr auto join(this auto &&self, MM &&nested) {
        return self.bind(std::forward<MM>(nested),
                         [](auto const &inner) { return inner; });
    }

    /// Sequences two monadic values, discarding the first's value and
    /// returning the second. The second is not reached if the first failed.
    ///
    /// @tparam M First monadic value type.
    /// @tparam N Second monadic value type.
    template <class M, class N>
    constexpr auto then(this auto &&self, M &&first, N &&second) {
        return self.bind(std::forward<M>(first),
                         [second = std::forward<N>(second)](auto const &) {
                             return second;
                         });
    }

    /// Applicative application, derived from @c bind and @c pure — the
    /// standard @c ap: `f >>= \g -> a >>= \x -> pure (g x)`.
    ///
    /// @tparam MF A monadic value holding a callable.
    /// @tparam MA A monadic value holding that callable's argument.
    template <class MF, class MA>
    constexpr auto apply(this auto &&self, MF const &function_value,
                         MA const &argument_value) {
        return self.bind(
            function_value, [&self, &argument_value](auto const &function) {
                return self.bind(
                    argument_value, [&self, &function](auto const &argument) {
                        return self.pure(std::invoke(function, argument));
                    });
            });
    }
};

/// Typeclass lookup variable for Monad; specialize for each type.
///
/// Default is @c std::false_type{}, producing a compile error if @ref bind
/// is called for an unregistered type.
template <class T>
inline constexpr auto monad_typeclass = std::false_type{};

/// Customization-point object for @c bind.
///
/// Deduces the monadic type from the first argument and dispatches through
/// @c monad_typeclass<M>. The NTTP @c TC may be pinned explicitly.
struct bind_fn {
    /// Sequences @p m with @p f, which is invoked only if @p m succeeded.
    ///
    /// @tparam M  Monadic type (deduced, used for typeclass lookup).
    /// @tparam F  Callable, @c Value -> @c Monadic<U>.
    /// @tparam TC Typeclass instance (NTTP, defaults to lookup).
    template <class M, class F,
              const auto &TC = monad_typeclass<std::remove_cvref_t<M>>>
    constexpr auto operator()(M &&m, F &&f) const {
        using tc_type = std::remove_cvref_t<decltype(TC)>;
        return tc_type{}.bind(std::forward<M>(m), std::forward<F>(f));
    }
};

/// Global CPO for Monad's @c bind operation.
inline constexpr bind_fn bind{};

/// Customization-point object for the derived @c join operation.
struct join_fn {
    /// Collapses one layer of nesting in @p nested.
    template <class MM,
              const auto &TC = monad_typeclass<std::remove_cvref_t<MM>>>
    constexpr auto operator()(MM &&nested) const {
        using tc_type = std::remove_cvref_t<decltype(TC)>;
        return tc_type{}.join(std::forward<MM>(nested));
    }
};

/// Global CPO for Monad's @c join operation.
inline constexpr join_fn join{};

} // namespace smd::cl::foundation

#endif
