// src/smd/smdscheme/parser/parser_ops.test.cpp                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/parser/parser_ops.hpp>
#include <smd/smdscheme/parser/parser_ops.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

static_assert([] {
    using namespace smd::smdscheme::parser;
    auto p = parser_v.map(char_p('x'), [](char) { return 42; });
    auto r = p(cursor{"x"});
    return r.has_value() && r.value().value == 42;
}());

static_assert([] {
    using namespace smd::smdscheme::parser;
    auto p = parser_v.alt(char_p('a'), char_p('b'));
    auto ra = p(cursor{"a"});
    auto rb = p(cursor{"b"});
    return ra.has_value() && ra.value().value == 'a' && rb.has_value() &&
           rb.value().value == 'b';
}());

static_assert([] {
    using namespace smd::smdscheme::parser;
    auto p = parser_v.pure(7);
    auto r = p(cursor{"anything"});
    return r.has_value() && r.value().value == 7;
}());

TEST_CASE("ParserOpsTest - HeaderIsIdempotent") { REQUIRE(true); }
