// src/smd/cl/foundation/alternative.test.cpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test (step R8): the substantive Alternative law tests moved to
// src/smd/kit/foundation/alternative.test.cpp with the definition. This
// file only verifies that the forwarding shim compiles and that the
// forwarded names are usable from smd::cl::foundation exactly as before.

#include <smd/cl/foundation/alternative.hpp>
#include <smd/cl/foundation/alternative.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <string>

using smd::cl::foundation::alt;
using smd::cl::foundation::alternative;
using smd::cl::foundation::alternative_typeclass;

TEST_CASE("AlternativeShimTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

template <class T>
struct logged {
    std::string log;
    T value;
};

template <class T>
struct logged_alternative_impl_t {
    constexpr auto empty(this auto &&) -> logged<T> {
        return logged<T>{"", T{}};
    }

    constexpr auto alt(this auto &&, logged<T> const &a, logged<T> const &b)
        -> logged<T> {
        return {a.log + b.log, b.value};
    }
};

template <class T>
struct logged_alternative_map : alternative<logged_alternative_impl_t<T>> {
    using logged_alternative_impl_t<T>::alt;
    using logged_alternative_impl_t<T>::empty;
};

} // namespace

// alternative_typeclass now lives in smd::kit::foundation (step R8): a
// specialization must be declared where the primary template actually
// lives, not merely where a using-declaration makes its name callable.
namespace smd::kit::foundation {
template <class T>
inline constexpr auto alternative_typeclass<logged<T>> =
    logged_alternative_map<T>{};
}

TEST_CASE("AlternativeShimTest - ForwardedAltCpoWorks") {
    auto a = logged<int>{"a:", 1};
    auto b = logged<int>{"b:", 2};
    auto result = alt(a, b);
    CHECK(result.log == "a:b:");
    CHECK(result.value == 2);
}
