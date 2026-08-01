// src/smd/cl/foundation/arena_box.test.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Adapted by copy from compile-time-forth (smd::forth::foundation):
// src/smd/forth/foundation/arena_box.test.cpp. The compile-time-scheme
// copy was a two-line stub.

#include <smd/cl/foundation/arena_box.hpp>
#include <smd/cl/foundation/arena_box.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::arena_box;
using smd::cl::foundation::make_arena_box;
using smd::cl::foundation::tree_arena;

TEST_CASE("ArenaBoxTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ArenaBoxTest - NullHandleIsFalse") {
    constexpr arena_box<int> b;
    CHECK(!static_cast<bool>(b));
}

TEST_CASE("ArenaBoxTest - ConstructedHandleIsTrue") {
    constexpr arena_box<int> b{0};
    CHECK(static_cast<bool>(b));
}

namespace {

/// A trivially destructible test node for a small local `tree_arena`, per
/// the F3 merge criteria: build an arena of this type and read a node back
/// by `arena_box` handle, in a `static_assert`.
struct point_node {
    int x{};
    int y{};

    friend constexpr auto operator==(point_node, point_node) -> bool = default;
};

// The immediately-invoked-lambda `static_assert` pattern: build a small
// arena of `point_node`s and read one back by handle, all at compile time.
constexpr auto build_and_read_back = [] {
    tree_arena<point_node, 8> arena;
    auto origin = make_arena_box(arena, point_node{0, 0});
    auto unit = make_arena_box(arena, point_node{1, 1});
    return arena.get(unit).x + arena.get(origin).x + arena.get(unit).y;
}();

static_assert(build_and_read_back == 2);

} // namespace

TEST_CASE("ArenaBoxTest - TreeArenaAllocateAndGetById") {
    tree_arena<point_node, 4> arena;
    int id = arena.allocate(point_node{3, 4});
    CHECK(arena.get(id) == point_node{3, 4});
}

TEST_CASE("ArenaBoxTest - TreeArenaAllocateAndGetByHandle") {
    tree_arena<point_node, 4> arena;
    auto handle = make_arena_box(arena, point_node{5, 6});
    CHECK(static_cast<bool>(handle));
    CHECK(arena.get(handle) == point_node{5, 6});
}

TEST_CASE("ArenaBoxTest - TreeArenaAppendsInOrder") {
    tree_arena<point_node, 4> arena;
    auto first = make_arena_box(arena, point_node{1, 1});
    auto second = make_arena_box(arena, point_node{2, 2});
    CHECK(first.id_ == 0);
    CHECK(second.id_ == 1);
    CHECK(arena.data.size() == 2);
}

TEST_CASE("ArenaBoxTest - MutatingAccessThroughHandle") {
    tree_arena<point_node, 4> arena;
    auto handle = make_arena_box(arena, point_node{0, 0});
    arena.get(handle).x = 42;
    CHECK(arena.get(handle) == point_node{42, 0});
}

TEST_CASE("ArenaBoxTest - CompileTimeBuildAndReadBack") {
    // Runtime re-check of the static_assert above, for visibility in the
    // Catch2 report.
    CHECK(build_and_read_back == 2);
}
