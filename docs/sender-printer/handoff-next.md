# Sender Graph Printer — Handoff Notes

This file is rewritten by each step's agent with notes for the next step.

## Current State

Step 6 is complete. All 151 tests pass (up from 147 — 4 new FormatSchemeNodeTest tests:
HeaderIsIdempotent, LeafNode, NodeWithChildren, ValidDotSyntax). Linters pass.

The branch `worktree-sender-printer-step-06` contains Step 6 work built on top of Step 5.

## What Was Done in Step 6

- Created `src/smd/smdscheme/sender/format_scheme_node.hpp` — `format_scheme_node`
  inline function that takes `scheme_node_data const&` and returns
  `string_writer<std::monostate>` with DOT node definition and edge lines.
- Created `src/smd/smdscheme/sender/format_scheme_node.test.cpp` — 4 Catch2 tests.
- Updated `src/smd/smdscheme/sender/CMakeLists.txt` to add `format_scheme_node.hpp`
  to FILE_SET.
- clang-format also fixed reference-spacing in `string_writer.hpp` (`const&` →
  `const &`) and alignment in `string_writer.test.cpp`.

## Confirmed: std::format works in GCC16 C++26

No issues. `std::format` with `string_view` arguments (e.g. `node.sender_algo`,
`node.scheme_context`) works correctly.

## Sample DOT Output

For a leaf `just` node (id=0, scheme_context empty string):
```
  n0 [label="just\n[]"];
```

For a `then` node (id=3, scheme_context="Apply: +") with one child (id=4):
```
  n3 [label="then\n[Apply: +]"];
  n3 -> n4;
```

For a `when_all` node (id=1) with two children (id=2, id=3):
```
  n1 [label="when_all\n[]"];
  n1 -> n2;
  n1 -> n3;
```

Note: `scheme_context` is empty string by default since `build_scheme_tree` does not
currently populate it. Labels appear as `"algo\n[]"`.

## Notes for Step 7

Begin with `step-07.md`. Step 7 creates `dump_scheme_plan.hpp` — the top-level API
that wires `build_scheme_tree` + `format_scheme_node` + `format_tree` together.

### Key Architecture Facts

- `scheme_tree<MaxNodes>.nodes` is `foundation::static_vector<scheme_node_data, MaxNodes>`.
  Its `.size()` returns `int`. Range-based for loop works (begin/end iterators provided).
- `scheme_tree<MaxNodes>.get(id)` returns `scheme_node_data const&` indexed by `node_id`,
  which equals the allocation order. Node IDs are always contiguous 0..N-1.
- `build_scheme_tree<MaxNodes>(^^Sender, t)` must be called in a `constexpr` lambda.
  The `^^Sender` splice WORKS here because `Sender` is a template type parameter (NOT a
  consteval function parameter). The GCC16 splice restriction only applies to consteval
  function parameters.

### CMakeLists.txt current FILE_SET (after step 6)

```cmake
FILES
    sender_program.hpp
    sender_v.hpp
    sender_cps_program.hpp
    scheme_node_data.hpp
    scheme_tree.hpp
    build_scheme_tree.hpp
    string_writer.hpp
    format_scheme_node.hpp
```

Add `dump_scheme_plan.hpp` (gersemi formats one-per-line; match existing style).

### Iteration in format_tree

Step-07.md uses an index loop. A range-based for loop also works since `static_vector`
has begin/end:
```cpp
for (auto const& node : tree.nodes) {
    dot += format_scheme_node(node).log;
}
```

### Critical GCC16 reflection constraints (carried forward from Step 4)

Step 7 uses reflection (build_scheme_tree uses `<meta>`). These constraints apply:

- `[:r:]` splice DOES NOT WORK on consteval function parameters.
- `^^Sender` splice WORKS on template type parameters (step 7 uses this form).
- `template_arguments_of` works on `product_type` base, not directly on `basic_sender`.
- `bases_of` requires explicit `access_context::unchecked()`.
- The working `classify_sender` uses `display_string_of` + position-based min-find on
  `"just_t"`, `"then_t"`, `"when_all_t"` to identify the outermost sender tag.
