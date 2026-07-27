// src/smd/smdlisp/sender/build_lisp_tree.test.cpp                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/sender/build_lisp_tree.hpp>
#include <smd/smdlisp/sender/build_lisp_tree.hpp> // test 2nd include OK

#include <smd/smdlisp/elaborator/elaborated_core.hpp>
#include <smd/smdlisp/sender/unwind_protect.hpp>

#include <catch2/catch_test_macros.hpp>

#include <meta>

namespace {
namespace lisp = smd::smdlisp;
namespace snd = smd::smdlisp::sender;
namespace sv = smd::smdlisp::sender_v;

constexpr int MaxNodes = 128;
constexpr int MaxList = 16;

using Core = lisp::elaborator::core_type<MaxNodes, MaxList>;
using Val = lisp::closure::value<Core>;

auto const thunk = [] { return snd::make_value<Core>(Val{1}); };

/// Classifies @p T the way @ref snd::build_lisp_tree does, i.e. after
/// dealiasing.  Reflections preserve aliases, and every type named below is
/// one.
template <typename T>
consteval auto label() -> std::string_view {
    return snd::classify_lisp_sender(std::meta::dealias(^^T));
}
} // namespace

TEST_CASE("BuildLispTreeTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- the completion channel lives in the TAG's template argument -------------
// This is the bug the Scheme classifier has, harmlessly, because the Scheme
// backend only ever built value-channel senders. In this vendored Beman
// revision:
//
//   just_error_t   == just_t<set_error_t>
//   just_stopped_t == just_t<set_stopped_t>
//   upon_error_t   == then_t<set_error_t>
//   upon_stopped_t == then_t<set_stopped_t>
//   let_value_t    == let_t<set_value_t>
//
// so `identifier_of(template_of(tag))` answers "just_t" for all three `just`
// flavours. Reading the tag's own argument is the only way to tell them
// apart, and a `smdlisp` graph is largely ABOUT the non-value channels, so
// collapsing them would defeat the point of drawing it. Pinned directly here
// rather than only through a rendered string.

using JustPlan = decltype(sv::just(1));
using JustErrorPlan = decltype(sv::just_error(1));
using JustStoppedPlan = decltype(sv::just_stopped());

static_assert(label<JustPlan>() == "just");
static_assert(label<JustErrorPlan>() == "just_error");
static_assert(label<JustStoppedPlan>() == "just_stopped");

using ThenPlan = decltype(sv::then(sv::just(1), [](int x) { return x; }));
using UponStoppedPlan =
    decltype(sv::upon_stopped(sv::just_stopped(), [] { return 0; }));

static_assert(label<ThenPlan>() == "then");
static_assert(label<UponStoppedPlan>() == "upon_stopped");

// A tag with no template arguments names its algorithm directly.
using WhenAllPlan = decltype(sv::when_all(sv::just(1), sv::just(2)));
static_assert(label<WhenAllPlan>() == "when_all");

// -- this backend's own senders are not `basic_sender`s ----------------------
// They have no tag at all, so the tag-based path cannot see them; matching by
// template identity is what keeps the two most interesting nodes in any
// `smdlisp` graph from rendering as "unknown".

using DeferPlan = decltype(snd::defer_outcome<Core>(thunk));
using UnwindPlan =
    decltype(snd::unwind_protect<Core>(snd::defer_outcome<Core>(thunk), thunk));

static_assert(label<DeferPlan>() == "defer");
static_assert(label<UnwindPlan>() == "unwind_protect");

// -- child selection differs by sender shape ---------------------------------

// `unwind_protect_sender<Core, Child, Cleanup>` keeps its single child at
// args[1], not args[2...]: args[0] is the core type and args[2] the cleanup
// callable, so the `basic_sender` rule would pick up the cleanup as a child.
static_assert([] {
    snd::lisp_plan_tree<8> t{};
    auto const root = snd::build_lisp_tree<8>(^^UnwindPlan, t);
    return root == 0 && t.nodes.size() == 2 &&
           t.get(0).sender_algo == "unwind_protect" &&
           t.get(0).child_ids.size() == 1 && t.get(0).child_ids[0] == 1 &&
           t.get(1).sender_algo == "defer";
}());

// `defer_outcome_sender` is a leaf: its thunk is opaque to reflection.
static_assert([] {
    snd::lisp_plan_tree<8> t{};
    auto const root = snd::build_lisp_tree<8>(^^DeferPlan, t);
    return root == 0 && t.nodes.size() == 1 && t.get(0).child_ids.size() == 0;
}());

// `basic_sender<Tag, Data, Child...>`: children start at args[2].
static_assert([] {
    snd::lisp_plan_tree<8> t{};
    snd::build_lisp_tree<8>(^^WhenAllPlan, t);
    return t.nodes.size() == 3 && t.get(0).sender_algo == "when_all" &&
           t.get(0).child_ids.size() == 2 && t.get(1).sender_algo == "just" &&
           t.get(2).sender_algo == "just";
}());

// Nodes are appended in pre-order, so a parent always precedes its children.
static_assert([] {
    using Nested = decltype(snd::unwind_protect<Core>(
        snd::unwind_protect<Core>(snd::defer_outcome<Core>(thunk), thunk),
        thunk));
    snd::lisp_plan_tree<16> t{};
    t.root_id = snd::build_lisp_tree<16>(^^Nested, t);
    return t.root_id == 0 && t.nodes.size() == 3 &&
           t.get(0).sender_algo == "unwind_protect" &&
           t.get(1).sender_algo == "unwind_protect" &&
           t.get(2).sender_algo == "defer";
}());

TEST_CASE("BuildLispTreeTest - ClassifierAndWalkAreConstevalOnly") {
    // Every assertion in this file is a static_assert: the classifier is
    // `consteval` and the walk reflects on types, so there is nothing to run.
    // This case exists so the file reports as a test, not so it checks
    // anything the assertions above have not already proved at compile time.
    REQUIRE(true);
}
