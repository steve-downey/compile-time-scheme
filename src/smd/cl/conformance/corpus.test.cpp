// src/smd/cl/conformance/corpus.test.cpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/conformance/corpus.hpp>
#include <smd/cl/conformance/corpus.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <ranges>

using smd::cl::conformance::ansi_test_adapted;
using smd::cl::conformance::satisfies;
using smd::cl::conformance::spec_derived;

namespace {

constexpr auto every_ansi_test_adapted_case_holds() -> bool {
    return std::ranges::all_of(ansi_test_adapted, satisfies);
}

constexpr auto every_spec_derived_case_holds() -> bool {
    return std::ranges::all_of(spec_derived, satisfies);
}

} // namespace

// The compile-time twin: the whole corpus, run through `cl/eval` at compile
// time, exactly as the pipeline it exercises is constant-evaluable end to
// end (decision D14 keeps every runtime object capacity-free; nothing here
// depends on I/O or the clock).
static_assert(every_ansi_test_adapted_case_holds());
static_assert(every_spec_derived_case_holds());

TEST_CASE("CorpusTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("CorpusTest - AnsiTestAdapted") {
    for (auto const &c : ansi_test_adapted) {
        INFO(c.name);
        INFO(c.origin);
        CHECK(satisfies(c));
    }
}

TEST_CASE("CorpusTest - SpecDerived") {
    for (auto const &c : spec_derived) {
        INFO(c.name);
        INFO(c.origin);
        CHECK(satisfies(c));
    }
}
