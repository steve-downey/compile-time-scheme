// src/smd/smdlisp/closure/value.test.cpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/closure/value.hpp>
#include <smd/smdlisp/closure/value.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {

/// Stand-in core AST type; step L7 only needs some type to instantiate
/// @c value<Core> with, the real core arena arrives in step L10.
struct dummy_core {};

using val = smd::smdlisp::closure::value<dummy_core>;

// Merge criteria (docs/cl-pivot-plan.md, Step L7): truthiness of nil, the
// integer 0, a t-symbol, and a keyword.

static_assert(
    !smd::smdlisp::closure::is_true(val{smd::smdlisp::closure::nil_t{}}));
static_assert(smd::smdlisp::closure::is_true(val{0}));
static_assert(smd::smdlisp::closure::is_true(
    val{smd::smdlisp::closure::symbol{"T"}}));
static_assert(smd::smdlisp::closure::is_true(
    val{smd::smdlisp::closure::keyword{"FOO"}}));

// nil is false regardless of how it is spelled; every other integer,
// including negatives, stays true too.
static_assert(smd::smdlisp::closure::is_true(val{42}));
static_assert(smd::smdlisp::closure::is_true(val{-1}));
static_assert(smd::smdlisp::closure::is_true(
    val{smd::smdlisp::closure::builtin{smd::smdlisp::closure::builtin_op::add}}));

static_assert(smd::smdlisp::closure::nil_t{} == smd::smdlisp::closure::nil_t{});
static_assert(smd::smdlisp::closure::symbol{"FOO"} ==
              smd::smdlisp::closure::symbol{"FOO"});
static_assert(!(smd::smdlisp::closure::symbol{"FOO"} ==
                smd::smdlisp::closure::symbol{"BAR"}));
static_assert(smd::smdlisp::closure::keyword{"FOO"} ==
              smd::smdlisp::closure::keyword{"FOO"});
static_assert(!(smd::smdlisp::closure::keyword{"FOO"} ==
                smd::smdlisp::closure::keyword{"BAR"}));

// A symbol and a keyword spelled the same way are still distinct value
// kinds: the variant holds either alternative, never both at once.
static_assert(std::holds_alternative<smd::smdlisp::closure::keyword>(
    val{smd::smdlisp::closure::keyword{"FOO"}}));
static_assert(!std::holds_alternative<smd::smdlisp::closure::symbol>(
    val{smd::smdlisp::closure::keyword{"FOO"}}));

} // namespace

TEST_CASE("ValueTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ValueTest - NilIsFalse") {
    using namespace smd::smdlisp;
    val v{closure::nil_t{}};
    REQUIRE_FALSE(closure::is_true(v));
}

TEST_CASE("ValueTest - IntegerZeroIsTrue") {
    using namespace smd::smdlisp;
    val v{0};
    REQUIRE(closure::is_true(v));
}

TEST_CASE("ValueTest - SymbolTIsTrue") {
    using namespace smd::smdlisp;
    val v{closure::symbol{"T"}};
    REQUIRE(closure::is_true(v));
}

TEST_CASE("ValueTest - KeywordIsTrue") {
    using namespace smd::smdlisp;
    val v{closure::keyword{"FOO"}};
    REQUIRE(closure::is_true(v));
}

TEST_CASE("ValueTest - BuiltinIsTrue") {
    using namespace smd::smdlisp;
    val v{closure::builtin{closure::builtin_op::multiply}};
    REQUIRE(closure::is_true(v));
}

TEST_CASE("ValueTest - ClosureIsTrue") {
    using namespace smd::smdlisp;
    dummy_core node{};
    val v{closure::closure<dummy_core>{&node, nullptr}};
    REQUIRE(closure::is_true(v));
}

TEST_CASE("ValueTest - ForeignFunctionIsTrue") {
    using namespace smd::smdlisp;
    using ff = closure::foreign_function<dummy_core>;
    using result_t = smd::smdscheme::foundation::result<ff::val_t>;
    ff::sig_t fn = [](std::span<ff::val_t const>) -> result_t {
        return result_t{ff::val_t{0}};
    };
    val v{ff{fn}};
    REQUIRE(closure::is_true(v));
}

TEST_CASE("ValueTest - SymbolEquality") {
    using namespace smd::smdlisp;
    REQUIRE(closure::symbol{"FOO"} == closure::symbol{"FOO"});
    REQUIRE_FALSE(closure::symbol{"FOO"} == closure::symbol{"BAR"});
}

TEST_CASE("ValueTest - KeywordEquality") {
    using namespace smd::smdlisp;
    REQUIRE(closure::keyword{"FOO"} == closure::keyword{"FOO"});
    REQUIRE_FALSE(closure::keyword{"FOO"} == closure::keyword{"BAR"});
}
