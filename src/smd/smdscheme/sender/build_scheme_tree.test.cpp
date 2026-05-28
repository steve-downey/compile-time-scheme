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
        auto root      = sender::build_scheme_tree<16>(^^JustType, t);
        t.root_id      = root;
        return t;
    }();

    REQUIRE(tree.nodes.size() == 1);
    CHECK(tree.get(tree.root_id).sender_algo == "just");
}
