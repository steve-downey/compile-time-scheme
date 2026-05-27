// src/smd/smdscheme/sender/scheme_tree.test.cpp                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/scheme_tree.hpp>
#include <smd/smdscheme/sender/scheme_tree.hpp>  // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::smdscheme::sender::scheme_node_data;
using smd::smdscheme::sender::scheme_tree;

static_assert([] {
    scheme_tree<16> tree{};
    auto root_id = tree.allocate(scheme_node_data{.sender_algo = "when_all"});
    auto child1  = tree.allocate(scheme_node_data{.sender_algo = "just"});
    auto child2  = tree.allocate(scheme_node_data{.sender_algo = "just"});
    tree.get(root_id).child_ids.push_back(child1);
    tree.get(root_id).child_ids.push_back(child2);
    tree.root_id = root_id;
    return tree.nodes.size() == 3 &&
           tree.get(root_id).child_ids.size() == 2 &&
           tree.get(child1).sender_algo == "just";
}());

TEST_CASE("SchemeTreeTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("SchemeTreeTest - AllocateAndGet") {
    scheme_tree<16> tree{};
    auto id = tree.allocate(scheme_node_data{.sender_algo = "then"});
    CHECK(id == 0);
    CHECK(tree.get(id).sender_algo == "then");
    CHECK(tree.get(id).node_id == 0);
}

TEST_CASE("SchemeTreeTest - BuildSimpleTree") {
    scheme_tree<16> tree{};
    auto root  = tree.allocate(scheme_node_data{.sender_algo = "then"});
    auto child = tree.allocate(
        scheme_node_data{.sender_algo = "just", .scheme_context = "Atom: 1"});
    tree.get(root).child_ids.push_back(child);
    tree.root_id = root;

    CHECK(tree.nodes.size() == 2);
    CHECK(tree.get(tree.root_id).child_ids[0] == child);
    CHECK(tree.get(child).scheme_context == "Atom: 1");
}
