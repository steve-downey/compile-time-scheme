# AGENTS.md

Rules for automated coding agents working in this repository.

Read these files first, in order:

1. `docs/codestyle.org`
2. `AGENTS.md`
3. `docs/CODING_RULES.md`
4. `CLAUDE.md`
5. `handoff.md`
6. `handoff-next.md`
7. `checklist.md`

`docs/codestyle.org` is authoritative.
If any rule conflicts with `docs/codestyle.org`, follow `docs/codestyle.org`.

## Current task protocol

- Work only the next unchecked step in `checklist.md`.
- Read `handoff.md` before editing.
- Read `handoff-next.md` before editing.
- Keep the step small and mergeable.
- Do not continue into later steps unless explicitly instructed.
- Do not leave vague TODOs.
- Document blockers in `handoff-next.md`.
- Update `checklist.md` when the step is complete.
- Update `handoff.md` with durable facts.
- Rewrite `handoff-next.md` for the next clean agent.

## Required commands

Before declaring the step complete, run:

```sh
make compile
make test
make lint
```

Use these as needed:

```sh
make ctest
make coverage
make install
make testinstall
make presentation
make help
```

Do not claim success unless required commands pass.

## Worktree and commit policy

- For non-trivial work, create a git worktree.
- Use a short descriptive kebab-case branch name.
- Work only inside that worktree.
- Do not edit the main working tree while the worktree is active.
- Commit only when tests pass.
- Do not commit partial work unless explicitly instructed.
- Merge to main with `--no-ff`.
- Do not push unless explicitly instructed.

Suggested worktree command:

```sh
git worktree add ../<branch-name> -b <branch-name>
```

## Copier/template rule

This repository may also be a Copier template.

- When changing a root file that has a `template/` counterpart, change both.
- Keep generated default template output equivalent to the repo root.
- Preserve Copier delimiters:
  - variables: `[[[ var ]]]`
  - blocks: `[% %]`
  - comments: `[# #]`
- Template files use `.jinja`.

## Baseline language and compiler

- C++26 is the baseline.
- GCC16 is the baseline compiler.
- Do not add fallback paths for older C++ standards.
- Do not add fallback paths for other compilers unless explicitly requested.
- Prefer direct C++26 code over compatibility scaffolding.
- If a C++26 standard-library facility is unavailable in GCC16, use the project-designated dependency.

## Sender dependencies

- Use Beman Execution for sender/receiver support.
- Vendor Beman Execution as a git submodule at `vendor/execution`.
- Use Beman Task only if task/coroutine support is required.
- Vendor Beman Task as a git submodule at `vendor/task`.
- Integrate vendored dependencies with `add_subdirectory`.
- Do not use FetchContent for Beman Execution or Beman Task.
- Do not use vcpkg for Beman Execution or Beman Task.
- Do not use install-time discovery for Beman Execution or Beman Task.
- Do not vendor these dependencies by git subtree.
- Keep a project sender adapter if it simplifies compiler backend code.
- Do not treat the adapter as a portability layer for older standards or compilers.

## Layout

Use merged `src/` layout for new code.

Place each logical component under its namespace path:

```txt
src/smd/schemepoc/<component>.hpp
src/smd/schemepoc/<component>.cpp
src/smd/schemepoc/<component>.test.cpp
```

Use `detail/` only for non-public textual implementation support:

```txt
src/smd/schemepoc/detail/<helper>.hpp
```

Do not create split `include/`, `src/`, and `tests/` trees for new work.

## File names

- Use `snake_case`.
- Prefer `.hpp`, `.cpp`, and `.test.cpp`.
- Use `.h` or `.t.cpp` only when matching an existing subtree.

## File prolog

Every source-like file starts with a canonical repo-relative path comment and Emacs mode line.

Then put SPDX on the next comment-capable line.

C++ example:

```cpp
// src/smd/schemepoc/reader.hpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
```

CMake example:

```cmake
# src/smd/schemepoc/CMakeLists.txt                              -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
```

## Include rules

- Include project headers only by canonical include spelling.
- Use angle brackets.
- Do not use relative includes.
- Do not include by leaf name.
- Do not rely on transitive includes.
- Public headers must be self-contained.

Good:

```cpp
#include <smd/schemepoc/reader.hpp>
```

Bad:

```cpp
#include "reader.hpp"
#include "../schemepoc/reader.hpp"
#include "../../src/smd/schemepoc/reader.hpp"
```

## Namespace rules

- Namespace, directory path, and include spelling must align.
- Use `smd::schemepoc` for this project.
- Multiple namespace levels are allowed.
- Namespace levels should correspond to directories.
- Do not write `using namespace` in headers.

Example:

```txt
Path:      src/smd/schemepoc/reader.hpp
Namespace: smd::schemepoc
Include:   <smd/schemepoc/reader.hpp>
```

## Header guards

Use classical include guards.

Do not use `#pragma once`.

Guard names use the full repo-relative path pattern.

Example:

```cpp
#ifndef SRC_SMD_SCHEMEPOC_READER_HPP
#define SRC_SMD_SCHEMEPOC_READER_HPP

#endif
```

If the local subtree already uses an `INCLUDED_...` guard convention, follow the local convention.

## C++ structure

- Declare functions before defining them.
- Keep regular member and free-function definitions out of class bodies when practical.
- Define ordinary functions out of line.
- Fully qualify out-of-line definitions.
- Use hidden friends only for short customization points.
- Mark hidden friends clearly.
- Keep public declarations complete and truthful.
- Do not omit concepts, constraints, or `requires` clauses from public declarations.
- Prefer `constexpr` for APIs that can be meaningfully constant-evaluated.
- Add compile-time tests for constexpr contracts.
- Use C++26 directly.
- Use C++26 facilities when GCC16 supports them.

## Formatting and linting

- Let `clang-format` format C++.
- Let `gersemi` format CMake.
- Treat pre-commit hooks as binding.
- Do not fight formatter output.
- Do not hand-align code in ways the formatter will undo.
- Run `make lint`.

## CMake rules

- Use target-based CMake.
- Define a target first.
- Attach sources with `target_sources`.
- Use `FILE_SET HEADERS` for public headers.
- Keep `CMAKE_VERIFY_INTERFACE_HEADER_SETS` enabled where practical.
- Each `CMakeLists.txt` lists only files in its own directory.
- Descend with `add_subdirectory`.
- Do not reach upward with `..`.
- Do not reach sideways into peer directories.
- Use concrete library targets for compiled code.
- Use `INTERFACE` libraries only for truly header-only code.
- Export and install project targets, not loose files.
- Avoid single-use variables for file lists.
- Vendor Beman Execution under `vendor/execution` as a git submodule.
- Vendor Beman Task under `vendor/task` as a git submodule only if needed.
- Integrate vendored Beman dependencies with `add_subdirectory`.
- Do not use FetchContent for Beman dependencies.
- Do not use vcpkg for Beman dependencies.
- Do not use `find_package` for vendored Beman dependencies.

## Tests

- Tests use Catch2.
- Do not mention or introduce GTest.
- Put tests next to the component under `src/`.
- Use `.test.cpp` for new tests.
- Include the component header first.
- Include the component header twice.
- Add a trivial bootstrap test for new components.
- Add substantive tests incrementally.
- Add compile-time tests for constexpr APIs.
- Add negative compile tests for constraints when appropriate.
- Match specific diagnostics in negative compile tests.
- Add law-focused tests before performance tests for typeclass-style code.
- Keep optional Applicative test support separate from monoid test support when possible.

Test skeleton:

```cpp
// src/smd/schemepoc/reader.test.cpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/reader.hpp>
#include <smd/schemepoc/reader.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ReaderTest - HeaderIsIdempotent")
{
    REQUIRE(true);
}
```

## Before handoff

Verify:

```txt
make compile passed
make test passed
make lint passed
checklist.md updated
handoff.md updated
handoff-next.md rewritten
no unrelated files changed
no unexplained generated files changed
template/ counterpart updated when required
```
