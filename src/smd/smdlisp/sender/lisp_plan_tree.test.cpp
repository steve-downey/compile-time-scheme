// src/smd/smdlisp/sender/lisp_plan_tree.test.cpp                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/sender/lisp_plan_tree.hpp>
#include <smd/smdlisp/sender/lisp_plan_tree.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

namespace {
namespace snd = smd::smdlisp::sender;
} // namespace

TEST_CASE("LispPlanTreeTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- the container, independent of anything that reflects into it ------------
// `build_lisp_tree.test.cpp` owns what gets PUT in the tree; this file owns
// only that the tree stores and addresses it.

// `allocate` overwrites the caller's `node_id` with the assigned index, so a
// caller cannot accidentally pick its own and desynchronise the edge lists.
static_assert([] {
    snd::lisp_plan_tree<8> t{};
    auto const id = t.allocate(snd::lisp_node_data{99, "just", "", {}});
    return id == 0 && t.get(0).node_id == 0;
}());

// Ids are handed out in allocation order, which is what makes the pre-order
// walk in `build_lisp_tree` produce contiguous, renderable indices.
static_assert([] {
    snd::lisp_plan_tree<8> t{};
    auto const a = t.allocate(snd::lisp_node_data{});
    auto const b = t.allocate(snd::lisp_node_data{});
    auto const c = t.allocate(snd::lisp_node_data{});
    return a == 0 && b == 1 && c == 2 && t.nodes.size() == 3;
}());

TEST_CASE("LispPlanTreeTest - RootIdStartsUnset") {
    snd::lisp_plan_tree<8> t{};
    REQUIRE(t.root_id == -1);
    REQUIRE(t.nodes.size() == 0);
}

TEST_CASE("LispPlanTreeTest - MutableGetEditsInPlace") {
    // `build_lisp_tree` relies on this: it allocates a parent, recurses to get
    // a child id, then pushes that id onto the ALREADY-allocated parent.
    snd::lisp_plan_tree<8> t{};
    auto const parent =
        t.allocate(snd::lisp_node_data{0, "unwind_protect", "", {}});
    auto const child = t.allocate(snd::lisp_node_data{0, "defer", "", {}});
    t.get(parent).child_ids.push_back(child);

    REQUIRE(t.get(parent).child_ids.size() == 1);
    REQUIRE(t.get(parent).child_ids[0] == 1);
    REQUIRE(t.get(child).child_ids.size() == 0);
}

TEST_CASE("LispPlanTreeTest - NodeDataComparesByValue") {
    snd::lisp_node_data const a{0, "just", "Atom: 5", {}};
    snd::lisp_node_data const b{0, "just", "Atom: 5", {}};
    snd::lisp_node_data const c{0, "just_error", "Atom: 5", {}};
    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);
}
