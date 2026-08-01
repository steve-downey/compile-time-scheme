// src/smd/cl/foundation/applicative.test.cpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Adapted by copy from compile-time-forth (smd::forth::foundation):
// src/smd/forth/foundation/applicative.test.cpp. No test existed in
// compile-time-scheme's copy of the foundation.

#include <smd/cl/foundation/applicative.hpp>
#include <smd/cl/foundation/applicative.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <string>
#include <type_traits>
#include <utility>

using smd::cl::foundation::applicative;
using smd::cl::foundation::applicative_typeclass;
using smd::cl::foundation::invoke;

TEST_CASE("ApplicativeTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

/// A minimal writer-like value, local to this test: a value paired with a
/// string log that accumulates across `apply`. Exercises the applicative
/// CRTP base's derived `invoke`/`lift_a2`/`ap`/`discard_first`/
/// `discard_second` without pulling in a production type.
template <class T>
struct logged {
    std::string log;
    T value;
};

/// Applicative @c Impl for @c logged<T>.
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

namespace smd::cl::foundation {
template <class T>
inline constexpr auto applicative_typeclass<logged<T>> =
    logged_applicative_map{};
}

TEST_CASE("ApplicativeTest - Pure") {
    logged_applicative_map m;
    auto w = m.pure(42);
    CHECK(w.log.empty());
    CHECK(w.value == 42);
}

TEST_CASE("ApplicativeTest - Apply") {
    logged_applicative_map m;
    auto func_w = logged<int (*)(int)>{"f:", [](int x) { return x + 1; }};
    auto arg_w = logged<int>{"a:", 10};
    auto result = m.apply(func_w, arg_w);
    CHECK(result.log == "f:a:");
    CHECK(result.value == 11);
}

TEST_CASE("ApplicativeTest - Invoke") {
    logged_applicative_map m;
    auto a = logged<int>{"x:", 3};
    auto b = logged<int>{"y:", 4};
    auto result = m.invoke([](int x, int y) { return x + y; }, a, b);
    CHECK(result.log == "x:y:");
    CHECK(result.value == 7);
}

TEST_CASE("ApplicativeTest - LiftA2") {
    logged_applicative_map m;
    auto a = logged<int>{"a:", 10};
    auto b = logged<int>{"b:", 20};
    auto result = m.lift_a2([](int x, int y) { return x * y; }, a, b);
    CHECK(result.log == "a:b:");
    CHECK(result.value == 200);
}

TEST_CASE("ApplicativeTest - DiscardFirst") {
    logged_applicative_map m;
    auto a = logged<int>{"first:", 1};
    auto b = logged<int>{"second:", 2};
    auto result = m.discard_first(a, b);
    CHECK(result.log == "first:second:");
    CHECK(result.value == 2);
}

TEST_CASE("ApplicativeTest - DiscardSecond") {
    logged_applicative_map m;
    auto a = logged<int>{"first:", 1};
    auto b = logged<int>{"second:", 2};
    auto result = m.discard_second(a, b);
    CHECK(result.log == "first:second:");
    CHECK(result.value == 1);
}

TEST_CASE("ApplicativeTest - TypeclassLookup") {
    const auto &tc = applicative_typeclass<logged<int>>;
    static_assert(
        !std::is_same_v<std::remove_cvref_t<decltype(tc)>, std::false_type>);

    auto w = tc.pure(42);
    CHECK(w.log.empty());
    CHECK(w.value == 42);
}

TEST_CASE("ApplicativeTest - InvokeCpo") {
    auto a = logged<int>{"x:", 3};
    auto b = logged<int>{"y:", 4};
    auto result = invoke([](int x, int y) { return x + y; }, a, b);
    CHECK(result.log == "x:y:");
    CHECK(result.value == 7);
}

TEST_CASE("ApplicativeTest - InvokeThreeArgs") {
    logged_applicative_map m;
    auto a = logged<int>{"a:", 1};
    auto b = logged<int>{"b:", 2};
    auto c = logged<int>{"c:", 3};
    auto result =
        m.invoke([](int x, int y, int z) { return x + y + z; }, a, b, c);
    CHECK(result.log == "a:b:c:");
    CHECK(result.value == 6);
}
