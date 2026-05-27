# Step 7: Create dump_scheme_plan top-level API

**Phase:** 4 — Integration  
**Prereqs:** Steps 1–6 (all components exist)  
**Next step:** step-08.md

## What To Do

Wire everything together into a single `dump_scheme_plan` function template that takes a sender type and writes its execution plan as Graphviz DOT to an output stream.

## Files to Create

### 1. `src/smd/smdscheme/sender/dump_scheme_plan.hpp`

```cpp
// src/smd/smdscheme/sender/dump_scheme_plan.hpp                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_DUMP_SCHEME_PLAN_HPP
#define SRC_SMD_SMDSCHEME_SENDER_DUMP_SCHEME_PLAN_HPP

#include <smd/smdscheme/sender/build_scheme_tree.hpp>
#include <smd/smdscheme/sender/format_scheme_node.hpp>
#include <smd/smdscheme/sender/string_writer.hpp>

#include <iostream>
#include <string>

namespace smd::smdscheme::sender {

// Build the DOT string for a scheme_tree by formatting each node.
template <int MaxNodes>
auto format_tree(scheme_tree<MaxNodes> const &tree) -> std::string {
    std::string dot = "digraph SchemeExecutionPlan {\n";
    dot += "  node [shape=Mrecord, style=filled, fillcolor=lightcyan];\n";

    for (int i = 0; i < static_cast<int>(tree.nodes.size()); ++i) {
        auto fragment = format_scheme_node(tree.get(i));
        dot += fragment.log;
    }

    dot += "}\n";
    return dot;
}

// Top-level API: reflect on a Sender type, build tree, emit DOT.
//
// Usage:
//   auto s = sender_v::then(sender_v::just(1), [](int x){ return x+1; });
//   dump_scheme_plan<decltype(s)>(std::cout);
//
// Or with a compiled scheme program:
//   auto prog = sender_cps::compile_sender_cps<32,16>("(+ 1 2)");
//   // dump_scheme_plan needs the sender TYPE, not a runtime value.
template <typename Sender, int MaxNodes = 64>
void dump_scheme_plan(std::ostream &out = std::cout) {
    // 1. Build the tree at compile time via reflection
    constexpr auto tree = [] {
        scheme_tree<MaxNodes> t{};
        auto root = build_scheme_tree<MaxNodes>(^^Sender, t);
        t.root_id = root;
        return t;
    }();

    // 2. Format each node into DOT at runtime
    out << format_tree(tree);
}

} // namespace smd::smdscheme::sender

#endif
```

Key design:
- `dump_scheme_plan<SenderType>()` — the sender type is a template parameter because reflection needs the type at compile time.
- `build_scheme_tree` runs at compile time (`constexpr`/`consteval`).
- `format_tree` runs at runtime (produces `std::string` DOT output).
- `format_scheme_node` is called per-node, accumulating DOT fragments.

### 2. `src/smd/smdscheme/sender/dump_scheme_plan.test.cpp`

```cpp
// src/smd/smdscheme/sender/dump_scheme_plan.test.cpp            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/dump_scheme_plan.hpp>
#include <smd/smdscheme/sender/dump_scheme_plan.hpp> // test 2nd include OK

#include <smd/smdscheme/sender/sender_v.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

using namespace smd::smdscheme;

TEST_CASE("DumpSchemePlanTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("DumpSchemePlanTest - JustSender") {
    using JustType = decltype(sender_v::just(42));
    std::ostringstream out;
    sender::dump_scheme_plan<JustType>(out);
    std::string dot = out.str();

    CHECK(dot.find("digraph SchemeExecutionPlan") != std::string::npos);
    CHECK(dot.find("just") != std::string::npos);
    CHECK(dot.find("}") != std::string::npos);
}

TEST_CASE("DumpSchemePlanTest - ThenSender") {
    using ThenType = decltype(sender_v::then(
        sender_v::just(1), [](int x) { return x + 1; }));
    std::ostringstream out;
    sender::dump_scheme_plan<ThenType>(out);
    std::string dot = out.str();

    CHECK(dot.find("then") != std::string::npos);
    CHECK(dot.find("just") != std::string::npos);
    CHECK(dot.find("->") != std::string::npos); // at least one edge
}

TEST_CASE("DumpSchemePlanTest - WhenAllThenSender") {
    using ComposedType = decltype(sender_v::then(
        sender_v::when_all(sender_v::just(1), sender_v::just(2)),
        [](int a, int b) { return a + b; }));
    std::ostringstream out;
    sender::dump_scheme_plan<ComposedType>(out);
    std::string dot = out.str();

    CHECK(dot.find("then") != std::string::npos);
    CHECK(dot.find("when_all") != std::string::npos);
    // Should have edges from then→when_all and when_all→just nodes
    auto edge_count = 0;
    std::string::size_type pos = 0;
    while ((pos = dot.find("->", pos)) != std::string::npos) {
        ++edge_count;
        pos += 2;
    }
    CHECK(edge_count >= 3); // then→when_all, when_all→just, when_all→just
}
```

## Files to Modify

### 3. `src/smd/smdscheme/sender/CMakeLists.txt`

Add `dump_scheme_plan.hpp` to FILES list.

## Verification

```bash
make compile && make test
```

All `DumpSchemePlanTest` tests must pass. The DOT output should be valid Graphviz.

## Handoff

Update `docs/sender-printer/handoff-next.md` with:
- Confirmation that the constexpr tree build + runtime DOT output pipeline works
- Sample DOT output for `then(when_all(just(1), just(2)), fn)`
- Any issues with `constexpr` evaluation depth or limits
- Mark Step 7 complete in `docs/sender-printer/checklist.md`
