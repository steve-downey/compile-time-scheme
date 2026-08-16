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
#ifndef SRC_SMD_KIT_FOUNDATION_MONOID_HPP
#define SRC_SMD_KIT_FOUNDATION_MONOID_HPP

#include <concepts>

namespace smd::kit::foundation {

/// A monoid instance object over carrier @p A: a concept-map-like record of
/// the two monoid operations. Instances are passed explicitly to generic
/// algorithms such as @c fold_map (per the Typeclass Design rules: "generic
/// algorithms may also accept explicit or NTTP-pinned instance objects").
///
/// Laws (tested in monoid.test.cpp):
/// - identity:      combine(empty(), a) == a == combine(a, empty())
/// - associativity: combine(combine(a, b), c) == combine(a, combine(b, c))
template <class M, class A>
concept monoid_for = requires(M const &m, A a, A b) {
    { m.empty() } -> std::convertible_to<A>;
    { m.combine(a, b) } -> std::convertible_to<A>;
};

/// The trivial one-value carrier for effect-only folds: fold_map with
/// @ref unit_monoid visits every element for its side effect on captured
/// state and combines nothing.
struct unit {
    // HIDDEN FRIEND
    friend constexpr auto operator==(unit, unit) -> bool = default;
};

/// Monoid instance object: @ref unit under its only operation.
struct unit_monoid_t {
    [[nodiscard]] constexpr auto empty() const -> unit;
    [[nodiscard]] constexpr auto combine(unit, unit) const -> unit;
};

/// Monoid instance object: int under addition with identity 0.
struct sum_monoid_t {
    [[nodiscard]] constexpr auto empty() const -> int;
    [[nodiscard]] constexpr auto combine(int a, int b) const -> int;
};

/// Monoid instance object: bool under conjunction with identity true.
struct all_monoid_t {
    [[nodiscard]] constexpr auto empty() const -> bool;
    [[nodiscard]] constexpr auto combine(bool a, bool b) const -> bool;
};

/// Monoid instance object: bool under disjunction with identity false.
struct any_monoid_t {
    [[nodiscard]] constexpr auto empty() const -> bool;
    [[nodiscard]] constexpr auto combine(bool a, bool b) const -> bool;
};

constexpr auto unit_monoid_t::empty() const -> unit { return {}; }
constexpr auto unit_monoid_t::combine(unit, unit) const -> unit { return {}; }

constexpr auto sum_monoid_t::empty() const -> int { return 0; }
constexpr auto sum_monoid_t::combine(int a, int b) const -> int {
    return a + b;
}

constexpr auto all_monoid_t::empty() const -> bool { return true; }
constexpr auto all_monoid_t::combine(bool a, bool b) const -> bool {
    return a && b;
}

constexpr auto any_monoid_t::empty() const -> bool { return false; }
constexpr auto any_monoid_t::combine(bool a, bool b) const -> bool {
    return a || b;
}

/// Global instance objects, ready to pass to fold_map.
inline constexpr unit_monoid_t unit_monoid{};
inline constexpr sum_monoid_t sum_monoid{};
inline constexpr all_monoid_t all_monoid{};
inline constexpr any_monoid_t any_monoid{};

} // namespace smd::kit::foundation

#endif
