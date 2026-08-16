// src/smd/cl/foundation/applicative.test.cpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test (step R8): the substantive Applicative law tests moved to
// src/smd/kit/foundation/applicative.test.cpp with the definition. This
// file only verifies that the forwarding shim compiles and that the
// forwarded names are usable from smd::cl::foundation exactly as before.

#include <smd/cl/foundation/applicative.hpp>
#include <smd/cl/foundation/applicative.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <string>
#include <utility>

using smd::cl::foundation::applicative;
using smd::cl::foundation::applicative_typeclass;
using smd::cl::foundation::invoke;

TEST_CASE("ApplicativeShimTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

template <class T>
struct logged {
    std::string log;
    T value;
};

struct logged_applicative_impl {
    template <class V>
    constexpr auto pure(this auto &&, V &&val)
        -> logged<std::remove_cvref_t<V>> {
        return logged<std::remove_cvref_t<V>>{"", std::forward<V>(val)};
    }

    template <class FW, class AW>
    constexpr auto apply(this auto &&, FW &&func_logged, AW &&arg_logged) {
        using result_type = std::invoke_result_t<decltype(func_logged.value),
                                                 decltype(arg_logged.value)>;
        return logged<result_type>{
            func_logged.log + arg_logged.log,
            std::invoke(func_logged.value, arg_logged.value)};
    }
};

struct logged_applicative_map : applicative<logged_applicative_impl> {
    using logged_applicative_impl::apply;
    using logged_applicative_impl::pure;
};

} // namespace

// applicative_typeclass now lives in smd::kit::foundation (step R8): a
// specialization must be declared where the primary template actually
// lives, not merely where a using-declaration makes its name callable.
namespace smd::kit::foundation {
template <class T>
inline constexpr auto applicative_typeclass<logged<T>> =
    logged_applicative_map{};
}

TEST_CASE("ApplicativeShimTest - ForwardedInvokeCpoWorks") {
    auto a = logged<int>{"x:", 3};
    auto b = logged<int>{"y:", 4};
    auto result = invoke([](int x, int y) { return x + y; }, a, b);
    CHECK(result.log == "x:y:");
    CHECK(result.value == 7);
}
