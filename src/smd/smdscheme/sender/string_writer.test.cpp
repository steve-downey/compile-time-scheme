// src/smd/smdscheme/sender/string_writer.test.cpp              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/string_writer.hpp>
#include <smd/smdscheme/sender/string_writer.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <variant>

using smd::smdscheme::sender::combine;
using smd::smdscheme::sender::fmap;
using smd::smdscheme::sender::lift_a2;
using smd::smdscheme::sender::string_writer;
using smd::smdscheme::sender::tell;

using smd::smdscheme::foundation::alternative_typeclass;
using smd::smdscheme::foundation::applicative_typeclass;
using smd::smdscheme::foundation::functor_typeclass;

TEST_CASE("StringWriterTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("StringWriterTest - Pure") {
    auto w = string_writer<int>::pure(42);
    CHECK(w.log.empty());
    CHECK(w.value == 42);
}

TEST_CASE("StringWriterTest - Tell") {
    auto w = tell("hello");
    CHECK(w.log == "hello");
}

TEST_CASE("StringWriterTest - Combine") {
    auto a = tell("foo");
    auto b = tell("bar");
    auto c = combine(a, b);
    CHECK(c.log == "foobar");
}

TEST_CASE("StringWriterTest - LiftA2") {
    auto a = string_writer<int>{"log_a:", 3};
    auto b = string_writer<int>{"log_b:", 4};
    auto c = lift_a2([](int x, int y) { return x + y; }, a, b);
    CHECK(c.log == "log_a:log_b:");
    CHECK(c.value == 7);
}

TEST_CASE("StringWriterTest - Fmap") {
    auto w = string_writer<int>{"some_log", 5};
    auto w2 = fmap([](int x) { return x * 2; }, w);
    CHECK(w2.log == "some_log");
    CHECK(w2.value == 10);
}

TEST_CASE("StringWriterTest - CombineMultiple") {
    auto result = combine(combine(tell("a\n"), tell("b\n")), tell("c\n"));
    CHECK(result.log == "a\nb\nc\n");
}

// -- Typeclass object tests --

TEST_CASE("StringWriterTypeclass - FunctorLookup") {
    const auto &tc = functor_typeclass<string_writer<int>>;
    static_assert(
        !std::is_same_v<std::remove_cvref_t<decltype(tc)>, std::false_type>);

    auto w = string_writer<int>{"log:", 5};
    auto w2 = tc.fmap([](int x) { return x * 3; }, w);
    CHECK(w2.log == "log:");
    CHECK(w2.value == 15);
}

TEST_CASE("StringWriterTypeclass - FunctorReplace") {
    const auto &tc = functor_typeclass<string_writer<int>>;
    auto w = string_writer<int>{"log:", 5};
    auto w2 = tc.replace(w, 99);
    CHECK(w2.log == "log:");
    CHECK(w2.value == 99);
}

TEST_CASE("StringWriterTypeclass - ApplicativePure") {
    const auto &tc = applicative_typeclass<string_writer<int>>;
    static_assert(
        !std::is_same_v<std::remove_cvref_t<decltype(tc)>, std::false_type>);

    auto w = tc.pure(42);
    CHECK(w.log.empty());
    CHECK(w.value == 42);
}

TEST_CASE("StringWriterTypeclass - ApplicativeApply") {
    const auto &tc = applicative_typeclass<string_writer<int>>;
    auto func_w =
        string_writer<int (*)(int)>{"f:", [](int x) { return x + 1; }};
    auto arg_w = string_writer<int>{"a:", 10};
    auto result = tc.apply(func_w, arg_w);
    CHECK(result.log == "f:a:");
    CHECK(result.value == 11);
}

TEST_CASE("StringWriterTypeclass - ApplicativeInvoke") {
    const auto &tc = applicative_typeclass<string_writer<int>>;
    auto a = string_writer<int>{"x:", 3};
    auto b = string_writer<int>{"y:", 4};
    auto result = tc.invoke([](int x, int y) { return x + y; }, a, b);
    CHECK(result.log == "x:y:");
    CHECK(result.value == 7);
}

TEST_CASE("StringWriterTypeclass - ApplicativeLiftA2") {
    const auto &tc = applicative_typeclass<string_writer<int>>;
    auto a = string_writer<int>{"a:", 10};
    auto b = string_writer<int>{"b:", 20};
    auto result = tc.lift_a2([](int x, int y) { return x * y; }, a, b);
    CHECK(result.log == "a:b:");
    CHECK(result.value == 200);
}

TEST_CASE("StringWriterTypeclass - ApplicativeDiscardFirst") {
    const auto &tc = applicative_typeclass<string_writer<int>>;
    auto a = string_writer<int>{"first:", 1};
    auto b = string_writer<int>{"second:", 2};
    auto result = tc.discard_first(a, b);
    CHECK(result.log == "first:second:");
    CHECK(result.value == 2);
}

TEST_CASE("StringWriterTypeclass - ApplicativeDiscardSecond") {
    const auto &tc = applicative_typeclass<string_writer<int>>;
    auto a = string_writer<int>{"first:", 1};
    auto b = string_writer<int>{"second:", 2};
    auto result = tc.discard_second(a, b);
    CHECK(result.log == "first:second:");
    CHECK(result.value == 1);
}

TEST_CASE("StringWriterTypeclass - AlternativeEmpty") {
    const auto &tc = alternative_typeclass<string_writer<int>>;
    static_assert(
        !std::is_same_v<std::remove_cvref_t<decltype(tc)>, std::false_type>);

    auto w = tc.empty();
    CHECK(w.log.empty());
    CHECK(w.value == 0);
}

TEST_CASE("StringWriterTypeclass - AlternativeAlt") {
    const auto &tc = alternative_typeclass<string_writer<int>>;
    auto a = string_writer<int>{"left:", 1};
    auto b = string_writer<int>{"right:", 2};
    auto result = tc.alt(a, b);
    CHECK(result.log == "left:right:");
    CHECK(result.value == 2);
}

TEST_CASE("StringWriterTypeclass - AlternativeCombine") {
    const auto &tc = alternative_typeclass<string_writer<int>>;
    auto a = string_writer<int>{"a:", 1};
    auto b = string_writer<int>{"b:", 2};
    auto result = tc.combine(a, b);
    CHECK(result.log == "a:b:");
    CHECK(result.value == 2);
}

TEST_CASE("StringWriterTypeclass - FunctorCPO") {
    auto w = string_writer<int>{"log:", 5};
    auto w2 = smd::smdscheme::foundation::fmap([](int x) { return x * 2; }, w);
    CHECK(w2.log == "log:");
    CHECK(w2.value == 10);
}

TEST_CASE("StringWriterTypeclass - ApplicativeInvokeCPO") {
    auto a = string_writer<int>{"x:", 3};
    auto b = string_writer<int>{"y:", 4};
    auto result = smd::smdscheme::foundation::invoke(
        [](int x, int y) { return x + y; }, a, b);
    CHECK(result.log == "x:y:");
    CHECK(result.value == 7);
}

TEST_CASE("StringWriterTypeclass - AlternativeAltCPO") {
    auto a = string_writer<int>{"a:", 1};
    auto b = string_writer<int>{"b:", 2};
    auto result = smd::smdscheme::foundation::alt(a, b);
    CHECK(result.log == "a:b:");
    CHECK(result.value == 2);
}

// -- NTTP pinning tests --

template <class T,
          const auto &Map = smd::smdscheme::foundation::functor_typeclass<T>>
auto apply_fmap_pinned(auto &&f, T const &val) {
    using map_type = std::remove_cvref_t<decltype(Map)>;
    return map_type{}.fmap(std::forward<decltype(f)>(f), val);
}

TEST_CASE("StringWriterTypeclass - NTTPFunctor") {
    auto w = string_writer<int>{"log:", 7};
    auto w2 = apply_fmap_pinned([](int x) { return x + 1; }, w);
    CHECK(w2.value == 8);
}
