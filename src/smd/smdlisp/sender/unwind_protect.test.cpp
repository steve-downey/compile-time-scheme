// src/smd/smdlisp/sender/unwind_protect.test.cpp                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/sender/unwind_protect.hpp>
#include <smd/smdlisp/sender/unwind_protect.hpp> // test 2nd include OK

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

/// Runs @p protected_outcome under an `unwind-protect` whose cleanup bumps
/// @p log and yields @p cleanup_outcome, and returns the observed completion.
auto guarded(snd::outcome<Core> protected_outcome,
             snd::outcome<Core> cleanup_outcome, int &log)
    -> snd::outcome<Core> {
    return snd::run_sender<Core>(
        snd::unwind_protect<Core>(snd::defer_outcome<Core>([protected_outcome] {
                                      return protected_outcome;
                                  }),
                                  [cleanup_outcome, &log] {
                                      ++log;
                                      return cleanup_outcome;
                                  }));
}

auto ok(int n) -> snd::outcome<Core> { return snd::make_value<Core>(Val{n}); }
auto err(char const *m) -> snd::outcome<Core> {
    return snd::make_error<Core>(scm::foundation::parse_error{{}, m});
}
} // namespace

TEST_CASE("UnwindProtectTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- merge criterion (docs/cl-pivot-plan.md, step L21) -----------------------
// D5: `unwind-protect` is an adapter running cleanup on set_value, set_error
// AND set_stopped. These three cases ARE that sentence; they are checked here,
// on the bare adapter, before any evaluator is wired to it, so a later failure
// can be localised to the evaluator rather than to the adapter.

TEST_CASE("UnwindProtectTest - CleanupRunsOnSetValue") {
    int log = 0;
    auto o = guarded(ok(7), ok(0), log);
    REQUIRE(log == 1);
    REQUIRE(o.is_value());
    REQUIRE(std::get<int>(o.val) == 7);
}

TEST_CASE("UnwindProtectTest - CleanupRunsOnSetError") {
    int log = 0;
    auto o = guarded(err("boom"), ok(0), log);
    REQUIRE(log == 1);
    REQUIRE(o.is_error());
    REQUIRE(std::string_view{o.err.message} == "boom");
}

TEST_CASE("UnwindProtectTest - CleanupRunsOnSetStopped") {
    // The channel that only exists in this backend: an unwind in flight.
    int log = 0;
    auto o = guarded(snd::make_stopped<Core>(), ok(0), log);
    REQUIRE(log == 1);
    REQUIRE(o.is_stopped());
}

TEST_CASE("UnwindProtectTest - CleanupValueIsDiscarded") {
    int log = 0;
    auto o = guarded(ok(7), ok(99), log);
    REQUIRE(o.is_value());
    REQUIRE(std::get<int>(o.val) == 7);
}

TEST_CASE("UnwindProtectTest - AbnormalCleanupSupersedes") {
    // ANSI CL: a cleanup form that itself exits non-locally (or errors)
    // supersedes whatever was already in flight -- including an unwind.
    int log = 0;
    auto o = guarded(ok(7), err("cleanup failed"), log);
    REQUIRE(o.is_error());
    REQUIRE(std::string_view{o.err.message} == "cleanup failed");

    int log2 = 0;
    auto o2 = guarded(snd::make_stopped<Core>(), err("cleanup failed"), log2);
    REQUIRE(o2.is_error());
}

TEST_CASE("UnwindProtectTest - NestedCleanupsRunInnermostFirst") {
    // Innermost-first is a property of the sender graph's shape, not of any
    // ordering logic: the inner adapter's receiver is called before the outer
    // one's, because it is nearer the child.
    int order = 0;
    auto o = snd::run_sender<Core>(snd::unwind_protect<Core>(
        snd::unwind_protect<Core>(
            snd::defer_outcome<Core>([] { return snd::make_stopped<Core>(); }),
            [&order] {
                order = order * 10 + 1;
                return ok(0);
            }),
        [&order] {
            order = order * 10 + 2;
            return ok(0);
        }));
    REQUIRE(o.is_stopped());
    REQUIRE(order == 12);
}

TEST_CASE("UnwindProtectTest - ProtectedFormIsNotRunBeforeStart") {
    int runs = 0;
    auto s =
        snd::unwind_protect<Core>(snd::defer_outcome<Core>([&runs] {
                                      ++runs;
                                      return snd::make_value<Core>(Val{1});
                                  }),
                                  [] { return snd::make_value<Core>(Val{0}); });
    REQUIRE(runs == 0);
    static_cast<void>(snd::run_sender<Core>(std::move(s)));
    REQUIRE(runs == 1);
}
