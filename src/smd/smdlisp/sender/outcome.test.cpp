// src/smd/smdlisp/sender/outcome.test.cpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/sender/outcome.hpp>
#include <smd/smdlisp/sender/outcome.hpp> // test 2nd include OK

#include <smd/smdlisp/elaborator/elaborated_core.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
namespace lisp = smd::smdlisp;
namespace scm = smd::smdscheme;
namespace snd = smd::smdlisp::sender;

constexpr int MaxNodes = 128;
constexpr int MaxList = 16;

using Core = lisp::elaborator::core_type<MaxNodes, MaxList>;
using Val = lisp::closure::value<Core>;
using Res = scm::foundation::result<Val>;
} // namespace

TEST_CASE("OutcomeTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- the carrier's own algebra ----------------------------------------------
// The three constructors and the three predicates, checked against each other
// at this component's level. `sender_program.test.cpp` owns which channel a
// PROGRAM lands on; this file owns only that the type says what it holds.

TEST_CASE("OutcomeTest - ConstructorsSelectTheirChannel") {
    auto const v = snd::make_value<Core>(Val{7});
    REQUIRE(v.kind == snd::channel::value);
    REQUIRE(v.is_value());
    REQUIRE_FALSE(v.is_error());
    REQUIRE_FALSE(v.is_stopped());
    REQUIRE(std::get<int>(v.val) == 7);

    auto const e =
        snd::make_error<Core>(scm::foundation::parse_error{{}, "boom"});
    REQUIRE(e.kind == snd::channel::error);
    REQUIRE(e.is_error());
    REQUIRE_FALSE(e.is_value());
    REQUIRE_FALSE(e.is_stopped());
    REQUIRE(std::string_view{e.err.message} == "boom");

    auto const s = snd::make_stopped<Core>();
    REQUIRE(s.kind == snd::channel::stopped);
    REQUIRE(s.is_stopped());
    REQUIRE_FALSE(s.is_value());
    REQUIRE_FALSE(s.is_error());
}

TEST_CASE("OutcomeTest - DefaultIsTheValueChannel") {
    // The default matters: `outcome_receiver` default-constructs its slot
    // before connecting, and a default that read as "stopped" would make an
    // un-started operation look like an unwind in flight.
    snd::outcome<Core> const o{};
    REQUIRE(o.is_value());
}

// -- widening from, and narrowing back to, the closure backends' carrier -----

TEST_CASE("OutcomeTest - FromResultWidensBothChannels") {
    Res const ok{Val{7}};
    Res const bad{scm::foundation::parse_error{{}, "e"}};

    auto const wide_ok = snd::from_result<Core>(ok);
    REQUIRE(wide_ok.is_value());
    REQUIRE(std::get<int>(wide_ok.val) == 7);

    auto const wide_bad = snd::from_result<Core>(bad);
    REQUIRE(wide_bad.is_error());
    REQUIRE(std::string_view{wide_bad.err.message} == "e");
}

TEST_CASE("OutcomeTest - ToResultNarrowsAndDiagnosesAnEscapedStop") {
    auto const v = snd::to_result<Core>(snd::make_value<Core>(Val{7}));
    REQUIRE(v.has_value());
    REQUIRE(std::get<int>(v.value()) == 7);

    auto const e = snd::to_result<Core>(
        snd::make_error<Core>(scm::foundation::parse_error{{}, "e"}));
    REQUIRE_FALSE(e.has_value());
    REQUIRE(std::string_view{e.error().message} == "e");

    // The defensive branch. No well-formed program reaches it -- a stop is
    // only created once a LIVE target has been found, so the scope that will
    // consume it is always already on the stack -- but it must not silently
    // become a value if it ever does.
    auto const s = snd::to_result<Core>(snd::make_stopped<Core>());
    REQUIRE_FALSE(s.has_value());
    REQUIRE(std::string_view{s.error().message} ==
            "nonlocal exit escaped the top level");
}
