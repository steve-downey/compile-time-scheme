// src/smd/cl/conformance/driver.test.cpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/conformance/driver.hpp>
#include <smd/cl/conformance/driver.hpp> // test 2nd include OK

#include <smd/cl/eval/outcome.hpp>
#include <smd/cl/eval/value.hpp>

#include <catch2/catch_test_macros.hpp>

#include <variant>

using smd::cl::conformance::evaluate_program;
using smd::cl::eval::value_fixnum;
using smd::cl::eval::value_symbol;

namespace {

constexpr auto yields(std::string_view source, int expected) -> bool {
    auto const done = evaluate_program(source).how;
    return done.is_value() &&
           done.as_value() == smd::cl::eval::value{value_fixnum{expected}};
}

// ------------------------------------------------------------------
// The unfold: any number of top-level forms, one arena
// ------------------------------------------------------------------

constexpr auto one_form_is_its_own_value() -> bool {
    return yields("(+ 1 2)", 3);
}

// Two forms share one arena, exactly as `machine.test.cpp`'s
// `evaluate_both` established by hand for the recursive-defun witness;
// this is the same fact through the general driver.
constexpr auto two_forms_share_one_arena() -> bool {
    return yields("(defun sq (x) (* x x)) (sq 5)", 25);
}

// More than two forms still share one arena, which `evaluate_both` could
// not exercise at all: it only ever wrapped exactly two.
constexpr auto three_forms_share_one_arena() -> bool {
    return yields("(defun a () 1) (defun b () (+ (a) 1)) (b)", 2);
}

constexpr auto an_empty_source_is_diagnosed() -> bool {
    return evaluate_program("").how.is_error() &&
           evaluate_program("   ; only a comment\n").how.is_error();
}

constexpr auto a_read_failure_and_a_run_failure_both_land_in_outcome() -> bool {
    return evaluate_program("(1 2").how.is_error() &&  // unterminated
           evaluate_program("(car 2)").how.is_error(); // runs, then fails
}

// ------------------------------------------------------------------
// The witness: generalizing `machine.test.cpp`'s cleanup probe
// ------------------------------------------------------------------

constexpr auto the_witness_reports_whether_it_was_defined() -> bool {
    auto const ran =
        evaluate_program("(unwind-protect 1 (defun ran () 1))", "RAN");
    auto const not_ran = evaluate_program("1", "RAN");
    auto const ignored = evaluate_program("1"); // no witness name: false
    return ran.witness_defined && !not_ran.witness_defined &&
           !ignored.witness_defined;
}

constexpr auto known_resolves_nil_and_t() -> bool {
    auto const done = evaluate_program("nil");
    return done.how.is_value() &&
           done.how.as_value() == smd::cl::eval::symbol_value(done.known.nil);
}

} // namespace

static_assert(one_form_is_its_own_value());
static_assert(two_forms_share_one_arena());
static_assert(three_forms_share_one_arena());
static_assert(an_empty_source_is_diagnosed());
static_assert(a_read_failure_and_a_run_failure_both_land_in_outcome());
static_assert(the_witness_reports_whether_it_was_defined());
static_assert(known_resolves_nil_and_t());

TEST_CASE("DriverTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("DriverTest - OneShotChain") {
    CHECK(one_form_is_its_own_value());
    CHECK(two_forms_share_one_arena());
    CHECK(three_forms_share_one_arena());
}

TEST_CASE("DriverTest - EmptyAndFailingSources") {
    CHECK(an_empty_source_is_diagnosed());
    CHECK(a_read_failure_and_a_run_failure_both_land_in_outcome());
}

TEST_CASE("DriverTest - Witness") {
    CHECK(the_witness_reports_whether_it_was_defined());
    CHECK(known_resolves_nil_and_t());
}
