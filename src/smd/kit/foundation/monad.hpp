// src/smd/kit/foundation/monad.hpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// New in this tree rather than extracted with the rest of the kit: neither
// older copy had a Monad vocabulary. result has been a monad since R3 under
// the datatype's own name -- and_then -- and nothing registered it, so no
// generic algorithm could dispatch to it and docs/CODING_RULES.md's Semantic
// Defaults asked for monad-derived Applicative semantics against a Monad
// that did not exist. This is the mark; and_then stays as result's domain
// spelling and is now defined in terms of it.
#ifndef SRC_SMD_KIT_FOUNDATION_MONAD_HPP
#define SRC_SMD_KIT_FOUNDATION_MONAD_HPP

#include <functional>
#include <type_traits>
#include <utility>

namespace smd::kit::foundation {

/// CRTP base that derives Monad operations from @c bind and @c pure.
///
/// @c Impl must provide:
/// - @c pure(value) — embeds a plain value into the monadic context. As in
///   @ref applicative this is implementor-facing, with no CPO of its own,
///   because it cannot be dispatched from its argument.
/// - @c bind(m, f) — sequences a monadic value with a function returning the
///   next monadic value. @p f is invoked only if @p m succeeded, which is
///   what makes @c bind, and not @c apply, the operation at which effects
///   can be skipped.
///
/// Derived here: @c join, @c then, @c apply.
///
/// @c apply is the monad-derived Applicative that
/// <tt>docs/CODING_RULES.md</tt>'s Semantic Defaults mandate. An instance
/// that is both may define its Applicative primitive by calling this rather
/// than writing the branches out a second time.
///
/// The derivations assume @c bind invokes its function before returning,
/// which holds for every strict instance and is the same assumption
/// @ref applicative's derivations already make. An instance that defers its
/// continuation instead of running it — a parser that stores what to do
/// next — has to supply these operations itself rather than inherit them.
///
/// What this typeclass deliberately lacks is any way to ask an effect
/// whether it has already failed. That question is @c short_circuit_effect
/// in <smd/kit/foundation/fold_left_short.hpp>. It is not a Monad
/// operation, and the fold there is not derived from @c bind because of it.
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
    /// yielding the second.
    ///
    /// Both are already-evaluated values, so what a failed @p first skips is
    /// yielding @p second, not producing it — the caller has done that work
    /// before the call. @c bind can skip work only when it is handed a
    /// function not to call.
    ///
    /// @p second is captured by value rather than by reference: a deferred
    /// instance could outlive this call, and DIV-0007 is what a captured
    /// reference costs when it does.
    ///
    /// @tparam M First monadic value type.
    /// @tparam N Second monadic value type.
    template <class M, class N>
    constexpr auto then(this auto &&self, M &&first, N &&second) {
        return self.bind(
            std::forward<M>(first),
            [second = std::forward<N>(second)](auto const &) { return second; });
    }

    /// Applicative application, derived from @c bind and @c pure as the
    /// standard @c ap: `f >>= \g -> a >>= \x -> pure (g x)`.
    ///
    /// The leftmost error wins and the function runs only if both operands
    /// succeeded, which falls out of the nesting rather than needing to be
    /// written as branches. As with @ref then, both operands arrive already
    /// evaluated, so this discards after a failure but cannot prevent the
    /// work that produced them; stopping early is @c fold_left_short's job.
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

/// Customization-point object for the @c bind primitive.
///
/// Deduces the monadic type from the first argument and dispatches through
/// @c monad_typeclass<M>. The NTTP @c TC may be pinned explicitly.
struct bind_fn {
    /// Sequences @p m with @p f, which runs only if @p m succeeded.
    ///
    /// @tparam M  Monadic type (deduced, and what the lookup keys on).
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
    ///
    /// @tparam MM A monadic value whose value type is itself monadic.
    /// @tparam TC Typeclass instance (NTTP, defaults to lookup).
    template <class MM,
              const auto &TC = monad_typeclass<std::remove_cvref_t<MM>>>
    constexpr auto operator()(MM &&nested) const {
        using tc_type = std::remove_cvref_t<decltype(TC)>;
        return tc_type{}.join(std::forward<MM>(nested));
    }
};

/// Global CPO for Monad's @c join operation.
inline constexpr join_fn join{};

} // namespace smd::kit::foundation

#endif
