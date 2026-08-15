// src/smd/cl/foundation/traversable.test.cpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test (step R8, decision 2 corrected): the substantive
// Traversable law tests moved to
// src/smd/kit/foundation/traversable.test.cpp with the definition. This
// file only verifies that the forwarding shim compiles and that the
// forwarded names are usable from smd::cl::foundation exactly as before.

#include <smd/cl/foundation/traversable.hpp>
#include <smd/cl/foundation/traversable.hpp> // test 2nd include OK

#include <smd/cl/foundation/applicative.hpp>
#include <smd/cl/foundation/identity.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <utility>

using smd::cl::foundation::applicative_typeclass;
using smd::cl::foundation::identity;
using smd::cl::foundation::traversable;
using smd::cl::foundation::traversable_typeclass;
using smd::cl::foundation::traverse;

TEST_CASE("TraversableShimTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

template <class T>
struct box {
    T value;

    friend constexpr auto operator==(box const &, box const &)
        -> bool = default;
};

struct box_traversable_impl {
    template <class F, class T>
    constexpr auto traverse(this auto &&, F &&f, box<T> const &b) {
        using effect_type =
            std::remove_cvref_t<std::invoke_result_t<F &, T const &>>;
        using B = typename effect_type::value_type;
        auto const &tc = applicative_typeclass<effect_type>;
        return tc.invoke([](B mapped) { return box<B>{std::move(mapped)}; },
                         f(b.value));
    }
};

struct box_traversable_map : traversable<box_traversable_impl> {
    using box_traversable_impl::traverse;
};

} // namespace

// traversable_typeclass now lives in smd::kit::foundation (step R8): a
// specialization must be declared where the primary template actually
// lives, not merely where a using-declaration makes its name callable.
namespace smd::kit::foundation {
template <class T>
inline constexpr auto traversable_typeclass<box<T>> = box_traversable_map{};
}

TEST_CASE("TraversableShimTest - ForwardedTraverseCpoWorks") {
    auto traversed =
        traverse([](int x) { return identity<int>{x * 2}; }, box<int>{3});
    CHECK(traversed == identity<box<int>>{box<int>{6}});
}
