// src/smd/cl/sender/run_sender.test.cpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/sender/run_sender.hpp>
#include <smd/cl/sender/run_sender.hpp> // test 2nd include OK

#include <smd/cl/eval/outcome.hpp>
#include <smd/cl/eval/value.hpp>
#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/sender/sender_v.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>

using smd::cl::eval::outcome;
using smd::cl::eval::value;
using smd::cl::eval::value_fixnum;
using smd::cl::foundation::parse_error;
using smd::cl::sender::run_sender;

namespace sender_v = smd::cl::sender::sender_v;

TEST_CASE("RunSenderTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("RunSenderTest - JustBecomesAValueOutcome") {
    outcome const observed = run_sender(sender_v::just(value{value_fixnum{7}}));
    REQUIRE(observed.is_value());
    CHECK(observed.as_value() == value{value_fixnum{7}});
}

TEST_CASE("RunSenderTest - JustErrorBecomesAnErrorOutcome") {
    outcome const observed =
        run_sender(sender_v::just_error(parse_error{{}, "diagnosed"}));
    REQUIRE(observed.is_error());
    CHECK(std::string_view{observed.as_error().message} == "diagnosed");
}

TEST_CASE("RunSenderTest - JustStoppedBecomesADiagnosedError") {
    // Execution26's stopped channel is niladic (decision D13's mapping,
    // `machine_sender.hpp`): the only fact `run_sender` can recover is that
    // *something* stopped, not what. It is reported as a diagnosed error
    // rather than silently discarded.
    outcome const observed = run_sender(sender_v::just_stopped());
    REQUIRE(observed.is_error());
}
