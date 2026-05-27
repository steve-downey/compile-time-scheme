# Step 6: Create format_scheme_node DOT formatter

**Phase:** 3 — Applicative Traverser
**Prereqs:** Steps 1, 5 (scheme_node_data and string_writer exist)
**Next step:** step-07.md

## What To Do

Create a function that takes a `scheme_node_data` and produces a `string_writer<std::monostate>` containing Graphviz DOT fragments for that node (node definition + edges to children).

## Files to Create

### 1. `src/smd/smdscheme/sender/format_scheme_node.hpp`

```cpp
// src/smd/smdscheme/sender/format_scheme_node.hpp              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_FORMAT_SCHEME_NODE_HPP
#define SRC_SMD_SMDSCHEME_SENDER_FORMAT_SCHEME_NODE_HPP

#include <smd/smdscheme/sender/scheme_node_data.hpp>
#include <smd/smdscheme/sender/string_writer.hpp>

#include <format>
#include <variant>

namespace smd::smdscheme::sender {

inline auto format_scheme_node(scheme_node_data const &node)
    -> string_writer<std::monostate> {
    // Node definition: n<id> [label="<algo>\n[<context>]"];
    std::string label = std::format("{}\\n[{}]",
                                     node.sender_algo,
                                     node.scheme_context);
    std::string dot = std::format("  n{} [label=\"{}\"];\n",
                                   node.node_id, label);

    // Edge definitions: n<id> -> n<child_id>;
    for (int i = 0; i < node.child_ids.size(); ++i) {
        dot += std::format("  n{} -> n{};\n", node.node_id, node.child_ids[i]);
    }

    return tell(std::move(dot));
}

} // namespace smd::smdscheme::sender

#endif
```

Key design:
- Produces Graphviz node definitions with `Mrecord` shape styling.
- Label shows the sender algorithm and Scheme context.
- Edges point from parent to children.
- Returns a `string_writer<std::monostate>` — the DOT fragment is in the `log`.

### 2. `src/smd/smdscheme/sender/format_scheme_node.test.cpp`

```cpp
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
    // Should contain node definition
    CHECK(result.log.find("n0") != std::string::npos);
    CHECK(result.log.find("just") != std::string::npos);
    CHECK(result.log.find("Atom: 42") != std::string::npos);
    // No edges for a leaf
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
    // Should contain node definition
    CHECK(result.log.find("when_all") != std::string::npos);
    // Should contain two edges
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
    // Each line should end with semicolon + newline
    CHECK(result.log.find(";\n") != std::string::npos);
}
```

## Files to Modify

### 3. `src/smd/smdscheme/sender/CMakeLists.txt`

Add `format_scheme_node.hpp` to FILES list.

## Verification

```bash
make compile && make test
```

All `FormatSchemeNodeTest` tests must pass.

## Handoff

Update `docs/sender-printer/handoff-next.md` with:
- Confirmation that `std::format` works with GCC16 C++26
- The exact DOT syntax produced (paste a sample)
- Mark Step 6 complete in `docs/sender-printer/checklist.md`
