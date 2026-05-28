// src/smd/smdscheme/sender/build_scheme_tree.test.cpp          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/build_scheme_tree.hpp>
#include <smd/smdscheme/sender/build_scheme_tree.hpp> // test 2nd include OK

#include <smd/smdscheme/sender/sender_v.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace smd::smdscheme;

TEST_CASE("BuildSchemeTreeTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("BuildSchemeTreeTest - JustSender") {
    constexpr auto tree = [] {
        sender::scheme_tree<16> t{};
        using JustType = decltype(sender_v::just(42));
        auto root = sender::build_scheme_tree<16>(^^JustType, t);
        t.root_id = root;
        return t;
    }();

    REQUIRE(tree.nodes.size() == 1);
    CHECK(tree.get(tree.root_id).sender_algo == "just");
}

TEST_CASE("BuildSchemeTreeTest - ThenSender") {
    constexpr auto tree = [] {
        sender::scheme_tree<16> t{};
        using ThenType = decltype(sender_v::then(sender_v::just(1),
                                                 [](int x) { return x + 1; }));
        auto root = sender::build_scheme_tree<16>(^^ThenType, t);
        t.root_id = root;
        return t;
    }();

    REQUIRE(tree.nodes.size() == 2);
    CHECK(tree.get(tree.root_id).sender_algo == "then");
    REQUIRE(tree.get(tree.root_id).child_ids.size() == 1);
    auto child_id = tree.get(tree.root_id).child_ids[0];
    CHECK(tree.get(child_id).sender_algo == "just");
}

TEST_CASE("BuildSchemeTreeTest - WhenAllSender") {
    constexpr auto tree = [] {
        sender::scheme_tree<16> t{};
        using WhenAllType =
            decltype(sender_v::when_all(sender_v::just(1), sender_v::just(2)));
        auto root = sender::build_scheme_tree<16>(^^WhenAllType, t);
        t.root_id = root;
        return t;
    }();

    REQUIRE(tree.nodes.size() == 3);
    CHECK(tree.get(tree.root_id).sender_algo == "when_all");
    CHECK(tree.get(tree.root_id).child_ids.size() == 2);
}

TEST_CASE("BuildSchemeTreeTest - NestedSender") {
    constexpr auto tree = [] {
        sender::scheme_tree<16> t{};
        using NestedType = decltype(sender_v::then(
            sender_v::when_all(sender_v::just(1), sender_v::just(2)),
            [](int a, int b) { return a + b; }));
        auto root = sender::build_scheme_tree<16>(^^NestedType, t);
        t.root_id = root;
        return t;
    }();

    REQUIRE(tree.nodes.size() == 4); // then, when_all, just, just
    CHECK(tree.get(tree.root_id).sender_algo == "then");
    REQUIRE(tree.get(tree.root_id).child_ids.size() == 1);
    auto when_all_id = tree.get(tree.root_id).child_ids[0];
    CHECK(tree.get(when_all_id).sender_algo == "when_all");
    CHECK(tree.get(when_all_id).child_ids.size() == 2);
}
