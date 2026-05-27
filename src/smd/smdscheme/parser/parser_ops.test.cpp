// src/smd/smdscheme/parser/parser_ops.test.cpp                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/parser/parser_ops.hpp>
#include <smd/smdscheme/parser/parser_ops.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

// Existing tests via parser_v (backward compatibility)

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

// Typeclass lookup: parser_typeclass<parser<F>> resolves to parser_ops

static_assert([] {
    using namespace smd::smdscheme::parser;
    auto base_parser = char_p('x');
    using P = decltype(base_parser);
    const auto &tc = parser_typeclass<P>;
    auto p = tc.map(base_parser, [](char) { return 99; });
    auto r = p(cursor{"x"});
    return r.has_value() && r.value().value == 99;
}());

// Derived operations: apply from lift2

static_assert([] {
    using namespace smd::smdscheme::parser;
    auto pf = parser_v.pure([](char c) { return c == 'y' ? 1 : 0; });
    auto pa = char_p('y');
    auto p = parser_v.apply(pf, pa);
    auto r = p(cursor{"y"});
    return r.has_value() && r.value().value == 1;
}());

// Derived operations: sequence_left and sequence_right from lift2

static_assert([] {
    using namespace smd::smdscheme::parser;
    auto pa = char_p('a');
    auto pb = char_p('b');
    auto pl = parser_v.sequence_left(pa, pb);
    auto pr = parser_v.sequence_right(pa, pb);
    auto rl = pl(cursor{"ab"});
    auto rr = pr(cursor{"ab"});
    return rl.has_value() && rl.value().value == 'a' && rr.has_value() &&
           rr.value().value == 'b';
}());

// NTTP pinning: explicit typeclass override at call site

template <class P, const auto &TC = smd::smdscheme::parser::parser_typeclass<P>>
constexpr auto map_via_typeclass(P p, auto f) {
    return TC.map(p, f);
}

static_assert([] {
    using namespace smd::smdscheme::parser;
    auto p = map_via_typeclass(char_p('z'), [](char) { return 42; });
    auto r = p(cursor{"z"});
    return r.has_value() && r.value().value == 42;
}());

TEST_CASE("ParserOpsTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ParserOpsTest - TypeclassLookup") {
    using namespace smd::smdscheme::parser;
    auto base_parser = char_p('a');
    using P = decltype(base_parser);
    const auto &tc = parser_typeclass<P>;
    auto p = tc.map(base_parser, [](char c) { return c; });
    auto r = p(cursor{"a"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value == 'a');
}

TEST_CASE("ParserOpsTest - DerivedApply") {
    using namespace smd::smdscheme::parser;
    auto pf = parser_v.pure([](char c) -> int { return c - '0'; });
    auto pa = char_p('5');
    auto p = parser_v.apply(pf, pa);
    auto r = p(cursor{"5"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value == 5);
}

TEST_CASE("ParserOpsTest - DerivedSequenceLeft") {
    using namespace smd::smdscheme::parser;
    auto p = parser_v.sequence_left(char_p('a'), char_p('b'));
    auto r = p(cursor{"ab"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value == 'a');
}

TEST_CASE("ParserOpsTest - DerivedSequenceRight") {
    using namespace smd::smdscheme::parser;
    auto p = parser_v.sequence_right(char_p('a'), char_p('b'));
    auto r = p(cursor{"ab"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value == 'b');
}

TEST_CASE("ParserOpsTest - NttpPinning") {
    using namespace smd::smdscheme::parser;
    auto p = map_via_typeclass(char_p('q'), [](char) { return 77; });
    auto r = p(cursor{"q"});
    REQUIRE(r.has_value());
    REQUIRE(r.value().value == 77);
}
