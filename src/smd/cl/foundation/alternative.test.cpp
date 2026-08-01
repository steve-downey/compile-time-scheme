// src/smd/cl/foundation/alternative.test.cpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Adapted by copy from compile-time-forth (smd::forth::foundation):
// src/smd/forth/foundation/alternative.test.cpp. No test existed in
// compile-time-scheme's copy of the foundation.

#include <smd/cl/foundation/alternative.hpp>
#include <smd/cl/foundation/alternative.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <type_traits>
#include <utility>

using smd::cl::foundation::alt;
using smd::cl::foundation::alternative;
using smd::cl::foundation::alternative_typeclass;

TEST_CASE("AlternativeTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

/// A minimal writer-like value, local to this test: a value paired with a
/// string log that concatenates under `alt`. Exercises the alternative CRTP
/// base's derived `combine` without pulling in a production type.
template <class T>
struct logged {
    std::string log;
    T value;
};

/// Alternative @c Impl for @c logged<T>. Templated on @c T (rather than
/// providing per-call member templates) because @c empty() must name
/// @c logged<T> in its return type with no argument to deduce it from.
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

namespace smd::cl::foundation {
template <class T>
inline constexpr auto alternative_typeclass<logged<T>> =
    logged_alternative_map<T>{};
}

TEST_CASE("AlternativeTest - CrtpEmpty") {
    logged_alternative_map<int> m;
    auto w = m.empty();
    CHECK(w.log.empty());
    CHECK(w.value == 0);
}

TEST_CASE("AlternativeTest - CrtpAlt") {
    logged_alternative_map<int> m;
    auto a = logged<int>{"left:", 1};
    auto b = logged<int>{"right:", 2};
    auto result = m.alt(a, b);
    CHECK(result.log == "left:right:");
    CHECK(result.value == 2);
}

TEST_CASE("AlternativeTest - CrtpCombine") {
    logged_alternative_map<int> m;
    auto a = logged<int>{"a:", 1};
    auto b = logged<int>{"b:", 2};
    auto result = m.combine(a, b);
    CHECK(result.log == "a:b:");
    CHECK(result.value == 2);
}

TEST_CASE("AlternativeTest - TypeclassLookup") {
    const auto &tc = alternative_typeclass<logged<int>>;
    static_assert(
        !std::is_same_v<std::remove_cvref_t<decltype(tc)>, std::false_type>);

    auto w = tc.empty();
    CHECK(w.log.empty());
    CHECK(w.value == 0);
}

TEST_CASE("AlternativeTest - AltCpo") {
    auto a = logged<int>{"a:", 1};
    auto b = logged<int>{"b:", 2};
    auto result = alt(a, b);
    CHECK(result.log == "a:b:");
    CHECK(result.value == 2);
}
