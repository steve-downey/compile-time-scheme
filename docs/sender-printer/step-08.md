# Step 8: Example program and documentation update

**Phase:** 4 — Integration
**Prereqs:** Steps 1–7 (full pipeline works)
**Next step:** None — this is the final step.

## What To Do

Create a standalone example program that demonstrates the sender graph printer, and update documentation to reflect completion.

## Files to Create

### 1. `src/examples/sender_graph_demo.cpp`

```cpp
// src/examples/sender_graph_demo.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/dump_scheme_plan.hpp>
#include <smd/smdscheme/sender/sender_v.hpp>

#include <iostream>

int main() {
    // Demonstrate DOT output for a composed sender expression.
    // This shows the execution plan for: then(when_all(just(1), just(2)), add)
    using Sender = decltype(smd::smdscheme::sender_v::then(
        smd::smdscheme::sender_v::when_all(
            smd::smdscheme::sender_v::just(1),
            smd::smdscheme::sender_v::just(2)),
        [](int a, int b) { return a + b; }));

    std::cout << "// Sender execution plan for: (+ 1 2)\n";
    std::cout << "// Pipe to: dot -Tpng -o plan.png\n\n";
    smd::smdscheme::sender::dump_scheme_plan<Sender>(std::cout);
    return 0;
}
```

### 2. Modify `src/examples/CMakeLists.txt`

Add the new example executable. Find the existing example targets and add:

```cmake
add_executable(sender_graph_demo sender_graph_demo.cpp)
target_link_libraries(sender_graph_demo PRIVATE smdscheme.sender)
target_compile_options(sender_graph_demo PRIVATE -freflection)

if(SCHEMEPOC_ENABLE_TESTING)
    add_test(NAME examples_sender_graph_demo COMMAND sender_graph_demo)
endif()
```

### 3. Update `docs/sender_graphing.md`

Add a "Status" section at the top (after the title):

```markdown
## Status

**Implemented:** Phases 1–4 are complete.

- Phase 1: `scheme_node_data.hpp`, `scheme_tree.hpp` — runtime topology types
- Phase 2: `build_scheme_tree.hpp` — C++26 reflection bridge
- Phase 3: `string_writer.hpp`, `format_scheme_node.hpp` — Applicative DOT traverser
- Phase 4: `dump_scheme_plan.hpp` — top-level API

**Example:** `src/examples/sender_graph_demo.cpp`

**Usage:**
```bash
./build/src/examples/sender_graph_demo | dot -Tpng -o plan.png
```
```

## Verification

```bash
make compile && make test
```

Then run the example manually:
```bash
.build/build-gcc-16/src/examples/Asan/sender_graph_demo
```

Verify it produces valid DOT output starting with `digraph SchemeExecutionPlan {`.

Optionally, if `dot` is available:
```bash
.build/build-gcc-16/src/examples/Asan/sender_graph_demo | dot -Tpng -o /tmp/plan.png
```

## Handoff

Update `docs/sender-printer/handoff-next.md` with:
- Final DOT output from the example
- Confirmation all tests pass
- Mark Step 8 complete in `docs/sender-printer/checklist.md`
- Mark all phases ✅ in the checklist

Update `docs/sender-printer/checklist.md`:
- All steps `[x]`
- All phases `✅`

This completes the sender graph printer implementation.
