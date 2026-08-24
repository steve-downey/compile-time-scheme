# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

A compile-time compiler proof of concept targeting C++26 on GCC16. The live
front end, `smd::cl` (`src/smd/cl/`), the Common Lisp rebuild, follows a
staged pipeline:

```
source string -> reader datum tree -> elaborated core tree
  -> eval (small-step machine) -> Beman Execution sender backend
```

- `smd::cl` (`src/smd/cl/`) — the Common Lisp front end, and the active development tree.
- `smd::kit::foundation` (`src/smd/kit/foundation/`) — the language-agnostic substrate (`static_vector`, `result`, `arena_box`, `tree_arena`, `fix`) extracted in step R8, shared by `cl` and by `smd::forth` in the sibling `compile-time-forth` repository.
- `smd::fixpoint` (`src/smd/fixpoint/`) — standalone recursion-scheme playground.

The reader parses data but does not classify special forms. The elaborator recognizes the language's special operators. Evaluation is a small-step machine (`eval::machine`), not C++ recursion. The sender backend uses Beman Execution vendored as a git submodule at `vendor/execution`. Reflection has not yet been ported to this front end.

**The D1 freeze on `src/smd/smdscheme/**` was retired** by decision D11 (`docs/cl-rebuild-plan.md`), before the tree was deleted outright. Its rationale — that published blog phases transclude live code — was superseded when posts moved to `orgit-file:` links pinned to `blog/phase-NN` tags (`docs/blog/pins.md`). The UUID anchor rules have since been relaxed too, by decision D21: an anchor set belongs to the step that lands it, and the only surviving obligation is that it parses at that step's merge.

`AGENTS.md` § "Which trees you may edit" is authoritative: `src/smd/cl/**` is the live tree, and now the only one. Its two predecessors, `src/smd/smdscheme/**` (the original Scheme-light front end) and `src/smd/smdlisp/**` (the Common Lisp pivot and, from step R1 until decision D32, the rebuild's never-edited behavioural oracle), were deleted from trunk at step A3 of the tree-retirement plan (`tmp/plan/checklist.md`). Both are frozen at the annotated tags `iteration/smdscheme-final` and `iteration/smdlisp-final` and remain readable there; their architecture prose is pinned to those tags in `docs/history/architecture-iterations.org`.

Plans and status:

- `tmp/plan/` — **the active plan**: steps A0–A5 (retiring the two earlier iterations) and B1–B8 (parser combinators in `cl`'s reader). `tmp/plan/checklist.md` is its status; `tmp/plan/README.md` is the orchestrator's view. Agents read their own step file, not the whole directory.
- `docs/cl-parser-scoping.md` (D27–D31) and `docs/cl-language-scoping.md` (D22–D26) — the ratified reasoning the active plan executes. Read by numbered section, never wholesale.
- `checklist.md` — step-by-step status for the original steps, the Common Lisp pivot, and the rebuild.
- `docs/cl-rebuild-plan.md` — closed at R8 by `docs/cl-rebuild-closing-note.md`; its phase list is finished. Still authoritative for decision records D11–D21, which are live. Read by named section.
- `docs/cl-pivot-plan.md` — the pivot plan (decision records D1–D10). Still authoritative for how the current `smdlisp` got its shape; its step list is finished. Read by named section on demand, never wholesale.
- `docs/history/blog-backfill-plan.md` — the arrears: phases 23–27, for steps R0–R4, which landed without posts. Steps B5–B11.
- `docs/backlog/` — identified-but-unscheduled work, one file per item. Not a plan and not an agent read path; consult it only when deciding what to schedule next.
- `docs/history/schemepoc-plan.md` — the superseded pre-pivot plan. Historical only.
- `docs/divergences/` — one record per deliberate divergence from ANSI Common Lisp, the active plan, or a project rule. `docs/divergences/README.md` classifies each as `defect`, `scope-decision`, `artifact-of-D1`, `toolchain`, or `process`. These docs are append-only: append a dated note, never edit in place. **A test may pin a `scope-decision`; a test must never pin a `defect`** (D16).

`AGENTS.md` is authoritative on the agent reading contract and step protocol; `docs/codestyle.org` is authoritative on style. `docs/cpp-rules.md` is the terminal distillation of the C++ rules — read it last, immediately before writing code.

## Build commands

The top-level Makefile drives all workflow. Do not bypass it for normal work.

```bash
make                    # default: build and run all tests (Asan config)
make compile            # compile only
make test               # rebuild and run tests
make test-matrix        # rebuild and run tests in Debug (-O0) then Asan (-O3)
make ctest              # run CTest on current build without rebuilding
make lint               # run pre-commit linters (clang-format, gersemi, markdownlint, codespell, etc.)
make coverage           # build with Gcov profile and generate coverage report
make install            # install the project
make testinstall        # test the installed package
make compile-headers    # verify interface header sets compile independently
make help               # show all targets
```

Selecting a different compiler: `make TOOLCHAIN=gcc-17` or `make TOOLCHAIN=clang-23`. Default is `gcc-16`. Prefer the newest available toolchains (trunk is fine) over compromising on technique; toolchain files live in `etc/`.

Selecting a build config: `make CONFIG=RelWithDebInfo` (default is `Asan`). Available: `RelWithDebInfo`, `Debug`, `Tsan`, `Asan`, `Gcov`.

Note that `Asan` is `-O3 -g -fsanitize=address,undefined,leak` — the most optimized configuration routinely run, not a debug build. `Debug` is the `-O0` one. The two witness different defect classes and neither subsumes the other, so a step merges on `make test-matrix`, not `make test`. `docs/verification-matrix.md` is the reasoning and the evidence.

Tooling (cmake, ninja, pre-commit, clang-format, gcovr) is managed by `uv` and installed into a local `.venv`.

## Source layout

Merged `src/` layout (Pitchfork-style). Headers, implementations, and tests are co-located:

```
src/smd/<package>/<component>.hpp             (package header, e.g. smdlisp.hpp)
src/smd/<package>/<area>/<component>.hpp
src/smd/<package>/<area>/<component>.cpp
src/smd/<package>/<area>/<component>.test.cpp
src/examples/                                 (example programs)
```

`<package>` is `cl`, `kit`, or `fixpoint`. `<area>` is the pipeline stage — `foundation`, `symbol`, `reader`, `core`, `elaborator`, `eval`, `sender` — each with its own `CMakeLists.txt`.

Do not create split `include/`, `src/`, and `tests/` trees.

## C++ conventions

- **C++26 baseline, GCC16 baseline.** Do not add fallback paths for older standards or other compilers.
- **Namespace:** aligned with directory path and include spelling — `smd::smdlisp::reader` lives in `src/smd/smdlisp/reader/`. Sub-namespace per pipeline area.
- **Includes:** canonical angle-bracket spelling only (`#include <smd/smdlisp/reader/atom.hpp>`). No relative includes. No transitive include reliance.
- **Header guards:** classical `#ifndef`/`#define` guards, not `#pragma once`. Guard names are the uppercased repo-relative path (e.g., `SRC_SMD_SMDLISP_READER_ATOM_HPP`); `src/smd/fixpoint/` predates that convention and uses `INCLUDED_SMD_FIXPOINT_*`.
- **File prolog:** every source file starts with a canonical repo-relative path comment with Emacs mode line, then SPDX on the next line:
  ```cpp
  // src/smd/smdlisp/reader/atom.hpp                               -*-C++-*-
  // SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
  ```
- **Formatting:** clang-format is enforced (config in `.clang-format`). gersemi formats CMake files. Do not fight formatter output.
- Prefer `constexpr` for APIs that can be meaningfully constant-evaluated.
- No `using namespace` in headers.
- Use `snake_case` for file names. Prefer `.hpp`, `.cpp`, `.test.cpp`.

## Testing

- Tests use **Catch2**. Do not introduce GTest.
- Test files include the component header first, then include it again to verify idempotency.
- Co-locate tests with source under `src/`.
- Test skeleton:
  ```cpp
  // src/smd/smdlisp/reader/atom.test.cpp                           -*-C++-*-
  // SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

  #include <smd/smdlisp/reader/atom.hpp>
  #include <smd/smdlisp/reader/atom.hpp>  // test 2nd include OK

  #include <catch2/catch_test_macros.hpp>

  TEST_CASE("AtomTest - HeaderIsIdempotent") { REQUIRE(true); }
  ```

## CMake conventions

- Target-based CMake (cmake_minimum_required 4.0). Define a target first, attach sources with `target_sources`. Use `FILE_SET HEADERS` for public headers.
- Each `CMakeLists.txt` lists only files in its own directory. Descend with `add_subdirectory`. Do not reach upward or sideways.
- Catch2 is fetched via `infra/cmake/use-fetch-content.cmake` (set via `CMAKE_PROJECT_TOP_LEVEL_INCLUDES`).
- Beman Execution: vendored as git submodule at `vendor/execution`, integrated with `add_subdirectory`. Do not use FetchContent, vcpkg, or install-time discovery for Beman dependencies.

## UUID anchors and org-transclusion

Source files contain UUID-delimited blocks (e.g., `// 44cc988c-7353-43aa-a7d3-8840f92371a6`) used by org-transclusion to embed live code in `schemepoc.org` and in the blog posts under `docs/blog/`. New anchors must be real `uuidgen` output.

An anchor set belongs to the step that lands it (decision D21). A step may delete, move, split, merge or nest any existing pair in a file it owns; there is no duty of care toward an anchor because it already exists. Blog posts resolve transclusions against their own `blog/phase-NN` tag (`docs/blog/pins.md`), so no edit here can reach published prose, and a later post wanting a region nobody anchored places its own anchors. The one surviving obligation is empirical: the anchor set must parse at the step's merge, meaning `scripts/verify-transclusions.sh` is green there. See `AGENTS.md` for the full statement; do not restate it in stronger form.

## Infrastructure

The `infra/` directory is vendored from the [Beman Project infra](https://github.com/bemanproject/infra) via git subtree. Toolchain files are in `etc/`. The `.pre-commit-config.yaml` drives linting (clang-format, gersemi, markdownlint, codespell, checkmake, shellcheck, gitleaks).
