// src/smd/cl/foundation/functor.test.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test (step R8): the substantive Functor law tests moved to
// src/smd/kit/foundation/functor.test.cpp with the definition. This file
// only verifies that the forwarding shim compiles and that the forwarded
// names are usable from smd::cl::foundation exactly as before.

#include <smd/cl/foundation/functor.hpp>
#include <smd/cl/foundation/functor.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <utility>

using smd::cl::foundation::derive_functor;
using smd::cl::foundation::fmap;
using smd::cl::foundation::functor;

TEST_CASE("FunctorShimTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

template <class T>
struct box {
    T value;
};

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

// functor now lives in smd::kit::foundation (step R8): a
// specialization must be declared where the primary template actually
// lives, not merely where a using-declaration makes its name callable.
namespace smd::kit::foundation {
template <class T>
inline constexpr auto functor<box<T>> = box_functor_map{};
}

TEST_CASE("FunctorShimTest - ForwardedFmapCpoWorks") {
    auto b = box<int>{3};
    auto b2 = fmap([](int x) { return x * 3; }, b);
    CHECK(b2.value == 9);
}
