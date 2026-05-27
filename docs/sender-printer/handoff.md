# Sender Graph Printer — General Handoff

Read this file at the start of every step. It does not change between steps.

## Project

SchemePoC (`smd/smdscheme`) is a compile-time Scheme-light compiler proof-of-concept targeting C++26 on GCC16. The sender-CPS backend (`sender/sender_cps_program.hpp`) compiles Scheme expressions into Beman Execution sender pipelines using `just`, `then`, and `when_all`.

## Goal

Implement a DOT-format graph printer that visualizes the sender execution plan for compiled Scheme expressions. The architectural blueprint is in `docs/sender_graphing.md`.

The approach uses C++26 reflection (`<meta>`) to introspect sender types, projects them into a runtime tree, and traverses that tree to produce Graphviz DOT output.

## Build & Test

```bash
make compile   # build everything
make test      # rebuild + run all tests
make ctest     # run tests without rebuild
```

All steps MUST keep `make compile && make test` green.

## Conventions

- **C++26 baseline, GCC16 baseline.**
- **Namespace:** `smd::smdscheme::sender` for new sender-printer files.
- **Includes:** canonical angle-bracket only: `#include <smd/smdscheme/sender/scheme_node_data.hpp>`
- **Header guards:** `#ifndef`/`#define`, NOT `#pragma once`. Guard = path uppercased: `SRC_SMD_SMDSCHEME_SENDER_SCHEME_NODE_DATA_HPP`
- **File prolog:** every source file starts with repo-relative path + Emacs mode line, then SPDX:
  ```cpp
  // src/smd/smdscheme/sender/scheme_node_data.hpp               -*-C++-*-
  // SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
  ```
- **Naming rule:** every `xyzzy.hpp` must define something named `xyzzy`.
- **Tests:** Catch2 (NOT GTest). Test files include the component header first, then again for idempotency.
- **CMake:** add new headers to `sender/CMakeLists.txt` FILE_SET. Tests are auto-discovered via `GLOB_RECURSE` from the parent.
- **No `using namespace` in headers.**
- **Prefer `constexpr` where practical.** DOT output is runtime-only (uses `std::string`), but data types should be constexpr-friendly.
- **Include what you use.** No transitive include reliance.

## Key Existing Files

| File | Contains |
|------|----------|
| `sender/sender_v.hpp` | `just`, `then`, `when_all`, `sync_wait`, `task<T>` |
| `sender/sender_cps_program.hpp` | `sender_cps_program`, `eval_node`, `compile_sender_cps` |
| `sender/sender_program.hpp` | `sender_program`, `compile_to_sender` (coroutine-based) |
| `foundation/static_vector.hpp` | `static_vector<T, Capacity>` — constexpr-friendly vector |
| `foundation/arena_box.hpp` | `arena_box<T, N>`, `tree_arena<T, N>` — constexpr arena allocation |
| `foundation/fix.hpp` | `fix<F>`, `fold_fix` — recursive type combinator |
| `foundation/functor.hpp` | `functor<Impl>` CRTP, `fmap` CPO |
| `reflection/reified_environment.hpp` | Existing C++26 reflection spike using `<meta>` |
| `elaborator/elaborated_core.hpp` | `core_type<N,M>`, `core_if`, `core_lambda`, `core_application` etc. |
| `closure/value.hpp` | `value<Core>`, `env<Core, N>`, `closure<Core>` |

## Step Workflow

1. Read this file (`handoff.md`)
2. Read `handoff-next.md` (mutable notes from previous step)
3. Read your step file (`step-NN.md`)
4. Execute: create/modify files as instructed
5. Verify: `make compile && make test`
6. Update `handoff-next.md` with anything the next agent needs to know
7. Update `checklist.md` marking your step complete
