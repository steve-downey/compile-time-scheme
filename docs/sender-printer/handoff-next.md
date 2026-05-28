# Sender Graph Printer — Handoff Notes

This file is rewritten by each step's agent with notes for the next step.

## Current State

Step 5 is complete. All 147 tests pass (up from 140 — 7 new StringWriterTest tests).
Linters pass.

The branch `worktree-sender-printer-step-05` contains Step 5 work built on top of Step 4.

## What Was Done in Step 5

- Created `src/smd/smdscheme/sender/string_writer.hpp` — `string_writer<T>` with `pure()`,
  `tell()`, `combine()`, `lift_a2()`, `fmap()`. Runtime-only (uses `std::string`).
- Created `src/smd/smdscheme/sender/string_writer.test.cpp` — 7 Catch2 tests covering all
  operations including `CombineMultiple` (chained combine).
- Updated `src/smd/smdscheme/sender/CMakeLists.txt` to add `string_writer.hpp` to FILE_SET.
- `std::monostate` is included via `<variant>` in `string_writer.hpp`. No issues.
- Fixed Phase 1 checklist status from ⬜ to ✅ (steps 1-2 were already complete).

## Notes for Step 6

Begin with `step-06.md`. Step 6 creates `format_scheme_node.hpp` — the DOT formatter that
takes a `scheme_node_data` and returns a `string_writer<std::monostate>`.

Key facts for Step 6:

- `string_writer<T>` is in `src/smd/smdscheme/sender/string_writer.hpp`, namespace
  `smd::smdscheme::sender`. Include it with angle brackets.
- `scheme_node_data` is in `src/smd/smdscheme/sender/scheme_node_data.hpp`. Its
  `child_ids` field is a `foundation::static_vector<int, 8>` — use `.size()` and `[]`.
  The `sender_algo` and `scheme_context` fields are `std::string_view`.
- `std::format` is available in C++26 on GCC16 — no issues expected.
- The DOT output uses `tell(dot_string)` to produce `string_writer<std::monostate>`.
  The `log` field accumulates the DOT fragment; `value` is always `std::monostate{}`.
- `format_scheme_node` should be a plain `inline` function (not constexpr — it uses
  `std::string` and `std::format` which are runtime-only).

### CMakeLists.txt current FILE_SET (after step 5)

```cmake
FILES
    sender_program.hpp
    sender_v.hpp
    sender_cps_program.hpp
    scheme_node_data.hpp
    scheme_tree.hpp
    build_scheme_tree.hpp
    string_writer.hpp
```

Add `format_scheme_node.hpp` (gersemi formats one-per-line; match existing style).

### Critical GCC16 reflection constraints (carried forward from Step 4)

Step 6 does NOT use reflection. These constraints are relevant for step 7 wiring.

- `[:r:]` splice DOES NOT WORK on consteval function parameters.
- `template_arguments_of` works on `product_type` base, not directly on `basic_sender`.
- `bases_of` requires explicit `access_context::unchecked()`.
- The working `classify_sender` uses `display_string_of` + position-based min-find on
  `"just_t"`, `"then_t"`, `"when_all_t"` to identify the outermost sender tag.
