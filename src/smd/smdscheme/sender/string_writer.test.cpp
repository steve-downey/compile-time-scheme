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
