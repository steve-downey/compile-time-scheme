// src/smd/cl/foundation/monad.test.cpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test: the substantive Monad law tests live in
// src/smd/kit/foundation/monad.test.cpp with the definition. This file only
// verifies that the forwarding shim compiles and that the forwarded names
// are usable from smd::cl::foundation.

#include <smd/cl/foundation/monad.hpp>
#include <smd/cl/foundation/monad.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <utility>

using smd::cl::foundation::bind;
using smd::cl::foundation::join;
using smd::cl::foundation::monad;
using smd::cl::foundation::monad_typeclass;

TEST_CASE("MonadShimTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

template <class T>
struct box {
    T value;
};

struct box_monad_impl {
    template <class V>
    constexpr auto pure(this auto &&, V &&value) -> box<std::remove_cvref_t<V>> {
        return box<std::remove_cvref_t<V>>{std::forward<V>(value)};
    }

    template <class T, class F>
    constexpr auto bind(this auto &&, box<T> const &b, F &&f) {
        return std::forward<F>(f)(b.value);
    }
};

struct box_monad_map : monad<box_monad_impl> {
    using box_monad_impl::bind;
    using box_monad_impl::pure;
};

} // namespace

// monad_typeclass lives in smd::kit::foundation: a specialization must be
// declared where the primary template actually lives, not merely where a
// using-declaration makes its name callable.
namespace smd::kit::foundation {
template <class T>
inline constexpr auto monad_typeclass<box<T>> = box_monad_map{};
}

TEST_CASE("MonadShimTest - ForwardedBindCpoWorks") {
    auto const b = bind(box<int>{3}, [](int x) { return box<int>{x * 3}; });
    CHECK(b.value == 9);
}

TEST_CASE("MonadShimTest - ForwardedJoinCpoWorks") {
    auto const b = join(box<box<int>>{box<int>{7}});
    CHECK(b.value == 7);
}
