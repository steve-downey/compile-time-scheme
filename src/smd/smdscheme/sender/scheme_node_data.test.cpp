// src/smd/smdscheme/sender/scheme_node_data.test.cpp           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/scheme_node_data.hpp>
#include <smd/smdscheme/sender/scheme_node_data.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::smdscheme::sender::scheme_node_data;

static_assert(scheme_node_data{} == scheme_node_data{});

static_assert([] {
    scheme_node_data n{};
    n.node_id = 1;
    n.sender_algo = "just";
    n.scheme_context = "Atom: 42";
    return n.sender_algo == "just" && n.node_id == 1;
}());

static_assert([] {
    scheme_node_data n{};
    n.node_id = 0;
    n.child_ids.push_back(1);
    n.child_ids.push_back(2);
    return n.child_ids.size() == 2 && n.child_ids[0] == 1 &&
           n.child_ids[1] == 2;
}());

TEST_CASE("SchemeNodeDataTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("SchemeNodeDataTest - DefaultConstruction") {
    scheme_node_data n{};
    CHECK(n.node_id == 0);
    CHECK(n.sender_algo == "");
    CHECK(n.scheme_context == "");
    CHECK(n.child_ids.size() == 0);
}

TEST_CASE("SchemeNodeDataTest - WithChildren") {
    scheme_node_data n{};
    n.node_id = 5;
    n.sender_algo = "when_all";
    n.scheme_context = "Parallel Args";
    n.child_ids.push_back(6);
    n.child_ids.push_back(7);
    CHECK(n.child_ids.size() == 2);
    CHECK(n.child_ids[0] == 6);
    CHECK(n.child_ids[1] == 7);
}
