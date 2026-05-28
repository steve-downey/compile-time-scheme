// src/smd/smdscheme/sender/format_scheme_node.test.cpp         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/format_scheme_node.hpp>
#include <smd/smdscheme/sender/format_scheme_node.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <string>

using smd::smdscheme::sender::format_scheme_node;
using smd::smdscheme::sender::scheme_node_data;

TEST_CASE("FormatSchemeNodeTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("FormatSchemeNodeTest - LeafNode") {
    scheme_node_data node{};
    node.node_id = 0;
    node.sender_algo = "just";
    node.scheme_context = "Atom: 42";

    auto result = format_scheme_node(node);
    CHECK(result.log.find("n0") != std::string::npos);
    CHECK(result.log.find("just") != std::string::npos);
    CHECK(result.log.find("Atom: 42") != std::string::npos);
    CHECK(result.log.find("->") == std::string::npos);
}

TEST_CASE("FormatSchemeNodeTest - NodeWithChildren") {
    scheme_node_data node{};
    node.node_id = 0;
    node.sender_algo = "when_all";
    node.scheme_context = "Parallel Args";
    node.child_ids.push_back(1);
    node.child_ids.push_back(2);

    auto result = format_scheme_node(node);
    CHECK(result.log.find("when_all") != std::string::npos);
    CHECK(result.log.find("n0 -> n1") != std::string::npos);
    CHECK(result.log.find("n0 -> n2") != std::string::npos);
}

TEST_CASE("FormatSchemeNodeTest - ValidDotSyntax") {
    scheme_node_data node{};
    node.node_id = 3;
    node.sender_algo = "then";
    node.scheme_context = "Apply: +";
    node.child_ids.push_back(4);

    auto result = format_scheme_node(node);
    CHECK(result.log.find(";\n") != std::string::npos);
}
