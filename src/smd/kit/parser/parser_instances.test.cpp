// src/smd/kit/parser/parser_instances.test.cpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/kit/parser/parser_instances.hpp>
#include <smd/kit/parser/parser_instances.hpp> // test 2nd include OK

#include <smd/kit/foundation/parse_error.hpp>
#include <smd/kit/foundation/source_pos.hpp>
#include <smd/kit/parser/parser.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>

// bind is a CPO -- an object, not a function template -- so argument
// -dependent lookup does not find it from a parser<F> argument the way it
// would for a function template. Every call site brings it in this way or
// qualifies it; this is the same fact ae3c33a's commit message names for
// result's own and_then, more exposed here because parser has no
// pre-existing domain name to hide the CPO behind.
using smd::kit::foundation::bind;
using smd::kit::foundation::parse_error;
using smd::kit::foundation::source_pos;
using smd::kit::parser::cursor;
using smd::kit::parser::no_context;
using smd::kit::parser::parse_context;
using smd::kit::parser::parse_result;
using smd::kit::parser::parse_state;
using smd::kit::parser::parser;
using smd::kit::parser::pure;

TEST_CASE("ParserInstancesTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

// A non-trivial context, exercised the same way parser.test.cpp exercises
// it: the Monad laws must hold for a context a parser actually reads, not
// only for the empty no_context.
struct scale_context {
    int factor = 1;
};

constexpr auto char_val = parser{[](cursor cur,
                                    parse_context auto &) -> parse_result<int> {
    if (cur.empty()) {
        return parse_error{cur.position(), "expected char"};
    }
    return parse_state<int>{static_cast<unsigned char>(cur.peek()), cur.bump()};
}};

constexpr auto scaled_char =
    parser{[](cursor cur, scale_context &ctx) -> parse_result<int> {
        if (cur.empty()) {
            return parse_error{cur.position(), "expected char"};
        }
        return parse_state<int>{
            static_cast<unsigned char>(cur.peek()) * ctx.factor, cur.bump()};
    }};

// Runs a parser over a fresh cursor on the same text, threading ctx. The
// same helper shape parser.test.cpp uses for the Functor laws; B3 needs it
// again once repetition and choice arrive.
template <class P, class Ctx>
constexpr auto run(P const &p, std::string_view text, Ctx &ctx) {
    return p(cursor{text}, ctx);
}

// Two parsers are equal when they produce the same parse_result -- same
// value, same rest cursor, same error and error position -- for the same
// input and context.
template <class P, class Q, class Ctx>
constexpr auto same_parse(P const &p, Q const &q, std::string_view text,
                          Ctx &ctx) -> bool {
    return run(p, text, ctx) == run(q, text, ctx);
}

// A continuation for bind: doubles the value and re-wraps it as a parser
// that consumes no further input, over any threaded context.
constexpr auto double_p = [](int n) {
    return parser{[n](cursor cur, parse_context auto &) -> parse_result<int> {
        return parse_state<int>{n * 2, cur};
    }};
};

// A second continuation, so associativity has two genuinely different
// steps to chain.
constexpr auto succ_p = [](int n) {
    return parser{[n](cursor cur, parse_context auto &) -> parse_result<int> {
        return parse_state<int>{n + 1, cur};
    }};
};

} // namespace

// 915487ab-d22a-4335-ada8-fd79b9d159f4
// --- Monad laws, over no_context. -------------------------------------

static_assert([] {
    no_context ctx{};
    return same_parse(bind(pure(5), double_p), double_p(5), "a", ctx);
}());

static_assert([] {
    // Right identity must also hold on the failing branch.
    no_context ctx{};
    auto const pure_p = [](int n) { return pure(n); };
    return same_parse(bind(char_val, pure_p), char_val, "a", ctx) &&
           same_parse(bind(char_val, pure_p), char_val, "", ctx);
}());

static_assert([] {
    no_context ctx{};
    auto const lhs = bind(bind(char_val, double_p), succ_p);
    auto const rhs =
        bind(char_val, [](int n) { return bind(double_p(n), succ_p); });
    return same_parse(lhs, rhs, "a", ctx) && same_parse(lhs, rhs, "", ctx);
}());

// --- The same three laws, over a context the parsers actually read. -------

static_assert([] {
    scale_context ctx{3};
    return same_parse(bind(pure(5), double_p), double_p(5), "a", ctx);
}());

static_assert([] {
    scale_context ctx{3};
    auto const pure_p = [](int n) { return pure(n); };
    return same_parse(bind(scaled_char, pure_p), scaled_char, "a", ctx) &&
           same_parse(bind(scaled_char, pure_p), scaled_char, "", ctx);
}());

static_assert([] {
    scale_context ctx{3};
    auto const lhs = bind(bind(scaled_char, double_p), succ_p);
    auto const rhs =
        bind(scaled_char, [](int n) { return bind(double_p(n), succ_p); });
    return same_parse(lhs, rhs, "a", ctx) && same_parse(lhs, rhs, "", ctx);
}());

// --- bind does not invoke its continuation on a failed value. --------------

static_assert([] {
    no_context ctx{};
    bool invoked = false;
    auto const probe = [&invoked](int n) {
        invoked = true;
        return pure(n);
    };
    auto const r = bind(char_val, probe)(cursor{""}, ctx);
    return !invoked && !r.has_value();
}());
// 915487ab-d22a-4335-ada8-fd79b9d159f4 end

TEST_CASE("ParserInstancesTest - MonadLeftIdentity") {
    no_context ctx{};
    CHECK(same_parse(bind(pure(5), double_p), double_p(5), "a", ctx));
}

TEST_CASE("ParserInstancesTest - MonadRightIdentity") {
    no_context ctx{};
    auto const pure_p = [](int n) { return pure(n); };
    CHECK(same_parse(bind(char_val, pure_p), char_val, "a", ctx));
    CHECK(same_parse(bind(char_val, pure_p), char_val, "", ctx));
}

TEST_CASE("ParserInstancesTest - MonadAssociativity") {
    no_context ctx{};
    auto const lhs = bind(bind(char_val, double_p), succ_p);
    auto const rhs =
        bind(char_val, [](int n) { return bind(double_p(n), succ_p); });
    CHECK(same_parse(lhs, rhs, "a", ctx));
    CHECK(same_parse(lhs, rhs, "", ctx));
}

TEST_CASE("ParserInstancesTest - MonadLawsHoldForANonTrivialContext") {
    scale_context ctx{3};
    auto const pure_p = [](int n) { return pure(n); };
    CHECK(same_parse(bind(pure(5), double_p), double_p(5), "a", ctx));
    CHECK(same_parse(bind(scaled_char, pure_p), scaled_char, "a", ctx));
}

TEST_CASE("ParserInstancesTest - BindSkipsContinuationAfterFailure") {
    no_context ctx{};
    bool invoked = false;
    auto const probe = [&invoked](int n) {
        invoked = true;
        return pure(n);
    };
    auto const r = bind(char_val, probe)(cursor{""}, ctx);
    CHECK_FALSE(invoked);
    CHECK_FALSE(r.has_value());
}
