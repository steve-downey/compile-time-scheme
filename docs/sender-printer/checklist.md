# Sender Graph Printer — Phase Checklist

## Phase Index

| Phase | Description | Steps | Status |
|-------|-------------|-------|--------|
| 1 | Runtime Topology | Step 1–2 | ✅ |
| 2 | Reflection Bridge | Step 3–4 | ✅ |
| 3 | Applicative Traverser | Step 5–6 | ✅ |
| 4 | Integration | Step 7–8 | ✅ |

## Step Checklist

- [x] **Step 1:** Create `scheme_node_data.hpp` + test — topology node struct
- [x] **Step 2:** Create `scheme_tree.hpp` + test — tree type for holding the topology
- [x] **Step 3:** Create `build_scheme_tree.hpp` + test — reflection bridge for `just` senders
- [x] **Step 4:** Extend `build_scheme_tree` for `then` and `when_all` + tests
- [x] **Step 5:** Create `string_writer.hpp` + test — Writer applicative for string accumulation
- [x] **Step 6:** Create `format_scheme_node.hpp` + test — DOT formatter
- [x] **Step 7:** Create `dump_scheme_plan.hpp` + test — top-level API wiring
- [x] **Step 8:** Create example program + update docs

## Marking Complete

When you finish a step, change `[ ]` to `[x]` for that step.
When all steps in a phase are done, change that phase's status from `⬜` to `✅`.
