// src/smd/kit/foundation/functor.test.cpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Moved in step R8 from src/smd/cl/foundation/functor.test.cpp, which was
// itself adapted by copy from compile-time-forth
// (src/smd/forth/foundation/functor.test.cpp).

#include <smd/kit/foundation/functor.hpp>
#include <smd/kit/foundation/functor.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <utility>

using smd::kit::foundation::derive_functor;
using smd::kit::foundation::fmap;
using smd::kit::foundation::functor;

TEST_CASE("FunctorTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

/// A minimal boxed value, local to this test, used to exercise the @ref
/// functor CRTP base and the @c fmap customization-point object without
/// pulling in a production type.
template <class T>
struct box {
    /// The carried type, which @c element_type_t reads to key the deep
    /// object concepts.
    using value_type = T;

    T value;

    friend constexpr auto operator==(box const &, box const &)
        -> bool = default;
};

/// Functor @c Impl for @c box<T>: maps a function over the value.
struct box_functor_impl {
    template <class F, class T>
    constexpr auto fmap(this auto &&, F &&func, box<T> const &b) {
        return box{std::forward<F>(func)(b.value)};
    }
};

struct box_functor_map : derive_functor<box_functor_impl> {
    using box_functor_impl::fmap;
};

} // namespace

namespace smd::kit::foundation {
template <class T>
inline constexpr auto functor<box<T>> = box_functor_map{};
}

TEST_CASE("FunctorTest - CrtpFmap") {
    box_functor_map m;
    auto b = box<int>{5};
    auto b2 = m.fmap([](int x) { return x * 2; }, b);
    CHECK(b2.value == 10);
}

TEST_CASE("FunctorTest - CrtpReplace") {
    box_functor_map m;
    auto b = box<int>{5};
    auto b2 = m.replace(b, 99);
    CHECK(b2.value == 99);
}

TEST_CASE("FunctorTest - TypeclassLookup") {
    const auto &tc = functor<box<int>>;
    static_assert(
        !std::is_same_v<std::remove_cvref_t<decltype(tc)>, std::false_type>);

    auto b = box<int>{7};
    auto b2 = tc.fmap([](int x) { return x + 1; }, b);
    CHECK(b2.value == 8);
}

TEST_CASE("FunctorTest - FmapCpo") {
    auto b = box<int>{3};
    auto b2 = fmap([](int x) { return x * 3; }, b);
    CHECK(b2.value == 9);
}

// --- Native preference on a derived operation. ----------------------------
//
// Wrapping fills gaps; it never shadows a better operation the instance
// author supplied. An Impl that writes its own replace gets its own replace,
// not the fmap-based derivation.

namespace {

/// A box functor whose Impl supplies a native replace that leaves a mark,
/// so the two paths are distinguishable at the value level.
struct marking_functor_impl {
    template <class F, class T>
    constexpr auto fmap(this auto &&, F &&func, box<T> const &b) {
        return box{std::forward<F>(func)(b.value)};
    }

    template <class T, class U>
    constexpr auto replace(this auto &&, box<T> const &, U &&replacement) {
        return box<std::remove_cvref_t<U>>{
            static_cast<std::remove_cvref_t<U>>(replacement + 100)};
    }
};

struct marking_functor_map : derive_functor<marking_functor_impl> {
    using marking_functor_impl::fmap;
    using marking_functor_impl::replace;
};

/// The same functor without a native replace, so the derivation runs.
struct plain_functor_map : derive_functor<box_functor_impl> {};

} // namespace

static_assert(marking_functor_map{}.replace(box<int>{1}, 7) == box<int>{107});
static_assert(plain_functor_map{}.replace(box<int>{1}, 7) == box<int>{7});

// --- The two concepts. ----------------------------------------------------

static_assert(smd::kit::foundation::functor_impl<box_functor_impl, box<int>>);
static_assert(smd::kit::foundation::functor_object<box_functor_map, box<int>>);
static_assert(
    smd::kit::foundation::functor_object<plain_functor_map, box<int>>);

// The Impl alone has fmap and no replace: it satisfies the minimal-basis
// concept and fails the object concept. That gap is what the CRTP base is
// for, and checking the derived surface at the gate is what moves the
// failure from three frames deep to the call site.
static_assert(
    !smd::kit::foundation::functor_object<box_functor_impl, box<int>>);
