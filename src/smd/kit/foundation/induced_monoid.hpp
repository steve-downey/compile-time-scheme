// src/smd/kit/foundation/induced_monoid.hpp                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Picked up from beman.transpose's monad-induced-monoids step
// (include/beman/transpose/induced_monoid.hpp).
//
// EVIDENCE, NOT VOCABULARY. These are two monoids the kit's own typeclass
// instances induce, not facilities anything here needs: a Monad induces a
// monoid on its Kleisli arrows, and an Applicative induces a monoid on its
// context, lifted from a monoid on the element. Both are named carriers with
// their own Monoid registration, and neither registers anything for a raw
// carrier. The header exists to show the mechanism carries the induction
// without strain, and to record, once, the thing worth not rediscovering:
// the categorical statement "a monad is a monoid in the category of
// endofunctors" is inexpressible at monoid<T>, whose tensor is a product and
// whose carrier is a type rather than a type constructor. The Kleisli
// endomorphism monoid below is the value-level statement that survives that
// gap -- evidence of the theorem, not the theorem itself.
//
// DELIBERATE DIVERGENCE FROM TRANSPOSE. Transpose erases its Kleisli arrows
// through std::function, because a monoid's carrier must close under combine
// and a bare lambda type does not. std::function is neither constexpr nor
// allocation-free, and this kit is both, so the carrier here is a bounded
// sequence of arrows under concatenation instead, interpreted by running
// them through bind. That is the free monoid on the arrows, and running it
// is Kleisli composition: run(fs ++ gs) == run(fs) >=> run(gs), by the
// monad's own left-identity law. The laws are the same laws; only the
// representation differs, and it differs for the reason this whole tree
// exists.
#ifndef SRC_SMD_KIT_FOUNDATION_INDUCED_MONOID_HPP
#define SRC_SMD_KIT_FOUNDATION_INDUCED_MONOID_HPP

#include <smd/kit/foundation/applicative.hpp>
#include <smd/kit/foundation/monad.hpp>
#include <smd/kit/foundation/monoid.hpp>
#include <smd/kit/foundation/static_vector.hpp>
#include <smd/kit/foundation/typeclass_base.hpp>

#include <cstddef>
#include <utility>

namespace smd::kit::foundation {

/// The monoid a Monad induces on its Kleisli arrows @c A -> @c M<A>.
///
/// @p MonadObject is a type parameter rather than a value: every typeclass
/// object in this kit is stateless and empty, so a registration that has no
/// member to keep an instance in default-constructs a fresh one wherever it
/// needs one. That construction is free, not a workaround.
///
/// The carrier holds the arrows rather than their composition. A composed
/// closure would have a fresh type per composition, and a monoid's carrier
/// has to close under @c combine; erasing the closure is one way out and
/// costs an allocation, holding the sequence is the other and costs a
/// capacity bound. This kit already pays capacity bounds everywhere else.
///
/// @tparam MonadObject The Monad instance object the arrows run in.
/// @tparam A           The element type the arrows take and produce.
/// @tparam Capacity    How many arrows one carrier may hold.
template <class MonadObject, class A, std::size_t Capacity = 8>
struct kleisli_endo {
    /// What @c MonadObject::pure returns for an @p A: the context every
    /// arrow in this monoid returns into.
    using result_type = decltype(std::declval<MonadObject const &>().pure(
        std::declval<A const &>()));

    /// One Kleisli arrow. A function pointer, not a closure: a closure has
    /// no type the carrier could name, and a capturing one could not be a
    /// constant expression here anyway.
    using arrow_type = auto (*)(A const &) -> result_type;

    static_vector<arrow_type, static_cast<int>(Capacity)> arrows{};

    /// Runs the arrows in order, threading the value through @c bind. An
    /// empty carrier runs as @c pure, which is what makes the identity below
    /// the identity.
    constexpr auto operator()(A const &a) const -> result_type {
        MonadObject const m{};
        result_type accumulated = m.pure(a);
        for (arrow_type const &arrow : arrows) {
            accumulated = m.bind(accumulated, arrow);
        }
        return accumulated;
    }

    // HIDDEN FRIEND
    friend constexpr auto operator==(kleisli_endo const &lhs,
                                     kleisli_endo const &rhs) -> bool {
        return lhs.arrows == rhs.arrows;
    }
};

/// Monoid instance object for @ref kleisli_endo: identity is @c pure, and
/// combine is forward Kleisli composition.
///
/// The Kleisli-form monad laws — @c pure is the two-sided unit of @c >=>,
/// and @c >=> is associative — are exactly this monoid's laws, which is the
/// whole reason to name the carrier. It is the value-level statement of "a
/// monad is a monoid in the category of endofunctors", and the only form of
/// that statement this kit can make.
template <class MonadObject, class A, std::size_t Capacity>
struct kleisli_endo_monoid_t {
    using carrier = kleisli_endo<MonadObject, A, Capacity>;

    /// The empty arrow sequence, which runs as @c pure.
    [[nodiscard]] constexpr auto identity() const -> carrier { return {}; }

    /// Concatenation, which interprets as @c lhs @c >=> @c rhs.
    [[nodiscard]] constexpr auto combine(carrier const &lhs,
                                         carrier const &rhs) const -> carrier {
        carrier composed{lhs};
        for (auto const &arrow : rhs.arrows) {
            composed.arrows.push_back(arrow);
        }
        return composed;
    }
};

/// Registers the induced instance. Named and reachable, and it claims
/// nothing for any raw carrier — the registration is on @ref kleisli_endo
/// itself.
template <class MonadObject, class A, std::size_t Capacity>
inline constexpr auto monoid<kleisli_endo<MonadObject, A, Capacity>> =
    kleisli_endo_monoid_t<MonadObject, A, Capacity>{};

/// The monoid an Applicative induces on its context @p Context, lifted from
/// a registered monoid on the element type.
///
/// @p ApplicativeObject is a type parameter for the same reason
/// @c MonadObject is above.
///
/// @tparam ApplicativeObject The Applicative instance object.
/// @tparam Context           The context being lifted, e.g. @c result<sum<int>>.
template <class ApplicativeObject, class Context>
struct lifted {
    Context value;

    // HIDDEN FRIEND
    friend constexpr auto operator==(lifted const &, lifted const &)
        -> bool = default;
};

/// Monoid instance object for @ref lifted: identity lifts the element
/// monoid's identity with @c pure, and combine lifts its combine with
/// @c invoke.
///
/// It uses @c invoke rather than @c apply because @c invoke is the operation
/// that works for every context, including one that cannot hold a callable;
/// @c apply requires the context to be able to hold the function.
///
/// Note what the element type has to be. @c lifted<map, result<sum<int>>>
/// works and @c lifted<map, result<int>> does not, because nothing registers
/// a monoid for a bare @c int — which is the no-numeric-defaults decision
/// doing its job at the one place that would otherwise have picked addition
/// silently.
template <class ApplicativeObject, class Context>
struct lifted_monoid_t {
    using carrier = lifted<ApplicativeObject, Context>;
    using element = element_type_t<Context>;

    [[nodiscard]] constexpr auto identity() const -> carrier {
        return carrier{ApplicativeObject{}.pure(monoid<element>.identity())};
    }

    [[nodiscard]] constexpr auto combine(carrier const &lhs,
                                         carrier const &rhs) const -> carrier {
        return carrier{ApplicativeObject{}.invoke(
            [](element const &a, element const &b) {
                return monoid<element>.combine(a, b);
            },
            lhs.value, rhs.value)};
    }
};

/// Registers the induced instance, on @ref lifted itself and on no raw
/// carrier.
template <class ApplicativeObject, class Context>
inline constexpr auto monoid<lifted<ApplicativeObject, Context>> =
    lifted_monoid_t<ApplicativeObject, Context>{};

} // namespace smd::kit::foundation

#endif
