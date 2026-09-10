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

#include <smd/kit/foundation/functor.hpp>
#include <smd/kit/foundation/typeclass_base.hpp>

#include <concepts>
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
/// Derived here: @c fmap (the Functor basis), @c join, @c then, @c apply,
/// @c kleisli, and @ref as_functor.
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
struct derive_monad : protected Impl {
    /// Embeds a plain value into the monadic context.
    template <class V>
    constexpr auto pure(this auto &&self, V &&value)
        requires requires(Impl const &impl) { impl.pure(std::forward<V>(value)); }
    {
        return impl_of(self).pure(std::forward<V>(value));
    }

    /// Sequences @p m with @p f, which runs only if @p m succeeded.
    template <class M, class F>
    constexpr auto bind(this auto &&self, M &&m, F &&f)
        requires requires(Impl const &impl) {
            impl.bind(std::forward<M>(m), std::forward<F>(f));
        }
    {
        return impl_of(self).bind(std::forward<M>(m), std::forward<F>(f));
    }

    /// @c fmap: the Functor basis, grounded in @c bind and @c pure.
    ///
    ///     fmap(f, ma) = ma >>= (pure . f)
    ///
    /// A monad is a functor, and this is that theorem spelled as an
    /// operation rather than as a superclass constraint. A native
    /// @c Impl::fmap is preferred — an instance that can map without
    /// sequencing usually should. The full Functor instance is spelled at
    /// the registration site as @c derive_functor<this_map>{}, or reached
    /// through @ref as_functor: this base grows the Functor BASIS only, so
    /// Functor's derived surface stays Functor's and monad-grounded
    /// instances track it instead of drifting from it.
    ///
    /// @tparam F  Callable type.
    /// @tparam MA Monadic value type.
    template <class F, class MA>
    constexpr auto fmap(this auto &&self, F &&f, MA &&ma)
        requires requires(Impl const &impl) {
            impl.fmap(std::forward<F>(f), std::forward<MA>(ma));
        } || requires(Impl const &impl) {
            impl.bind(std::forward<MA>(ma), std::declval<F &>());
        }
    {
        if constexpr (requires {
                          impl_of(self).fmap(std::forward<F>(f),
                                             std::forward<MA>(ma));
                      }) {
            return impl_of(self).fmap(std::forward<F>(f), std::forward<MA>(ma));
        } else {
            return impl_of(self).bind(std::forward<MA>(ma), [&](auto &&a) {
                return impl_of(self).pure(
                    std::invoke(f, std::forward<decltype(a)>(a)));
            });
        }
    }

    /// Collapses one layer of nesting, derived as @c bind with the identity
    /// function. A native @c Impl::join is preferred.
    ///
    /// @c join and @c bind are a mutually-derivable pair — @c join is
    /// @c bind with the identity, and @c bind is recoverable from @c join
    /// and @c fmap — so the fallback addresses @c Impl directly rather than
    /// @c self, which is what keeps the two from recursing into each other.
    ///
    /// @tparam MM A monadic value whose value type is itself monadic.
    template <class MM>
    constexpr auto join(this auto &&self, MM &&nested)
        requires requires(Impl const &impl) {
            impl.join(std::forward<MM>(nested));
        } || requires(Impl const &impl) {
            impl.bind(std::forward<MM>(nested),
                      [](auto const &inner) { return inner; });
        }
    {
        if constexpr (requires { impl_of(self).join(std::forward<MM>(nested)); }) {
            return impl_of(self).join(std::forward<MM>(nested));
        } else {
            return impl_of(self).bind(std::forward<MM>(nested),
                                      [](auto const &inner) { return inner; });
        }
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
    constexpr auto then(this auto &&self, M &&first, N &&second)
        requires requires(Impl const &impl) {
            impl.then(std::forward<M>(first), std::forward<N>(second));
        } || requires(Impl const &impl) {
            impl.bind(std::forward<M>(first),
                      [second = std::forward<N>(second)](auto const &) {
                          return second;
                      });
        }
    {
        if constexpr (requires {
                          impl_of(self).then(std::forward<M>(first),
                                             std::forward<N>(second));
                      }) {
            return impl_of(self).then(std::forward<M>(first),
                                      std::forward<N>(second));
        } else {
            // One-way derivation, so it routes through self: a shadowing
            // bind on a wrapping map is still meant to be reached.
            return self.bind(std::forward<M>(first),
                             [second = std::forward<N>(second)](auto const &) {
                                 return second;
                             });
        }
    }

    /// Forward Kleisli composition, `(f >=> g) a = f a >>= g`. A native
    /// @c Impl::kleisli is preferred.
    ///
    /// Unlike the other derived members here, this one's basis requirement
    /// cannot be spelled as a second alternative in the requires-clause: the
    /// derivation's use of @c bind lives inside the returned closure, over
    /// an argument type that is not known until the closure is called, so
    /// there is no concrete expression to probe at this member's own
    /// instantiation. @c bind's presence is guaranteed by the class
    /// invariant anyway, which is why the second alternative is trivially
    /// true rather than absent.
    ///
    /// @tparam F Callable, @c A -> @c Monadic<B>.
    /// @tparam G Callable, @c B -> @c Monadic<C>.
    template <class F, class G>
    constexpr auto kleisli(this auto &&self, F f, G g)
        requires requires(Impl const &impl) { impl.kleisli(f, g); } || true
    {
        if constexpr (requires { impl_of(self).kleisli(f, g); }) {
            return impl_of(self).kleisli(f, g);
        } else {
            return [&self, f = std::move(f), g = std::move(g)](auto &&a) {
                return self.bind(f(std::forward<decltype(a)>(a)), g);
            };
        }
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
                         MA const &argument_value)
        requires requires(Impl const &impl) {
            impl.apply(function_value, argument_value);
        } || requires {
            typename element_type_t<MF>;
            typename element_type_t<MA>;
            requires std::invocable<element_type_t<MF> const &,
                                    element_type_t<MA> const &>;
        }
    {
        if constexpr (requires {
                          impl_of(self).apply(function_value, argument_value);
                      }) {
            return impl_of(self).apply(function_value, argument_value);
        } else {
            return self.bind(
                function_value, [&self, &argument_value](auto const &function) {
                    return self.bind(argument_value, [&self, &function](
                                                         auto const &argument) {
                        return self.pure(std::invoke(function, argument));
                    });
                });
        }
    }

    /// The full Functor instance over this monad object.
    ///
    /// Every typeclass object in this kit is stateless and empty, so
    /// constructing the full instance and "converting" are the same free
    /// type-level move. This is `Monad m => Functor m` superclass
    /// subsumption, paid for with one visible call that names which functor
    /// is meant instead of a @c remove_cvref_t incantation at the call site:
    ///
    ///     f(monad_map.as_functor(), xs);
    ///
    /// It is needed because a bare monad object correctly fails the deep
    /// @c functor_object concept: this base grows the Functor basis, never
    /// its derived surface, so a monad object has no @c replace.
    ///
    /// The functor returned is the one derived from the object in hand,
    /// law-compatible with that object's @c bind by construction. It
    /// deliberately does not consult @c functor<T>: a caller who
    /// wants the registered default says so by looking it up.
    constexpr auto as_functor(this auto &&self) {
        return derive_functor<std::remove_cvref_t<decltype(self)>>{};
    }

  private:
    template <class Self>
    static constexpr auto impl_of(Self &&self) -> decltype(auto) {
        return static_cast<impl_ref_t<Impl, Self>>(self);
    }
};

/// Typeclass lookup variable for Monad; specialize for each type.
///
/// Default is @c std::false_type{}, producing a compile error if @ref bind
/// is called for an unregistered type.
template <class T>
inline constexpr auto monad = std::false_type{};

/// Restricted @c Impl concept for Monad: satisfied when @p Impl supplies
/// the one minimal complete basis @ref derive_monad admits today — @c pure
/// and @c bind.
///
/// Monad's other complete bases — @c pure with @c fmap and @c join, and
/// @c pure with @c kleisli — are deliberately not admitted here; extending
/// this concept to accept them is separate, unscheduled work. This is the
/// MINIMAL pragma to @ref monad_object's class declaration: @c fmap,
/// @c join, @c then, @c apply, @c kleisli and @c as_functor are all derived
/// and belong to @ref monad_object alone.
template <class Impl, class Context>
concept monad_impl = requires(Impl const &impl, Context const &context,
                              element_type_t<Context> const &element) {
    impl.pure(element);
    impl.bind(context, detail::probe_witness<Context>{});
};

/// Deep object concept for a Monad object over @p Context: satisfied when
/// @p Obj provides the whole object surface — @c pure, @c bind, @c fmap,
/// @c join, @c then, @c kleisli and @c as_functor. @c apply is required only
/// where the context can hold a callable, mirroring the condition on the
/// member itself; a context that cannot is not thereby a worse monad.
///
/// @c kleisli is probed because the class surface names it, but its presence
/// is never load-bearing evidence here: @ref derive_monad's @c kleisli is
/// genuinely unconstrained, its @c bind call living inside a returned
/// closure whose argument type is unknown until the closure is invoked. It
/// is @c bind, @c join and @c fmap that carry a real either-basis condition
/// and do the discriminating.
template <class Obj, class Context>
concept monad_object =
    requires(Obj const &obj, Context const &context,
             element_type_t<Context> const &element) {
        obj.pure(element);
        obj.bind(context, detail::probe_witness<Context>{});
        obj.fmap(detail::probe_witness<element_type_t<Context>>{}, context);
        obj.join(obj.pure(context));
        obj.then(context, context);
        obj.kleisli(detail::probe_witness<Context>{},
                    detail::probe_witness<Context>{});
        obj.as_functor();
    } &&
    (!requires(Obj const &obj) {
        obj.pure(detail::probe_witness<element_type_t<Context>>{});
    } || requires(Obj const &obj, Context const &context) {
        obj.apply(obj.pure(detail::probe_witness<element_type_t<Context>>{}),
                  context);
    });

/// Operation object for the @c bind primitive.
///
/// Deduces the monadic type from the first argument and dispatches through
/// @c monad<M>. The NTTP @c TC may be pinned explicitly.
struct bind_fn {
    /// Sequences @p m with @p f, which runs only if @p m succeeded.
    ///
    /// @tparam M  Monadic type (deduced, and what the lookup keys on).
    /// @tparam F  Callable, @c Value -> @c Monadic<U>.
    /// @tparam TC Typeclass instance (NTTP, defaults to lookup).
    template <class M, class F, const auto &TC = monad<std::remove_cvref_t<M>>>
    constexpr auto operator()(M &&m, F &&f) const {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Monad instance for this type. Specialize "
                      "smd::kit::foundation::monad<T> with an object "
                      "providing pure(value) and bind(m, f).");
        return TC.bind(std::forward<M>(m), std::forward<F>(f));
    }
};

/// Global operation object for Monad's @c bind.
inline constexpr bind_fn bind{};

/// Operation object for the derived @c join operation.
struct join_fn {
    /// Collapses one layer of nesting in @p nested.
    ///
    /// @tparam MM A monadic value whose value type is itself monadic.
    /// @tparam TC Typeclass instance (NTTP, defaults to lookup).
    template <class MM, const auto &TC = monad<std::remove_cvref_t<MM>>>
    constexpr auto operator()(MM &&nested) const {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Monad instance for this type. Specialize "
                      "smd::kit::foundation::monad<T> with an object "
                      "providing pure(value) and bind(m, f).");
        return TC.join(std::forward<MM>(nested));
    }
};

/// Global operation object for Monad's @c join.
inline constexpr join_fn join{};

/// Operation object for the derived @c then operation.
struct then_fn {
    /// Sequences @p first and @p second, yielding @p second's value.
    ///
    /// @tparam M  First monadic value type (deduced, and what keys the lookup).
    /// @tparam N  Second monadic value type.
    /// @tparam TC Typeclass instance (NTTP, defaults to lookup).
    template <class M, class N, const auto &TC = monad<std::remove_cvref_t<M>>>
    constexpr auto operator()(M &&first, N &&second) const {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Monad instance for this type. Specialize "
                      "smd::kit::foundation::monad<T> with an object "
                      "providing pure(value) and bind(m, f).");
        return TC.then(std::forward<M>(first), std::forward<N>(second));
    }
};

/// Global operation object for Monad's @c then.
inline constexpr then_fn then{};

/// Operation object for the derived @c kleisli composition.
struct kleisli_fn {
    /// Composes @p f and @p g as `f >=> g`, keyed on @p M, the monadic type
    /// the composition runs in. There is no argument to deduce it from, so
    /// it is an explicit template argument.
    ///
    /// @tparam M  The monadic type (explicit template argument required).
    /// @tparam F  Callable, @c A -> @c Monadic<B>.
    /// @tparam G  Callable, @c B -> @c Monadic<C>.
    /// @tparam TC Typeclass instance (NTTP, defaults to lookup).
    template <class M, class F, class G,
              const auto &TC = monad<std::remove_cvref_t<M>>>
    constexpr auto operator()(F &&f, G &&g) const {
        static_assert(has_instance_v<decltype(TC)>,
                      "No Monad instance for this type. Specialize "
                      "smd::kit::foundation::monad<T> with an object "
                      "providing pure(value) and bind(m, f).");
        return TC.kleisli(std::forward<F>(f), std::forward<G>(g));
    }
};

/// Global operation object for Monad's @c kleisli.
inline constexpr kleisli_fn kleisli{};

} // namespace smd::kit::foundation

#endif
