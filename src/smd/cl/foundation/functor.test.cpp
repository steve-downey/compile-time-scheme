// src/smd/cl/foundation/functor.test.cpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Adapted by copy from compile-time-forth (smd::forth::foundation):
// src/smd/forth/foundation/functor.test.cpp. No test existed in
// compile-time-scheme's copy of the foundation.

#include <smd/cl/foundation/functor.hpp>
#include <smd/cl/foundation/functor.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <utility>

using smd::cl::foundation::fmap;
using smd::cl::foundation::functor;
using smd::cl::foundation::functor_typeclass;

TEST_CASE("FunctorTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

/// A minimal boxed value, local to this test, used to exercise the @ref
/// functor CRTP base and the @c fmap customization-point object without
/// pulling in a production type.
template <class T>
struct box {
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

struct box_functor_map : functor<box_functor_impl> {
    using box_functor_impl::fmap;
};

} // namespace

namespace smd::cl::foundation {
template <class T>
inline constexpr auto functor_typeclass<box<T>> = box_functor_map{};
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
    const auto &tc = functor_typeclass<box<int>>;
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
