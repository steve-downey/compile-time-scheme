// src/smd/smdlisp/sender/defer_outcome.test.cpp                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/sender/defer_outcome.hpp>
#include <smd/smdlisp/sender/defer_outcome.hpp> // test 2nd include OK

#include <smd/smdlisp/elaborator/elaborated_core.hpp>

#include <catch2/catch_test_macros.hpp>

#include <variant>

namespace {
namespace lisp = smd::smdlisp;
namespace snd = smd::smdlisp::sender;

constexpr int MaxNodes = 128;
constexpr int MaxList = 16;

using Core = lisp::elaborator::core_type<MaxNodes, MaxList>;
using Val = lisp::closure::value<Core>;
} // namespace

TEST_CASE("DeferOutcomeTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- the custom-sender plumbing itself ---------------------------------------
// Before anything is built on `defer_outcome_sender`, prove it actually
// satisfies Execution26's concepts: this vendored revision reports completion
// signatures through a *static member template*, not the older
// `completion_signatures` typedef, and getting that wrong fails late and
// verbosely.

static_assert(beman::execution::sender<
              snd::defer_outcome_sender<Core, snd::outcome<Core> (*)()>>);
static_assert(beman::execution::receiver<snd::outcome_receiver<Core>>);

TEST_CASE("DeferOutcomeTest - ValueChannel") {
    auto o = snd::run_sender<Core>(snd::defer_outcome<Core>(
        [] { return snd::make_value<Core>(Val{42}); }));
    REQUIRE(o.is_value());
    REQUIRE(std::get<int>(o.val) == 42);
}

TEST_CASE("DeferOutcomeTest - ErrorChannel") {
    auto o = snd::run_sender<Core>(snd::defer_outcome<Core>([] {
        return snd::make_error<Core>(
            smd::smdscheme::foundation::parse_error{{}, "boom"});
    }));
    REQUIRE(o.is_error());
    REQUIRE(std::string_view{o.err.message} == "boom");
}

TEST_CASE("DeferOutcomeTest - StoppedChannel") {
    // The channel the Scheme backend never had, and the reason this backend
    // needs no unwind sentinel at all.
    auto o = snd::run_sender<Core>(
        snd::defer_outcome<Core>([] { return snd::make_stopped<Core>(); }));
    REQUIRE(o.is_stopped());
}

TEST_CASE("DeferOutcomeTest - IsLazyUntilStarted") {
    // Laziness is load-bearing: `unwind_protect_sender` relies on the
    // protected form running INSIDE its operation, not before connect.
    int runs = 0;
    auto s = snd::defer_outcome<Core>([&runs] {
        ++runs;
        return snd::make_value<Core>(Val{1});
    });
    REQUIRE(runs == 0);
    auto o = snd::run_sender<Core>(std::move(s));
    REQUIRE(runs == 1);
    REQUIRE(o.is_value());
}

TEST_CASE("DeferOutcomeTest - ResultRoundTrip") {
    using Res = smd::smdscheme::foundation::result<Val>;
    Res const ok{Val{7}};
    Res const bad{smd::smdscheme::foundation::parse_error{{}, "e"}};
    REQUIRE(snd::from_result<Core>(ok).is_value());
    REQUIRE(snd::from_result<Core>(bad).is_error());

    auto const val = snd::make_value<Core>(Val{7});
    auto const stopped = snd::make_stopped<Core>();
    REQUIRE(snd::to_result<Core>(val).has_value());
    REQUIRE_FALSE(snd::to_result<Core>(stopped).has_value());
}
