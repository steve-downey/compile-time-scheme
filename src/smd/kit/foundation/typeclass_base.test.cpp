// src/smd/kit/foundation/typeclass_base.test.cpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/kit/foundation/typeclass_base.hpp>
#include <smd/kit/foundation/typeclass_base.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

namespace kf = smd::kit::foundation;

TEST_CASE("TypeclassBaseTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

/// A context with the nested alias @ref element_type reads.
template <class T>
struct box {
    using value_type = T;
    T value;
};

/// Minimal probe standing in for an @c Impl: one stateless operation whose
/// result is a constant expression, so the mechanism below can be exercised
/// with @c STATIC_REQUIRE rather than at run time.
struct probe {
    constexpr auto op(this auto &&) -> int { return 42; }
};

/// A CRTP base over @ref probe, carrying the same private @c impl_of shape
/// the six real typeclass bases carry: @c "protected Impl", plus a private
/// @c impl_of naming @ref kf::impl_ref_t.
template <class Impl>
struct inner : protected Impl {
    template <class Self>
    constexpr auto derived(this Self &&self) {
        return impl_of(self).op();
    }

  private:
    template <class Self>
    static constexpr auto impl_of(Self &&self) -> decltype(auto) {
        return static_cast<kf::impl_ref_t<Impl, Self>>(self);
    }
};

/// The registration shape this tree actually uses: a @c _map publicly
/// deriving from the CRTP base and adding nothing of its own.
struct inner_map : inner<probe> {};

/// A second CRTP base wrapping @ref inner_map — standing in for
/// @c derive_functor<some_monad_map>. @c derived is exposed as this base's
/// own member, forwarding through its own @c impl_of, never as
/// @c "using Impl::derived;". That is the own-member rule: a
/// using-declaration here would let @c inner<probe>'s @c impl_of see a
/// @c self typed as @c outer<inner_map> rather than @c inner_map, and the
/// cast would fail with "'probe' is an inaccessible base of
/// 'outer<inner_map>'".
template <class Impl>
struct outer : protected Impl {
    template <class Self>
    constexpr auto derived(this Self &&self) {
        return impl_of(self).derived();
    }

  private:
    template <class Self>
    static constexpr auto impl_of(Self &&self) -> decltype(auto) {
        return static_cast<kf::impl_ref_t<Impl, Self>>(self);
    }
};

using outer_map = outer<inner_map>;

struct unregistered {};

} // namespace

TEST_CASE("TypeclassBaseTest - ElementTypeExtractsTheElementType") {
    STATIC_REQUIRE(std::is_same_v<kf::element_type_t<box<int>>, int>);
    STATIC_REQUIRE(
        std::is_same_v<kf::element_type_t<box<double> const &>, double>);
}

TEST_CASE("TypeclassBaseTest - HasInstanceSeesPastTheFalseTypeDefault") {
    STATIC_REQUIRE_FALSE(kf::has_instance_v<decltype(std::false_type{})>);
    STATIC_REQUIRE_FALSE(kf::has_instance_v<std::false_type const &>);
    STATIC_REQUIRE(kf::has_instance_v<decltype(inner_map{})>);
}

TEST_CASE("TypeclassBaseTest - AlwaysFalseIsFalse") {
    STATIC_REQUIRE_FALSE(kf::always_false_v<unregistered>);
    STATIC_REQUIRE_FALSE(kf::always_false_v<int, char>);
}

TEST_CASE("TypeclassBaseTest - ImplRefPropagatesConst") {
    STATIC_REQUIRE(std::is_same_v<kf::impl_ref_t<probe, inner<probe> const &>,
                                  probe const &>);
    STATIC_REQUIRE(
        std::is_same_v<kf::impl_ref_t<probe, inner<probe> &>, probe &>);
}

TEST_CASE("TypeclassBaseTest - ImplOfReachesImplOneLevelDeep") {
    STATIC_REQUIRE(inner<probe>{}.derived() == 42);
    STATIC_REQUIRE(inner_map{}.derived() == 42);
}

TEST_CASE("TypeclassBaseTest - ImplOfReachesImplTwoLevelsDeep") {
    STATIC_REQUIRE(outer_map{}.derived() == 42);
}

TEST_CASE("TypeclassBaseTest - ConstPropagatesThroughTheTwoDeepCall") {
    constexpr outer_map const_outer{};
    STATIC_REQUIRE(const_outer.derived() == 42);
}

TEST_CASE("TypeclassBaseTest - ProbeWitnessesReportTheirResultType") {
    // Declared, never defined: these are checked for invocability and
    // return type, never called.
    STATIC_REQUIRE(
        std::is_invocable_r_v<int, kf::detail::probe_witness<int>, box<char>>);
    STATIC_REQUIRE(std::is_invocable_r_v<bool, kf::detail::probe_witness2<bool>,
                                         int, char>);
    STATIC_REQUIRE_FALSE(
        std::is_invocable_v<kf::detail::probe_witness<int>, int, int>);
}
