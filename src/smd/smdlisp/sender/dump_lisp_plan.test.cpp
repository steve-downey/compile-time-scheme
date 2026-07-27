// src/smd/smdlisp/sender/dump_lisp_plan.test.cpp                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/sender/dump_lisp_plan.hpp>
#include <smd/smdlisp/sender/dump_lisp_plan.hpp> // test 2nd include OK

#include <smd/smdlisp/elaborator/elaborated_core.hpp>
#include <smd/smdlisp/sender/sender_eval.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

namespace {
namespace lisp = smd::smdlisp;
namespace snd = smd::smdlisp::sender;
namespace sv = smd::smdlisp::sender_v;

constexpr int MaxNodes = 128;
constexpr int MaxList = 16;

using Core = lisp::elaborator::core_type<MaxNodes, MaxList>;
using Val = lisp::closure::value<Core>;

/// A stand-in for one evaluated sub-form.
auto const thunk = [] { return snd::make_value<Core>(Val{1}); };

/// The sender graph `core_unwind_protect` builds for
/// `(unwind-protect <protected> <cleanup>...)`.
///
/// This is not an illustration of the shape -- it is the shape: the two
/// factories called here, in this nesting, are exactly the two
/// `sender_eval.hpp`'s `core_unwind_protect` arm calls, with the recursive
/// `child(...)` calls replaced by constant thunks so the type can be named
/// without an evaluation context.  Anything the classifier can say about this
/// type, it says about the real graph.
using UnwindPlan =
    decltype(snd::unwind_protect<Core>(snd::defer_outcome<Core>(thunk), thunk));

/// The nested case, as in `cleanup_order_src`: a `throw` crossing two
/// `unwind-protect` frames on its way to a `catch`.
using NestedUnwindPlan = decltype(snd::unwind_protect<Core>(
    snd::unwind_protect<Core>(snd::defer_outcome<Core>(thunk), thunk), thunk));

/// A stock-algorithm graph, to show the classifier still handles the Beman
/// senders the Scheme port knew about, plus the two channels it did not.
using JoinPlan = decltype(sv::then(sv::when_all(sv::just(1), sv::just(2)),
                                   [](int a, int b) { return a + b; }));
} // namespace

TEST_CASE("DumpLispPlanTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- merge criterion (docs/cl-pivot-plan.md, step L21) -----------------------
// "A DOT dump of an `unwind-protect` example."

TEST_CASE("DumpLispPlanTest - UnwindProtectGraph") {
    auto const dot = snd::lisp_plan_dot<UnwindPlan>();
    REQUIRE(dot.find("digraph LispExecutionPlan") != std::string::npos);
    // The adapter is named, not "unknown": this is the whole reason the
    // classifier had to be extended past the Scheme one, which only knows
    // `basic_sender` tags.
    REQUIRE(dot.find("unwind_protect") != std::string::npos);
    REQUIRE(dot.find("defer") != std::string::npos);
    // The protected form hangs off the adapter as its single child.
    REQUIRE(dot.find("n0 -> n1;") != std::string::npos);
}

TEST_CASE("DumpLispPlanTest - NestedUnwindProtectGraph") {
    auto const dot = snd::lisp_plan_dot<NestedUnwindPlan>();
    auto const first = dot.find("unwind_protect");
    REQUIRE(first != std::string::npos);
    REQUIRE(dot.find("unwind_protect", first + 1) != std::string::npos);
    REQUIRE(dot.find("n0 -> n1;") != std::string::npos);
    REQUIRE(dot.find("n1 -> n2;") != std::string::npos);
}

TEST_CASE("DumpLispPlanTest - StockAlgorithmsStillClassify") {
    auto const dot = snd::lisp_plan_dot<JoinPlan>();
    REQUIRE(dot.find("then") != std::string::npos);
    REQUIRE(dot.find("when_all") != std::string::npos);
    REQUIRE(dot.find("just") != std::string::npos);
    REQUIRE(dot.find("unknown") == std::string::npos);
}

// Classification itself, and the pre-order shape of the walk, belong to
// `build_lisp_tree.hpp` and are pinned in `build_lisp_tree.test.cpp`. This
// file owns only the rendering.

TEST_CASE("DumpLispPlanTest - DumpWritesToStream") {
    std::ostringstream out;
    snd::dump_lisp_plan<UnwindPlan>(out);
    REQUIRE(out.str().find("digraph LispExecutionPlan") != std::string::npos);
}
