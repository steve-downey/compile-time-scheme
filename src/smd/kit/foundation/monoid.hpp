// src/smd/kit/foundation/monoid.hpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Extracted in step R8 from smd::cl::foundation::monoid. Decision 2's
// original cut left this in cl on the grounds that neither smdscheme nor
// compile-time-forth had a Foldable vocabulary to need it. That reasoning
// was overturned: the absence reflects the algebraic refactoring never
// having been done on the Forth side, not a fact about this code's
// generality. The corrected standard is docs/cl-rebuild-plan.md §5's own
// test — generic in shape and free of language-specific types — which this
// file passes outright: no type here names anything from a parser, a
// reader, or an AST.
//
// The named carriers and the unregistered-by-default lookup are picked up
// from beman.transpose's named-monoid-carriers and numeric-monoid-defaults
// steps.
#ifndef SRC_SMD_KIT_FOUNDATION_MONOID_HPP
#define SRC_SMD_KIT_FOUNDATION_MONOID_HPP

#include <smd/kit/foundation/typeclass_base.hpp>

#include <concepts>
#include <limits>
#include <type_traits>

namespace smd::kit::foundation {

/// A monoid instance object over carrier @p A: a concept-map-like record of
/// the two monoid operations. Instances are passed explicitly to generic
/// algorithms such as @c fold_map (per the Typeclass Design rules: "generic
/// algorithms may also accept explicit or NTTP-pinned instance objects").
///
/// The unit is @c identity, not @c empty. One namespace holds the whole
/// typeclass family, and @c empty is Foldable's predicate there — the
/// reading C++ already has for the name.
///
/// Laws (tested in monoid.test.cpp):
/// - identity:      combine(identity(), a) == a == combine(a, identity())
/// - associativity: combine(combine(a, b), c) == combine(a, combine(b, c))
template <class M, class A>
concept monoid_for = requires(M const &m, A a, A b) {
    { m.identity() } -> std::convertible_to<A>;
    { m.combine(a, b) } -> std::convertible_to<A>;
};

/// Typeclass lookup variable for Monoid; specialize for each carrier that
/// has a canonical instance.
///
/// Default is @c std::false_type{}, and for @c int, @c bool and every other
/// number it stays that way. A carrier gets a registration only where one
/// instance is so obviously right that it can be the default; where several
/// compete — addition or multiplication, conjunction or disjunction — the
/// choice is spelled by choosing one of the named carriers below, not made
/// on the caller's behalf. Registering @c monoid<int> as addition would
/// decide that for every caller and be invisible at every call site.
template <class A>
inline constexpr auto monoid = std::false_type{};

/// The trivial one-value carrier for effect-only folds: fold_map with
/// @ref unit_monoid visits every element for its side effect on captured
/// state and combines nothing.
struct unit {
    // HIDDEN FRIEND
    friend constexpr auto operator==(unit, unit) -> bool = default;
};

/// Monoid instance object: @ref unit under its only operation.
struct unit_monoid_t {
    [[nodiscard]] constexpr auto identity() const -> unit;
    [[nodiscard]] constexpr auto combine(unit, unit) const -> unit;
};

/// Monoid instance object: @p T under addition, with identity @c T{}.
template <class T>
struct sum_monoid_t {
    [[nodiscard]] constexpr auto identity() const -> T { return T{}; }
    [[nodiscard]] constexpr auto combine(T a, T b) const -> T { return a + b; }
};

/// Monoid instance object: @p T under multiplication, with identity @c T{1}.
template <class T>
struct product_monoid_t {
    [[nodiscard]] constexpr auto identity() const -> T { return T{1}; }
    [[nodiscard]] constexpr auto combine(T a, T b) const -> T { return a * b; }
};

/// Monoid instance object: @p T under @c max.
///
/// The identity is the saturating bottom of @p T — negative infinity where
/// the type has one, @c lowest() where it does not. No adjoined identity
/// element, which would need a wider type than @p T.
template <class T>
struct maximum_monoid_t {
    [[nodiscard]] constexpr auto identity() const -> T {
        if constexpr (std::numeric_limits<T>::has_infinity) {
            return -std::numeric_limits<T>::infinity();
        } else {
            return std::numeric_limits<T>::lowest();
        }
    }
    [[nodiscard]] constexpr auto combine(T a, T b) const -> T {
        return a < b ? b : a;
    }
};

/// Monoid instance object: @p T under @c min, with the saturating top of
/// @p T as identity. See @ref maximum_monoid_t.
template <class T>
struct minimum_monoid_t {
    [[nodiscard]] constexpr auto identity() const -> T {
        if constexpr (std::numeric_limits<T>::has_infinity) {
            return std::numeric_limits<T>::infinity();
        } else {
            return std::numeric_limits<T>::max();
        }
    }
    [[nodiscard]] constexpr auto combine(T a, T b) const -> T {
        return b < a ? b : a;
    }
};

/// Monoid instance object: bool under conjunction with identity true.
struct all_monoid_t {
    [[nodiscard]] constexpr auto identity() const -> bool;
    [[nodiscard]] constexpr auto combine(bool a, bool b) const -> bool;
};

/// Monoid instance object: bool under disjunction with identity false.
struct any_monoid_t {
    [[nodiscard]] constexpr auto identity() const -> bool;
    [[nodiscard]] constexpr auto combine(bool a, bool b) const -> bool;
};

constexpr auto unit_monoid_t::identity() const -> unit { return {}; }
constexpr auto unit_monoid_t::combine(unit, unit) const -> unit { return {}; }

constexpr auto all_monoid_t::identity() const -> bool { return true; }
constexpr auto all_monoid_t::combine(bool a, bool b) const -> bool {
    return a && b;
}

constexpr auto any_monoid_t::identity() const -> bool { return false; }
constexpr auto any_monoid_t::combine(bool a, bool b) const -> bool {
    return a || b;
}

/// Global instance objects, ready to pass to fold_map.
inline constexpr unit_monoid_t unit_monoid{};
inline constexpr all_monoid_t all_monoid{};
inline constexpr any_monoid_t any_monoid{};

/// @copydoc sum_monoid_t
template <class T>
inline constexpr sum_monoid_t<T> sum_monoid{};

/// @copydoc product_monoid_t
template <class T>
inline constexpr product_monoid_t<T> product_monoid{};

/// @copydoc maximum_monoid_t
template <class T>
inline constexpr maximum_monoid_t<T> maximum_monoid{};

/// @copydoc minimum_monoid_t
template <class T>
inline constexpr minimum_monoid_t<T> minimum_monoid{};

// -- Named carriers --
//
// A number is not a monoid; a number under a chosen operation is. These
// carriers are how that choice is spelled in a type, for the algorithms that
// look an instance up rather than being handed one. Each is registered under
// its own name, and no bare number or bool is registered at all.

/// @p T under addition, as a carrier with a registered Monoid instance.
template <class T>
struct sum {
    T value{};

    // HIDDEN FRIEND
    friend constexpr auto operator==(sum const &, sum const &) -> bool =
        default;
};

/// @p T under multiplication, as a carrier with a registered instance.
template <class T>
struct product {
    T value{T{1}};

    // HIDDEN FRIEND
    friend constexpr auto operator==(product const &, product const &) -> bool =
        default;
};

/// @p T under @c max, as a carrier with a registered instance.
template <class T>
struct maximum {
    T value{maximum_monoid_t<T>{}.identity()};

    // HIDDEN FRIEND
    friend constexpr auto operator==(maximum const &, maximum const &) -> bool =
        default;
};

/// @p T under @c min, as a carrier with a registered instance.
template <class T>
struct minimum {
    T value{minimum_monoid_t<T>{}.identity()};

    // HIDDEN FRIEND
    friend constexpr auto operator==(minimum const &, minimum const &) -> bool =
        default;
};

/// bool under disjunction, as a carrier with a registered instance.
struct any {
    bool value{false};

    // HIDDEN FRIEND
    friend constexpr auto operator==(any, any) -> bool = default;
};

/// bool under conjunction, as a carrier with a registered instance.
struct all {
    bool value{true};

    // HIDDEN FRIEND
    friend constexpr auto operator==(all, all) -> bool = default;
};

/// Lifts an instance object over @p T to the one-field carrier @p Carrier.
template <class Carrier, class Instance>
struct wrapped_monoid_t {
    [[nodiscard]] constexpr auto identity() const -> Carrier {
        return Carrier{Instance{}.identity()};
    }
    [[nodiscard]] constexpr auto combine(Carrier a, Carrier b) const
        -> Carrier {
        return Carrier{Instance{}.combine(a.value, b.value)};
    }
};

/// Registers @ref unit — the one carrier whose instance is canonical because
/// it is the only one there is.
template <>
inline constexpr auto monoid<unit> = unit_monoid_t{};

template <class T>
inline constexpr auto monoid<sum<T>> =
    wrapped_monoid_t<sum<T>, sum_monoid_t<T>>{};

template <class T>
inline constexpr auto monoid<product<T>> =
    wrapped_monoid_t<product<T>, product_monoid_t<T>>{};

template <class T>
inline constexpr auto monoid<maximum<T>> =
    wrapped_monoid_t<maximum<T>, maximum_monoid_t<T>>{};

template <class T>
inline constexpr auto monoid<minimum<T>> =
    wrapped_monoid_t<minimum<T>, minimum_monoid_t<T>>{};

template <>
inline constexpr auto monoid<any> = wrapped_monoid_t<any, any_monoid_t>{};

template <>
inline constexpr auto monoid<all> = wrapped_monoid_t<all, all_monoid_t>{};

} // namespace smd::kit::foundation

#endif
