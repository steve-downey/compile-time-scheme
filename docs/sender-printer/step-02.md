# Step 2: Create scheme_tree type

**Phase:** 1 — Runtime Topology
**Prereqs:** Step 1 (scheme_node_data exists)
**Next step:** step-03.md

## What To Do

Create a tree storage type that holds the flattened sender topology. Use the project's existing arena pattern (`tree_arena` + `arena_box`) rather than recursive pointer-based trees.

## Design Decision

The sender graph is a DAG (directed acyclic graph) stored flat in an arena. Each `scheme_node_data` references children by `node_id` (which is the arena index). The `scheme_tree` struct bundles an arena with the root index.

## Files to Create

### 1. `src/smd/smdscheme/sender/scheme_tree.hpp`

```cpp
// src/smd/smdscheme/sender/scheme_tree.hpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_SCHEME_TREE_HPP
#define SRC_SMD_SMDSCHEME_SENDER_SCHEME_TREE_HPP

#include <smd/smdscheme/foundation/static_vector.hpp>
#include <smd/smdscheme/sender/scheme_node_data.hpp>

namespace smd::smdscheme::sender {

template <int MaxNodes = 64>
struct scheme_tree {
    foundation::static_vector<scheme_node_data, MaxNodes> nodes{};
    int root_id{-1};

    constexpr auto allocate(scheme_node_data node) -> int {
        int id = static_cast<int>(nodes.size());
        node.node_id = id;
        nodes.push_back(node);
        return id;
    }

    constexpr auto get(int id) const -> scheme_node_data const & {
        return nodes[id];
    }

    constexpr auto get(int id) -> scheme_node_data & {
        return nodes[id];
    }
};

} // namespace smd::smdscheme::sender

#endif
```

Key decisions:
- Flat arena storage using `static_vector<scheme_node_data, MaxNodes>`.
- `allocate()` assigns `node_id` automatically from the position in the vector.
- `root_id` marks which node is the outermost sender.
- Default capacity 64 nodes (enough for moderately complex expressions).
- Fully constexpr.

### 2. `src/smd/smdscheme/sender/scheme_tree.test.cpp`

```cpp
// src/smd/smdscheme/sender/scheme_tree.test.cpp                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/scheme_tree.hpp>
#include <smd/smdscheme/sender/scheme_tree.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::smdscheme::sender::scheme_node_data;
using smd::smdscheme::sender::scheme_tree;

static_assert([] {
    scheme_tree<16> tree{};
    auto root_id = tree.allocate(scheme_node_data{.sender_algo = "when_all"});
    auto child1 = tree.allocate(scheme_node_data{.sender_algo = "just"});
    auto child2 = tree.allocate(scheme_node_data{.sender_algo = "just"});
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
    auto root = tree.allocate(scheme_node_data{.sender_algo = "then"});
    auto child = tree.allocate(scheme_node_data{.sender_algo = "just",
                                                 .scheme_context = "Atom: 1"});
    tree.get(root).child_ids.push_back(child);
    tree.root_id = root;

    CHECK(tree.nodes.size() == 2);
    CHECK(tree.get(tree.root_id).child_ids[0] == child);
    CHECK(tree.get(child).scheme_context == "Atom: 1");
}
```

## Files to Modify

### 3. `src/smd/smdscheme/sender/CMakeLists.txt`

Add `scheme_tree.hpp` to the FILES list:
```cmake
FILES sender_program.hpp sender_v.hpp sender_cps_program.hpp scheme_node_data.hpp scheme_tree.hpp
```

## Verification

```bash
make compile && make test
```

New `SchemeTreeTest` tests must appear and pass. All existing tests still pass.

## Handoff

Update `docs/sender-printer/handoff-next.md` with:
- Confirmation that `scheme_tree` compiles at constexpr
- The MaxNodes default (64) and whether designated initializers worked with GCC16
- Mark Step 2 complete in `docs/sender-printer/checklist.md`
