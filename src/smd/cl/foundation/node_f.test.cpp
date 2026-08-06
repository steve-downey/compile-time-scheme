// src/smd/cl/foundation/node_f.test.cpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/foundation/node_f.hpp>
#include <smd/cl/foundation/node_f.hpp> // test 2nd include OK

#include <smd/cl/foundation/static_vector.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

using smd::cl::foundation::branch_f;
using smd::cl::foundation::fmap_children;
using smd::cl::foundation::node_f;
using smd::cl::foundation::static_vector;

namespace {

// A layer over int children, the stored form (R = int).
using layer = node_f<int, char, int, 4>;

constexpr auto sample_branch() -> layer {
    branch_f<char, int, 4> b{'L', {}};
    b.children.push_back(0);
    b.children.push_back(1);
    b.children.push_back(2);
    return layer{b};
}

constexpr auto add_one = [](int const &c) { return c + 1; };
constexpr auto times_two = [](int const &c) { return c * 2; };

// Functor law: identity. fmap_children(id) changes nothing.
constexpr auto identity_law() -> bool {
    auto const mapped =
        fmap_children([](int const &c) { return c; }, sample_branch());
    return mapped == sample_branch() &&
           fmap_children([](int const &c) { return c; }, layer{7}) == layer{7};
}

// Functor law: composition. fmap(g . f) == fmap(g) . fmap(f).
constexpr auto composition_law() -> bool {
    auto const composed = fmap_children(
        [](int const &c) { return times_two(add_one(c)); }, sample_branch());
    auto const chained =
        fmap_children(times_two, fmap_children(add_one, sample_branch()));
    return composed == chained;
}

// A leaf has no recursive positions: it passes through untouched, and
// only its slot type changes.
constexpr auto leaf_passes_through() -> bool {
    auto const mapped = fmap_children(
        [](int const &) { return std::string_view{"child"}; }, layer{42});
    return std::holds_alternative<int>(mapped) && std::get<int>(mapped) == 42;
}

// A branch's tag is not a recursive position either.
constexpr auto tag_is_untouched() -> bool {
    auto const mapped = fmap_children(add_one, sample_branch());
    auto const &branch = std::get<branch_f<char, int, 4>>(mapped);
    return branch.tag == 'L' && branch.children.size() == 3 &&
           branch.children[0] == 1 && branch.children[1] == 2 &&
           branch.children[2] == 3;
}

// The slot type genuinely changes: mapping int -> bool yields the same
// layer template at a different R.
constexpr auto slot_type_changes() -> bool {
    auto const mapped =
        fmap_children([](int const &c) { return c > 1; }, sample_branch());
    auto const &branch = std::get<branch_f<char, bool, 4>>(mapped);
    return !branch.children[0] && !branch.children[1] && branch.children[2];
}

} // namespace

static_assert(identity_law());
static_assert(composition_law());
static_assert(leaf_passes_through());
static_assert(tag_is_untouched());
static_assert(slot_type_changes());

TEST_CASE("NodeFTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("NodeFTest - FunctorLaws") {
    CHECK(identity_law());
    CHECK(composition_law());
}

TEST_CASE("NodeFTest - OnlyChildSlotsAreMapped") {
    CHECK(leaf_passes_through());
    CHECK(tag_is_untouched());
    CHECK(slot_type_changes());
}
