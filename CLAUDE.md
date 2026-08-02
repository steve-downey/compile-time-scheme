# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

A compile-time compiler proof of concept targeting C++26 on GCC16, with two front ends over a shared foundation. Both follow the same staged pipeline:

```
source string -> reader datum tree -> elaborated core tree
  -> CPS / defunctionalized program -> closure backend
  -> Beman Execution sender backend -> reflection reification spike
```

- `smd::smdscheme` (`src/smd/smdscheme/`) — the original Scheme-light front end, plus the language-agnostic `foundation/` (`static_vector`, `result`, `arena_box`, `tree_arena`, `fix`, parser combinators) that both front ends build on.
- `smd::smdlisp` (`src/smd/smdlisp/`) — the Common Lisp front end, and the active development tree.
- `smd::fixpoint` (`src/smd/fixpoint/`) — standalone recursion-scheme playground.

The reader parses data but does not classify special forms. The elaborator recognizes the language's special operators. CPS is the semantic center. The closure backend is the stable demo path. The sender backend uses Beman Execution vendored as a git submodule at `vendor/execution`. Reflection is isolated until explicitly integrated.

**The D1 freeze on `src/smd/smdscheme/**` is retired** by decision D11 (`docs/cl-rebuild-plan.md`). Its rationale — that published blog phases transclude live code — was superseded when posts moved to `orgit-file:` links pinned to `blog/phase-NN` tags (`docs/blog/pins.md`). The UUID anchor rules are unchanged: do not delete or nest an existing anchor pair. `src/smd/smdlisp/**` is frozen from step R1 onward for a different reason — it is the behavioural oracle for the rebuild — and is never edited.

Plans and status:

- `checklist.md` — step-by-step status for the original steps, the Common Lisp pivot, and the rebuild.
- `docs/cl-rebuild-plan.md` — the active plan, its decision records (D11–D17), and the phase list R0–R8.
- `docs/cl-pivot-plan.md` — the pivot plan (decision records D1–D10). Still authoritative for how the current `smdlisp` got its shape; its step list is finished. Read by named section on demand, never wholesale.
- `docs/blog-backfill-plan.md` — the arrears: phases 23–27, for steps R0–R4, which landed without posts. Steps B5–B11.
- `docs/backlog/` — identified-but-unscheduled work, one file per item. Not a plan and not an agent read path; consult it only when deciding what to schedule next.
- `docs/schemepoc-plan.md` — the superseded pre-pivot plan. Historical only.
- `docs/divergences/` — one record per deliberate divergence from ANSI Common Lisp, the active plan, or a project rule. `docs/divergences/README.md` classifies each as `defect`, `scope-decision`, `artifact-of-D1`, `toolchain`, or `process`. These docs are append-only: append a dated note, never edit in place. **A test may pin a `scope-decision`; a test must never pin a `defect`** (D16).

`AGENTS.md` is authoritative on the agent reading contract and step protocol; `docs/codestyle.org` is authoritative on style. `docs/cpp-rules.md` is the terminal distillation of the C++ rules — read it last, immediately before writing code.

## Build commands

The top-level Makefile drives all workflow. Do not bypass it for normal work.

```bash
make                    # default: build and run all tests (Asan config)
make compile            # compile only
make test               # rebuild and run tests
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

`<package>` is `smdscheme`, `smdlisp`, or `fixpoint`. `<area>` is the pipeline stage — `foundation`, `parser`, `reader`, `elaborator`, `closure`, `macroexpand`, `sender`, `reflection` — each with its own `CMakeLists.txt`.

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

Source files contain UUID-delimited blocks (e.g., `// 44cc988c-7353-43aa-a7d3-8840f92371a6`) used by org-transclusion to embed live code in `schemepoc.org` and in the blog posts under `docs/blog/`. Do not delete or nest existing UUID anchor pairs; new anchors must be real `uuidgen` output.

Editing *between* the markers is allowed — the prohibition is only on deleting or nesting the pairs themselves. Blog posts resolve transclusions against their own `blog/phase-NN` tag (`docs/blog/pins.md`), so an anchored region can be changed later. The binding constraint on `src/smd/smdscheme/**` is the D1 frozen-tree rule above, not the anchors. See `AGENTS.md` for the full statement; do not restate either rule in stronger form.

## Infrastructure

The `infra/` directory is vendored from the [Beman Project infra](https://github.com/bemanproject/infra) via git subtree. Toolchain files are in `etc/`. The `.pre-commit-config.yaml` drives linting (clang-format, gersemi, markdownlint, codespell, checkmake, shellcheck, gitleaks).
