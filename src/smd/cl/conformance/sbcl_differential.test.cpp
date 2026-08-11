// src/smd/cl/conformance/sbcl_differential.test.cpp               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// The guarded half of decision D16's differential oracle: cross-checks the
// corpus's fixnum- and boolean-valued cases against a real SBCL, whose
// version is probed and reported at the top of each run — the pinned
// oracle build a reported divergence would be attributed to, exactly as
// `compile-time-forth`'s `gforth_diff` pins the gforth version it runs
// against.
//
// This is a genuine runtime test: shelling out to a subprocess has no
// constexpr twin, and none is claimed. It is a member of the default suite
// (`make test`) per the orchestrator's decision, so it must SKIP rather
// than fail when `sbcl` is not on `PATH` — an environment without SBCL
// installed stays green, because a missing oracle is a fact about the
// environment, not a defect in this project.

#include <smd/cl/conformance/sbcl_oracle.hpp>
#include <smd/cl/conformance/sbcl_oracle.hpp> // test 2nd include OK

#include <smd/cl/conformance/corpus.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <span>
#include <string>
#include <variant>

using smd::cl::conformance::ansi_test_adapted;
using smd::cl::conformance::corpus_case;
using smd::cl::conformance::expect_boolean;
using smd::cl::conformance::expect_fixnum;
using smd::cl::conformance::find_sbcl_version;
using smd::cl::conformance::sbcl_prin1;
using smd::cl::conformance::spec_derived;

namespace {

/// Returns what SBCL's `prin1` must print for @p c's expectation, or
/// `nullopt` if @p c's channel is not one this informal check compares: a
/// diagnosed error or an unwind has no single SBCL condition text this
/// check normalizes against, and `expect_any_symbol` names no specific
/// text to compare.
[[nodiscard]] auto expected_sbcl_text(corpus_case const &c)
    -> std::optional<std::string> {
    if (auto const *fixnum = std::get_if<expect_fixnum>(&c.expect)) {
        return std::to_string(fixnum->value);
    }
    if (auto const *boolean = std::get_if<expect_boolean>(&c.expect)) {
        return boolean->truthy ? std::string{"T"} : std::string{"NIL"};
    }
    return std::nullopt;
}

/// Wraps @p source — one or more top-level forms — in one `progn`, so a
/// two-form corpus entry (a `defun` followed by a call) reaches SBCL as a
/// single `--eval` argument the same way this project's own driver reaches
/// it as a single core arena.
[[nodiscard]] auto as_one_form(std::string_view source) -> std::string {
    return "(progn " + std::string(source) + ")";
}

/// Checks every scalar-valued case of @p table against SBCL, skipping
/// (never failing on) any case whose channel this check does not compare.
void check_against_sbcl(std::span<corpus_case const> table) {
    for (auto const &c : table) {
        auto const expected = expected_sbcl_text(c);
        if (!expected.has_value()) {
            continue;
        }
        INFO(c.name);
        INFO(c.origin);
        auto const actual = sbcl_prin1(as_one_form(c.source));
        REQUIRE(actual.has_value());
        CHECK(*actual == *expected);
    }
}

} // namespace

TEST_CASE("SbclDifferentialTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("SbclDifferentialTest - AnsiTestAdaptedCasesAgreeWithSbcl") {
    auto const version = find_sbcl_version();
    if (!version.has_value()) {
        SKIP("sbcl not found on PATH; differential check skipped");
    }
    INFO("oracle: " << *version);
    check_against_sbcl(ansi_test_adapted);
}

TEST_CASE("SbclDifferentialTest - SpecDerivedCasesAgreeWithSbcl") {
    auto const version = find_sbcl_version();
    if (!version.has_value()) {
        SKIP("sbcl not found on PATH; differential check skipped");
    }
    INFO("oracle: " << *version);
    check_against_sbcl(spec_derived);
}
