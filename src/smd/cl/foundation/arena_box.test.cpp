// src/smd/cl/foundation/arena_box.test.cpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test (step R8): the substantive arena_box/tree_arena tests
// moved to src/smd/kit/foundation/arena_box.test.cpp with the definition.
// This file only verifies that the forwarding shim compiles and that the
// forwarded names are usable from smd::cl::foundation exactly as before.

#include <smd/cl/foundation/arena_box.hpp>
#include <smd/cl/foundation/arena_box.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::arena_box;
using smd::cl::foundation::make_arena_box;
using smd::cl::foundation::tree_arena;

TEST_CASE("ArenaBoxShimTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {
struct point_node {
    int x{};
    int y{};

    friend constexpr auto operator==(point_node, point_node) -> bool = default;
};
} // namespace

TEST_CASE("ArenaBoxShimTest - AllocateAndGetByHandleThroughShim") {
    tree_arena<point_node, 4> arena;
    auto handle = make_arena_box(arena, point_node{5, 6});
    CHECK(static_cast<bool>(handle));
    CHECK(arena.get(handle) == point_node{5, 6});
}
