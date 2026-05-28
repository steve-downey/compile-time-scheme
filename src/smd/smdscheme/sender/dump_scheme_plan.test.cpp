// src/smd/smdscheme/sender/dump_scheme_plan.test.cpp            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/dump_scheme_plan.hpp>
#include <smd/smdscheme/sender/dump_scheme_plan.hpp> // test 2nd include OK

#include <smd/smdscheme/sender/sender_v.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

using namespace smd::smdscheme;

TEST_CASE("DumpSchemePlanTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("DumpSchemePlanTest - JustSender") {
    using JustType = decltype(sender_v::just(42));
    std::ostringstream out;
    sender::dump_scheme_plan<JustType>(out);
    std::string dot = out.str();

    CHECK(dot.find("digraph SchemeExecutionPlan") != std::string::npos);
    CHECK(dot.find("just") != std::string::npos);
    CHECK(dot.find("}") != std::string::npos);
}

TEST_CASE("DumpSchemePlanTest - ThenSender") {
    using ThenType = decltype(sender_v::then(
        sender_v::just(1), [](int x) { return x + 1; }));
    std::ostringstream out;
    sender::dump_scheme_plan<ThenType>(out);
    std::string dot = out.str();

    CHECK(dot.find("then") != std::string::npos);
    CHECK(dot.find("just") != std::string::npos);
    CHECK(dot.find("->") != std::string::npos);
}

TEST_CASE("DumpSchemePlanTest - WhenAllThenSender") {
    using ComposedType = decltype(sender_v::then(
        sender_v::when_all(sender_v::just(1), sender_v::just(2)),
        [](int a, int b) { return a + b; }));
    std::ostringstream out;
    sender::dump_scheme_plan<ComposedType>(out);
    std::string dot = out.str();

    CHECK(dot.find("then") != std::string::npos);
    CHECK(dot.find("when_all") != std::string::npos);
    auto edge_count = 0;
    std::string::size_type pos = 0;
    while ((pos = dot.find("->", pos)) != std::string::npos) {
        ++edge_count;
        pos += 2;
    }
    CHECK(edge_count >= 3);
}
