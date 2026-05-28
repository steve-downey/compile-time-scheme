# Step 3: Reflection bridge — build_scheme_tree for `just`

**Phase:** 2 — Reflection Bridge
**Prereqs:** Steps 1–2 (scheme_node_data and scheme_tree exist)
**Next step:** step-04.md

## What To Do

Create the core reflection bridge that uses C++26 `<meta>` to introspect a sender type and populate a `scheme_tree`. Start with only `just` senders — step 4 will extend to `then` and `when_all`.

## Background

The project already uses C++26 reflection in `reflection/reified_environment.hpp` with `#include <meta>`, `std::meta::info`, `std::meta::identifier_of()`, etc. The sender package already has `-freflection` set via `target_compile_options(smdscheme.sender INTERFACE -freflection)`.

The sender types from Beman Execution (e.g., the return type of `sender_v::just(42)`) are template instantiations. Reflection lets us inspect their names and template arguments at compile time.

## Approach

The function `build_scheme_tree` takes a `std::meta::info` representing the sender type and recursively builds the tree. For this step, only handle `just`:

1. Use `std::meta::identifier_of(sender_type)` or `std::meta::display_string_of(sender_type)` to get the sender algorithm name.
2. Check if the name contains "just" (the exact identifier depends on the Beman implementation — you'll need to inspect what GCC16 gives you).
3. Create a `scheme_node_data` with `sender_algo = "just"` and allocate it in the tree.

## Files to Create

### 1. `src/smd/smdscheme/sender/build_scheme_tree.hpp`

```cpp
// src/smd/smdscheme/sender/build_scheme_tree.hpp               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_BUILD_SCHEME_TREE_HPP
#define SRC_SMD_SMDSCHEME_SENDER_BUILD_SCHEME_TREE_HPP

#include <smd/smdscheme/sender/scheme_tree.hpp>

#include <meta>
#include <string_view>

namespace smd::smdscheme::sender {

// Inspect a sender type and classify its algorithm.
// Returns a string_view like "just", "then", "when_all", or "unknown".
consteval auto classify_sender(std::meta::info type) -> std::string_view {
    auto name = std::meta::identifier_of(type);
    // The Beman execution types will have internal names.
    // Use display_string_of if identifier_of returns empty.
    // Check for known algorithm names in the type name.
    // IMPORTANT: the agent executing this step MUST inspect the actual
    // type names GCC16 gives for just/then/when_all senders and adjust
    // the string matching accordingly. Try:
    //   static_assert(std::meta::identifier_of(^^decltype(sender_v::just(1))) == "...");
    // to discover the actual names.

    // Placeholder — adjust based on actual GCC16 output:
    if (/* name contains "just" */) return "just";
    return "unknown";
}

// Build a scheme_tree by reflecting on a Sender type.
// Call as: build_scheme_tree<MaxNodes>(^^SenderType, tree)
template <int MaxNodes = 64>
consteval auto build_scheme_tree(std::meta::info sender_type,
                                  scheme_tree<MaxNodes>& tree) -> int {
    auto algo = classify_sender(sender_type);

    scheme_node_data node{};
    node.sender_algo = algo;

    // For "just", it's a leaf — no children.
    auto id = tree.allocate(node);
    return id;
}

} // namespace smd::smdscheme::sender

#endif
```

**CRITICAL NOTE FOR AGENT:** The `classify_sender` function's string matching is a discovery task. You MUST:
1. Write a small test that prints/asserts `std::meta::identifier_of(^^decltype(sender_v::just(42)))`.
2. If `identifier_of` returns empty, try `std::meta::display_string_of()`.
3. Adjust the matching logic based on what GCC16 actually returns.
4. Document the discovered type names in `handoff-next.md`.

### 2. `src/smd/smdscheme/sender/build_scheme_tree.test.cpp`

```cpp
// src/smd/smdscheme/sender/build_scheme_tree.test.cpp          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/build_scheme_tree.hpp>
#include <smd/smdscheme/sender/build_scheme_tree.hpp> // test 2nd include OK

#include <smd/smdscheme/sender/sender_v.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace smd::smdscheme;

// Discovery test: what does GCC16 call the just sender type?
// Uncomment and check compiler output to discover actual names:
// static_assert([] {
//     constexpr auto name = std::meta::identifier_of(^^decltype(sender_v::just(42)));
//     return name == "???";  // fill in after discovery
// }());

TEST_CASE("BuildSchemeTreeTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("BuildSchemeTreeTest - JustSender") {
    // This test verifies that build_scheme_tree recognizes a just sender.
    // The exact implementation depends on what GCC16 reflection returns.
    // See handoff-next.md for the discovered type names.
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
```

## Files to Modify

### 3. `src/smd/smdscheme/sender/CMakeLists.txt`

Add `build_scheme_tree.hpp` to FILES list.

## Verification

```bash
make compile && make test
```

The build_scheme_tree test must pass. If reflection API names don't match expectations, iterate on `classify_sender` until `just` is correctly recognized.

## Handoff

Update `docs/sender-printer/handoff-next.md` with:
- **The actual type name** GCC16 gives for `decltype(sender_v::just(42))` via reflection
- Whether `identifier_of` or `display_string_of` was needed
- The exact string matching logic that worked for "just"
- Any `-freflection` or `<meta>` issues encountered
- Mark Step 3 complete in `docs/sender-printer/checklist.md`

This information is CRITICAL for Step 4 — the next agent needs to know the exact reflection API behavior.
