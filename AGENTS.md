# AGENTS.md

Rules for automated coding agents working in this repository.

## Reading contract (three tiers — nothing an agent reads grows per step)

A cleared worker agent reads a bounded set. **Never read a file that accumulates
across steps.** The failure mode this exists to prevent is a cumulative handoff or a
full plan re-read on every turn.

1. **Tier 1 — always (rules pack, stable, cached):**
   `docs/codestyle.org`, `AGENTS.md`, `docs/CODING_RULES.md`, `CLAUDE.md` —
   and read `docs/cpp-rules.md` **last**, immediately before writing code.
   It is the terminal distillation of the C++ rules for execution agents; step
   briefs reference it as the final item of their reading list.
2. **Tier 2 — this step only:** your step brief + `checklist.md`. Steps run in
   parallel lanes, so each lane has its own brief (`step-brief-<lane>.md`, e.g.
   `step-brief-l13.md`); read only your lane's, never the others'.
3. **Tier 3 — on demand, by anchor/named section only, never wholesale:**
   `docs/compiler_architecture.org` (durable cross-step facts, by UUID anchor);
   `docs/cl-pivot-plan.md` (the active-plan dependency DAG — the orchestrator pastes
   the one step section a worker needs; workers do not open the full plan;
   `docs/schemepoc-plan.md` is the superseded pre-pivot plan, historical only);
   `git log` / `docs/history/handoff-archive.md` (retired cumulative log — archival,
   not a read path).

`docs/codestyle.org` is authoritative.
If any rule conflicts with `docs/codestyle.org`, follow `docs/codestyle.org`.

## The step-brief contract

A step brief (`step-brief-<lane>.md`) is the single per-step handoff for its lane. It
is **forward-only, rewritten fresh each step (never appended), consumed once, and
bounded (≤ ~150 lines).** It contains only: the next step's goal and merge criterion;
the exact files that step will touch; dependencies already satisfied, **referenced by
anchor** into `docs/compiler_architecture.org` (not re-narrated); and what this step
discovered that the next one needs and could not get from its own brief (a deviation,
surprise, or gotcha). It is **not** a summary of work done (that's the commit message
+ checklist), **not** architecture (that's `compiler_architecture.org`, updated in
place), and **not** history (that's `git log`). If it reads like a log, it has the
wrong contents.

## Current task protocol

- Work only the next unchecked step in `checklist.md`.
- Read your lane's step brief before editing (your Tier-2 handoff for this step).
- Keep the step small and mergeable.
- A step's merge criterion is `make test-matrix`, not `make test`. The default
  `Asan` config is `-O3`; `Debug` is the `-O0` one, and each witnesses defects the
  other cannot. Both have already cost this project a defect — see
  `docs/verification-matrix.md`. Report which legs you ran.
- Do not continue into later steps unless explicitly instructed.
- Do not leave vague TODOs.
- Document blockers in your lane's step brief.
- Update `checklist.md` when the step is complete.
- Record durable cross-step facts in `docs/compiler_architecture.org` in place (by
  UUID anchor), keeping its transcluded code annotations current — not in any growing
  handoff or log.
- Land the UUID anchors your step's blog post will transclude, around the regions
  that are the step's centre, and name them in your step brief. You do not write
  the post — see "The blog deliverable" below — but the anchors must exist at your
  merge commit, because that is what the post pins to.
- Rewrite your lane's step brief for the next clean agent, per the step-brief contract
  above.

## UUID anchors and the frozen Scheme tree

Source files carry UUID-delimited blocks used by org-transclusion. Two different
rules apply to them, and they are routinely conflated.

- Inside `src/smd/smdlisp/**`, you **may** edit the contents of an anchored region.
  Every blog post resolves its transclusions against its own `blog/phase-NN` tag
  (`docs/blog/pins.md`), so a later step may move, rename, reformat, or delete an
  anchored region without owing anything to already-published prose.
- An anchor set belongs to the step that lands it (decision D21). You may delete,
  move, split, merge or nest any existing pair in a file you own, whenever that
  serves the code. There is no duty of care toward an anchor just because it is
  already there, and none toward already-published prose, which resolves against
  its own tag.
- The whole of the anchor rule is now empirical: the set must **parse at your
  merge commit**, meaning `scripts/verify-transclusions.sh` is green there.
  Nesting is not checked and is not an error; it only puts the inner markers
  inside the outer region's transcluded text, which is the post drafter's
  legibility problem, not a violation.
- New anchors must be real `uuidgen` output, and must exist at the step's merge
  commit — a post pinned to that tag can only transclude anchors present there.
  A later post wanting a region nobody anchored places its own anchors.
- `src/smd/smdscheme/**` is frozen for semantic changes (decision D1), anchored or
  not. Read it, copy from it, link it; do not edit it. A frozen-tree edit needs a
  divergence doc and orchestrator sign-off.

Do not restate this rule in a step brief in stronger form than it is written here.
An over-broad "never edit inside a UUID anchor block" has twice caused a worker to
design around a constraint that does not exist.

## The blog deliverable

Every step ships a post, and three agents touch it, none reading another's
conversation (decision D20, `docs/cl-rebuild-plan.md` §2 and §7).

1. **The implementing agent** lands the code and its UUID anchors, and stops.
   It does not draft the post.
2. **A fresh drafting agent** reads the merged work — the step's diff, its commit
   message, its retired step brief — plus `docs/blog/` for house format, the
   preceding post, and the `voice` skill. It never reads the implementing agent's
   conversation, and there is no handoff between them.
3. **A second clean agent** reviews the draft with the `voice` skill, seeing only
   the draft and the diff. This is the rule already in force for commit and PR
   messages; longer prose is not where to relax it.

The distance in step 2 is a property of the genre, not a cost saving. A post is a
real-time, non-omniscient diary entry (`docs/cl-pivot-plan.md`); an agent that just
spent a step's context on the work knows every path it did not take and writes
omnisciently about a result that was not obvious at the time.

Between steps 1 and 2 the orchestrator tags the `--no-ff` merge `blog/phase-NN`
and pushes it, because the drafting agent writes `orgit-file:` links naming that
tag. The step is complete when the post lands, not when the code merges.

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
- Commit messages — on work branches and on main — and PR messages must be
  reviewed by a clean agent running the `voice` skill before use. The reviewing
  agent sees only the draft message and the diff, not the authoring
  conversation.
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
durable cross-step facts recorded in docs/compiler_architecture.org (in place, by anchor)
UUID anchors landed for the regions this step's post will transclude, and named in the brief
step brief rewritten per the step-brief contract
no unrelated files changed
no unexplained generated files changed
template/ counterpart updated when required
```
