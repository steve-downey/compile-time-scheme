# SchemePoC Agent Execution Plan — SUPERSEDED (historical)

> **Superseded by the Common Lisp pivot (`docs/cl-pivot-plan.md`).** This is the
> original Scheme-PoC plan, retained for history. It is **not** an agent read path and
> its internal instructions predate the current governance: they reference the retired
> cumulative `handoff.md`/`handoff-next.md` model, which has been replaced by the
> three-tier reading contract in `AGENTS.md` (rules pack + per-lane
> `step-brief-<lane>.md` + on-demand `docs/compiler_architecture.org`). Read
> `AGENTS.md` and `docs/cl-pivot-plan.md` for current rules; treat everything below as
> a record of how the PoC was built, not as instructions to follow.

This is the operational plan for building `smd/schemepoc`, a Scheme-light compile-time compiler proof of concept in C++26 on GCC16.

The project starts from a Copier-rendered baseline based on `github.com/steve-downey/example`.

The implementation target is intentionally staged:

```txt
source string
  -> reader datum tree
  -> elaborated core tree
  -> CPS / defunctionalized program
  -> closure backend
  -> Beman Execution sender backend
  -> reflection reification spike
```

The reader parses Scheme data.
The elaborator recognizes language semantics.
CPS is the semantic center.
The closure backend is the stable demo path.
The sender backend uses Beman Execution.
Reflection is isolated until explicitly integrated.

---

# 0. Rule and style precedence

Agents must read these files first, in order:

```txt
docs/codestyle.org
AGENTS.md
docs/CODING_RULES.md
CLAUDE.md
handoff.md
handoff-next.md
checklist.md
```

Rule precedence:

```txt
docs/codestyle.org
  > AGENTS.md
  > docs/CODING_RULES.md
  > CLAUDE.md
  > handoff.md / handoff-next.md / checklist.md
```

`docs/codestyle.org` is authoritative.

If anything conflicts with `docs/codestyle.org`, follow `docs/codestyle.org`.

C++26 and GCC16 are the baseline even if older inherited rules mention C++23 or optional compiler support.

Tests use Catch2.

Do not mention or introduce GTest.

---

# 1. Repository files to create immediately

Create these repository-control files before implementation work begins:

```txt
AGENTS.md
docs/codestyle.org
docs/CODING_RULES.md
handoff.md
handoff-next.md
checklist.md
```

Copy `codestyle.org` verbatim into:

```txt
docs/codestyle.org
```

Copy the uploaded `CODING_RULES.md` into:

```txt
docs/CODING_RULES.md
```

Create the concise `AGENTS.md` below.

---

# 2. `AGENTS.md`

```markdown
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
- Keep `docs/compiler_architecture.org` and its transcluded UUID code annotations up to date.
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

## Typeclass rules

- Typeclass interfaces are concept-map-like records of named operations.
- Lookup is via variable-template-selected typeclass objects.
- Generic algorithms may accept explicit or NTTP-pinned instance objects.
- Keep datatypes and typeclass adaptations separate.
- Do not put Foldable, Applicative, or Traversable specialization logic into core data-structure headers unless that header is the designated adapter.

## Foldable rules

- `fold_map` is the semantic center.
- Derive `length`, `to_vector`, `fold_left`, and `fold_right` where practical.
- Document tree traversal order.
- Treat traversal order as part of the instance contract.

## Applicative rules

- Public surface is `invoke(f, ax, ay, ...)`.
- Implementor primitives may be `pure` and `apply`.
- Tree Applicative semantics must be explicit.
- Default tree Applicative semantics are monad-derived.
- Zip semantics are alternate tree Applicative semantics.
- Range Applicative models nondeterminism.
- ZipList Applicative models positional parallel behavior.
- Do not silently replace default semantics with flattening, duplication, expansion, reordering, or zipping behavior.

## Traversable rules

- `traverse` is the minimal operation.
- Traversal preserves shape.
- Traversal uses documented Foldable order unless explicitly documented otherwise.
- Effect order is observable and part of the contract.

## SchemePoC architecture rules

Keep the pipeline staged:

```txt
source string
  -> reader datum tree
  -> elaborated core tree
  -> CPS / defunctionalized program
  -> closure backend
  -> Beman Execution sender backend
  -> reflection reification spike
```

- The reader parses Scheme data.
- The reader does not classify special forms.
- The elaborator recognizes `if`, `lambda`, application, quote semantics, and binding forms.
- Keep datum nodes distinct from core nodes.
- Use fixed-capacity constexpr structures for the first implementation.
- Use callable parser objects, not raw function pointers.
- Keep CPS as the semantic center.
- Use the closure backend as the stable demo path.
- Use Beman Execution for the sender backend.
- Keep reflection isolated until explicitly integrated.

## Parser rules

- Parser objects must support captures.
- Do not represent parsers as raw function pointers.
- Keep parser combinators independent from Scheme semantics.
- Build parser vocabulary incrementally.
- Prefer applicative composition.
- Avoid monadic parsing unless the step explicitly requires it.
- Parse datum syntax first.
- Elaborate language forms later.

## Backend rules

- Do not use `std::any` as the core value representation.
- Do not use `std::move_only_function` as core IR.
- Do not assume standard `any_sender`.
- Use Beman Execution for sender graphs.
- Keep sender support adapter-based if it simplifies Scheme backend code.
- Keep backend tests independent where possible.
- Keep the Godbolt demo small.

## Reflection rules

- Reflection code is isolated.
- Reflection code lives under an experimental path until explicitly integrated.
- Do not let reflection objects cross into runtime.
- Generate ordinary aggregate and storage shapes only.

## Slides and transclusion

- Code shown in slides must come from real source via UUID anchors.
- Do not transclude include guards.
- Do not transclude duplicate includes.
- Do not transclude boilerplate.
- Prefer short executable examples.
- One UUID block represents one slide concept.
- Do not nest UUID blocks.
- Do not invent illustrative code that does not compile.
- Do not delete existing UUID anchor pairs.

## Prose and docs

- Use one sentence per line in Markdown, Org, and LaTeX.
- Do not hard-wrap prose to a fixed column.
- Keep sentence boundaries stable.
- Keep `AGENTS.md` concise and imperative.
- Put rationale in `docs/codestyle.org` or architecture docs.
- Keep `handoff.md` factual.
- Keep `handoff-next.md` focused on the next agent.

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
```

---

# 3. Canonical clean-agent instruction

Use this for each new agent:

```txt
Please read AGENTS.md, docs/codestyle.org, docs/CODING_RULES.md, CLAUDE.md, handoff.md, handoff-next.md, and checklist.md.

Proceed to the next unchecked step described in the plan.

Finish only that step.

Run:

make compile
make test
make lint

When everything is green, update checklist.md, update handoff.md with durable project facts, and write a new handoff-next.md describing exactly what the next clean agent needs to know.

Do not continue into the following step unless the current step is blocked and you document the blocker.
```

---

# 4. Project-wide build conventions

The project uses the top-level `Makefile`.

Required before handoff:

```bash
make compile
make test
make lint
```

Useful commands:

```bash
make
make ctest
make coverage
make install
make testinstall
make presentation
make help
```

Tooling is managed by `uv`.

Do not bypass the Makefile for normal work.

C++26 and GCC16 are the baseline.

Tests use Catch2.

---

# 5. Baseline language, compiler, and dependency policy

C++26 and GCC16 are the project baseline.

Do not add compatibility paths for older C++ standards.

Do not add compatibility paths for non-GCC16 compilers unless explicitly requested.

Use C++26 library and language facilities directly when GCC16 supports them.

If GCC16 does not yet provide a required C++26 standard-library facility, use the designated project dependency instead of adding a portability abstraction for older standards.

The sender backend uses Beman Execution.

Primary sender dependency:

```txt
https://github.com/bemanproject/execution
```

Secondary task/coroutine dependency, only if needed:

```txt
https://github.com/bemanproject/task
```

Vendor these dependencies as git submodules under:

```txt
vendor/execution
vendor/task
```

Do not use FetchContent.

Do not use vcpkg.

Do not use install-time dependency discovery for these dependencies.

Do not vendor these dependencies by git subtree.

Integrate vendored dependencies with CMake using `add_subdirectory`.

Example setup:

```sh
git submodule add https://github.com/bemanproject/execution vendor/execution
git submodule add https://github.com/bemanproject/task vendor/task
git submodule update --init --recursive
```

CMake sketch:

```cmake
add_subdirectory(vendor/execution)

# Add only if task is needed.
# add_subdirectory(vendor/task)
```

The project may still keep a local sender adapter boundary.

That boundary is for isolating project semantics from Beman Execution spelling and churn.

It is not for supporting older compilers, older standards, vcpkg, FetchContent, or multiple sender implementations.

---

# 6. Project namespace and layout

Use this namespace:

```cpp
namespace smd::schemepoc {
}
```

Use merged `src/` layout:

```txt
src/smd/schemepoc/
```

Primary paths:

```txt
src/smd/schemepoc/CMakeLists.txt
src/smd/schemepoc/version.hpp
src/smd/schemepoc/version.cpp
src/smd/schemepoc/version.test.cpp
src/smd/schemepoc/detail/
src/examples/
vendor/execution/
vendor/task/
```

Public include spelling:

```cpp
#include <smd/schemepoc/version.hpp>
```

---

# 7. Initial `checklist.md`

```markdown
# SchemePoC checklist

## Ground rules

- [ ] `docs/codestyle.org` copied into repo.
- [ ] `docs/CODING_RULES.md` copied into repo.
- [ ] `AGENTS.md` created.
- [ ] C++26 baseline recorded.
- [ ] GCC16 baseline recorded.
- [ ] Catch2 test framework recorded.
- [ ] No GTest references remain.
- [ ] Beman Execution policy recorded.
- [ ] Beman dependency submodule policy recorded.
- [ ] Every file has canonical path comment and Emacs mode line.
- [ ] Every source-like file has SPDX header.
- [ ] Header guards use project convention.
- [ ] Includes use canonical angle-bracket spelling.
- [ ] No relative project includes.
- [ ] No `using namespace` in headers.
- [ ] Component header is included first and twice in tests.
- [ ] Every public constexpr API has a constexpr/static_assert test.
- [ ] CMake uses targets and file sets.
- [ ] Local CMakeLists only list local files.
- [ ] `make compile` passes.
- [ ] `make test` passes.
- [ ] `make lint` passes.

## Steps

- [ ] Step -1: add agent/style governance files
- [ ] Step 0: repository recustomization and skeleton
- [ ] Step 1: core utility vocabulary
- [ ] Step 2: input cursor and lexical primitives
- [ ] Step 3: minimal parser object
- [ ] Step 4: functor and applicative combinators
- [ ] Step 5: alternative, repetition, and lexeme
- [ ] Step 6: reader atom model
- [ ] Step 7: fixed-capacity datum tree
- [ ] Step 8: datum reader for lists and quote
- [ ] Step 9: core language model
- [ ] Step 10: datum-to-core elaborator for literals, variables, calls
- [ ] Step 11: direct evaluator for core arithmetic
- [ ] Step 12: elaborate `if`
- [ ] Step 13: typeclass-object facade for parser operations
- [ ] Step 14: generic fixpoint playground
- [ ] Step 15: CPS closure backend facade
- [ ] Step 16: bottom-up CPS direction decision
- [ ] Step 17: defunctionalized CPS program
- [ ] Step 18: closure materialization over CPS program
- [ ] Step 19: public one-shot API
- [ ] Step 20: lambda syntax
- [ ] Step 21: runtime closure values
- [ ] Step 22: function application
- [ ] Step 23: lexical closure capture
- [ ] Step 24: quote elaboration
- [ ] Step 25: error quality pass
- [ ] Step 26: negative compile tests
- [ ] Step 27: Godbolt single-file extraction
- [ ] Step 28: C++ Foreign Function Interface (FFI) spike
- [ ] Step 29: vendor Beman Execution
- [ ] Step 30: sender adapter over Beman Execution
- [ ] Step 31: sender backend over CPS program using Beman Execution
- [ ] Step 32: optional Beman Task integration, only if needed
- [ ] Step 33: reflection spike
- [ ] Step 34: documentation consolidation
```

---

# 8. Initial `handoff.md`

```markdown
# SchemePoC handoff

## Project

This is `smd/schemepoc`, a compile-time Scheme-light proof of concept in C++26 on GCC16.

The architecture is intentionally staged:

```txt
source string
  -> reader datum tree
  -> elaborated core tree
  -> CPS / defunctionalized program
  -> closure backend
  -> Beman Execution sender backend
  -> reflection reification spike
```

The reader parses Scheme data.
It must not classify special forms except as ordinary lists.

The elaborator owns recognition of `if`, `lambda`, calls, quote semantics, and later binding behavior.

The first implementation uses fixed-capacity constexpr arenas instead of heap-backed recursive ASTs.
This is deliberate.
It keeps the Godbolt demo small and avoids constexpr allocation persistence issues.

A generic `Fix<F>` playground may exist separately.

The closure backend is the stable demonstration path.
The sender backend uses Beman Execution through a project adapter if helpful.
Reflection remains isolated until explicitly integrated.

## Rule precedence

`docs/codestyle.org` is authoritative.

Rule order:

```txt
docs/codestyle.org
  > AGENTS.md
  > docs/CODING_RULES.md
  > CLAUDE.md
  > handoff.md / handoff-next.md / checklist.md
```

## Baseline

C++26 is the baseline.

GCC16 is the baseline compiler.

Tests use Catch2.

Do not introduce GTest.

## Sender dependencies

Beman Execution is the sender backend dependency.

It is vendored as a git submodule under:

```txt
vendor/execution
```

Beman Task is added only if needed.

If added, it is vendored as a git submodule under:

```txt
vendor/task
```

Use `add_subdirectory`.

Do not use FetchContent, vcpkg, install-time discovery, or git subtree for these dependencies.

## Build

Run:

```bash
make compile
make test
make lint
```

All steps must keep those green.

## Namespace

Use:

```cpp
namespace smd::schemepoc {
}
```

## Layout

Use merged `src/` layout:

```txt
src/smd/schemepoc/
```

Headers, implementations, and tests live together by component.

## Style

- Canonical repository-relative file path and Emacs mode line first.
- SPDX immediately after.
- Include guards, no `#pragma once`.
- Canonical angle-bracket includes only.
- No relative project includes.
- No `using namespace` in headers.
- Test files include the component header first and twice.
- Prefer `constexpr` everywhere practical.
- Use C++26 directly.
```

---

# 9. Initial `handoff-next.md`

```markdown
# Next step: Step -1

Create the agent and style governance files.

Add:

```txt
AGENTS.md
docs/codestyle.org
docs/CODING_RULES.md
handoff.md
handoff-next.md
checklist.md
```

Copy the provided `codestyle.org` into:

```txt
docs/codestyle.org
```

Copy the provided `CODING_RULES.md` into:

```txt
docs/CODING_RULES.md
```

Create `AGENTS.md` from the consolidated concise rule set in the execution plan.

Create initial `handoff.md`, `handoff-next.md`, and `checklist.md`.

Ensure these facts are recorded:

```txt
C++26 baseline
GCC16 baseline
Catch2 tests
No GTest
Beman Execution via git submodule under vendor/execution
Beman Task via git submodule under vendor/task only if needed
No FetchContent, vcpkg, install-time discovery, or subtree for Beman dependencies
```

After implementation, run:

```bash
make compile
make test
make lint
```

Then mark Step -1 complete in `checklist.md`, update `handoff.md` with any repo-specific build details discovered, and rewrite `handoff-next.md` for Step 0: repository recustomization and skeleton.
```

---

# Step -1 — Add agent/style governance files

## Goal

Install the concise agent rule set and the full style documents.

## Files

```txt
AGENTS.md
docs/codestyle.org
docs/CODING_RULES.md
handoff.md
handoff-next.md
checklist.md
```

## Required content

`docs/codestyle.org` is copied verbatim from the provided source.

`docs/CODING_RULES.md` is copied from the provided source.

`AGENTS.md` contains the concise imperative agent rules from this plan.

`handoff.md`, `handoff-next.md`, and `checklist.md` are initialized from this plan.

## Required policy facts

Record:

```txt
C++26 is baseline.
GCC16 is baseline.
Tests use Catch2.
Do not introduce GTest.
Beman Execution is the sender dependency.
Beman dependencies are vendored by git submodule under vendor/.
Beman dependencies are integrated by add_subdirectory.
Do not use FetchContent, vcpkg, install-time discovery, or subtree for Beman dependencies.
```

## Merge criteria

Run:

```bash
make compile
make test
make lint
```

All pass.

## handoff-next.md

Point the next agent at Step 0.

---

# Step 0 — Repository recustomization and skeleton

## Goal

Create the greenfield SchemePoC skeleton from the copied baseline, replacing example names with `smd/schemepoc`.

## Deliverables

Create or adapt:

```txt
src/smd/schemepoc/CMakeLists.txt
src/smd/schemepoc/version.hpp
src/smd/schemepoc/version.cpp
src/smd/schemepoc/version.test.cpp
```

If the Copier template requires synchronized changes, update the corresponding `template/` files.

## Minimal header

```cpp
// src/smd/schemepoc/version.hpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_VERSION_HPP
#define SRC_SMD_SCHEMEPOC_VERSION_HPP

namespace smd::schemepoc {

inline constexpr int version_major = 0;
inline constexpr int version_minor = 1;
inline constexpr int version_patch = 0;

} // namespace smd::schemepoc

#endif
```

## Test

Use Catch2.

```cpp
// src/smd/schemepoc/version.test.cpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/version.hpp>
#include <smd/schemepoc/version.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("VersionTest - HeaderIsIdempotent")
{
    REQUIRE(true);
}

TEST_CASE("VersionTest - VersionIsAvailable")
{
    CHECK(smd::schemepoc::version_major == 0);
}
```

## Merge criteria

Run:

```bash
make compile
make test
make lint
```

All pass.

## handoff-next.md

Say the next step adds the compile-time support vocabulary: source positions, result type, and fixed-capacity vector.

---

# Step 1 — Core utility vocabulary

## Goal

Add low-level constexpr-friendly types used by all later phases.

## Files

```txt
src/smd/schemepoc/source.hpp
src/smd/schemepoc/result.hpp
src/smd/schemepoc/static_vector.hpp
src/smd/schemepoc/source.test.cpp
src/smd/schemepoc/result.test.cpp
src/smd/schemepoc/static_vector.test.cpp
```

## Required APIs

```cpp
namespace smd::schemepoc {

struct source_pos {
    int offset{};
    int line{1};
    int column{1};

    friend constexpr auto operator==(source_pos, source_pos) -> bool = default;
};

struct source_span {
    source_pos first{};
    source_pos last{};

    friend constexpr auto operator==(source_span, source_span) -> bool = default;
};

struct parse_error {
    source_pos where{};
    char const* message{};

    friend constexpr auto operator==(parse_error, parse_error) -> bool = default;
};

template <class T>
class result {
public:
    constexpr result(T value);
    constexpr result(parse_error error);

    [[nodiscard]] constexpr auto has_value() const -> bool;
    [[nodiscard]] constexpr auto value() const -> T const&;
    [[nodiscard]] constexpr auto error() const -> parse_error const&;

private:
    std::variant<T, parse_error> data_;
};

template <class T, int Capacity>
class static_vector {
public:
    constexpr static_vector() = default;

    constexpr auto push_back(T value) -> void;
    [[nodiscard]] constexpr auto size() const -> int;
    [[nodiscard]] constexpr auto empty() const -> bool;
    [[nodiscard]] constexpr auto operator[](int index) -> T&;
    [[nodiscard]] constexpr auto operator[](int index) const -> T const&;

private:
    std::array<T, Capacity> storage_{};
    int size_{};
};

}
```

Use `std::variant` initially for `result`.

Use `std::array<T, Capacity>` for `static_vector`.

Use a simple assertion strategy for capacity overflow.

Do not design a general allocator.

## Tests

```cpp
constexpr auto make_vec()
{
    smd::schemepoc::static_vector<int, 4> xs;
    xs.push_back(1);
    xs.push_back(2);
    return xs;
}

static_assert(make_vec().size() == 2);
static_assert(make_vec()[0] == 1);
static_assert(make_vec()[1] == 2);
```

## Merge criteria

All tests pass.

No parser code yet.

---

# Step 2 — Input cursor and lexical primitives

## Goal

Create a constexpr cursor over `std::string_view`, with whitespace and delimiter handling for Scheme datum reading.

## Files

```txt
src/smd/schemepoc/reader_cursor.hpp
src/smd/schemepoc/reader_cursor.test.cpp
```

## Required API

```cpp
namespace smd::schemepoc {

class cursor {
public:
    constexpr explicit cursor(std::string_view input);

    [[nodiscard]] constexpr auto empty() const -> bool;
    [[nodiscard]] constexpr auto peek() const -> char;
    [[nodiscard]] constexpr auto bump() const -> cursor;
    [[nodiscard]] constexpr auto position() const -> source_pos;
    [[nodiscard]] constexpr auto remaining() const -> std::string_view;

private:
    std::string_view input_{};
    int offset_{};
    int line_{1};
    int column_{1};
};

[[nodiscard]] constexpr auto is_space(char ch) -> bool;
[[nodiscard]] constexpr auto is_initial_symbol_char(char ch) -> bool;
[[nodiscard]] constexpr auto is_symbol_char(char ch) -> bool;
[[nodiscard]] constexpr auto is_delimiter(char ch) -> bool;
[[nodiscard]] constexpr auto skip_intertoken_space(cursor cur) -> cursor;

}
```

Symbol rules for this step:

```txt
symbol initial: alphabetic or one of + - * / = < > ! ?
symbol rest: initial or digit
delimiter: whitespace, (, ), ', end
```

Do not support comments yet.

## Tests

```cpp
static_assert(smd::schemepoc::is_delimiter('('));
static_assert(smd::schemepoc::is_delimiter(')'));
static_assert(smd::schemepoc::skip_intertoken_space(smd::schemepoc::cursor{"  x"}).peek() == 'x');
```

## Merge criteria

Cursor handles offset, line, and column advancement.

No combinators yet.

---

# Step 3 — Minimal parser object, no typeclasses yet

## Goal

Establish the parser representation correctly.

Use a closure wrapper around a callable object.

Do not use a raw function pointer.

## Files

```txt
src/smd/schemepoc/parser.hpp
src/smd/schemepoc/parser.test.cpp
```

## Required model

```cpp
namespace smd::schemepoc {

template <class T>
struct parse_state {
    T value;
    cursor rest;
};

template <class T>
using parse_result = result<parse_state<T>>;

template <class F>
class parser {
public:
    constexpr explicit parser(F f);

    constexpr auto operator()(cursor cur) const;

private:
    F f_;
};

template <class F>
parser(F) -> parser<F>;

template <class T>
[[nodiscard]] constexpr auto pure(T value)
{
    return parser{[value](cursor cur) constexpr -> parse_result<T> {
        return parse_state<T>{value, cur};
    }};
}

[[nodiscard]] constexpr auto satisfy(auto pred, char const* expected);
[[nodiscard]] constexpr auto char_p(char expected);

}
```

## Tests

```cpp
constexpr auto p = smd::schemepoc::char_p('x');
constexpr auto r = p(smd::schemepoc::cursor{"xyz"});

static_assert(r.has_value());
static_assert(r.value().value == 'x');
static_assert(r.value().rest.peek() == 'y');
```

## Design note to write in `handoff.md`

Parser combinators must preserve capturing parser objects.

Raw function pointers are intentionally not used.

---

# Step 4 — Functor and Applicative combinators

## Goal

Add `map`, `apply`, `lift2`, `sequence_left`, and `sequence_right`.

## Files

```txt
src/smd/schemepoc/parser_combinators.hpp
src/smd/schemepoc/parser_applicative.test.cpp
```

## APIs

```cpp
namespace smd::schemepoc {

template <class P, class F>
[[nodiscard]] constexpr auto map(P p, F f);

template <class PF, class PA>
[[nodiscard]] constexpr auto apply(PF pf, PA pa);

template <class PLeft, class PRight>
[[nodiscard]] constexpr auto sequence_right(PLeft left, PRight right);

template <class PLeft, class PRight>
[[nodiscard]] constexpr auto sequence_left(PLeft left, PRight right);

template <class F, class PA, class PB>
[[nodiscard]] constexpr auto lift2(F f, PA pa, PB pb);

}
```

Implementation sketch:

```cpp
template <class P, class F>
[[nodiscard]] constexpr auto map(P p, F f)
{
    return parser{[p, f](cursor cur) constexpr {
        auto r = p(cur);
        using input_type = decltype(r.value().value);
        using output_type = std::invoke_result_t<F, input_type>;

        if (!r.has_value()) {
            return parse_result<output_type>{r.error()};
        }

        return parse_result<output_type>{
            parse_state<output_type>{f(r.value().value), r.value().rest}
        };
    }};
}
```

The exact implementation may need helper aliases to avoid awkward unevaluated contexts.

Keep it simple and local.

## Tests

Parse two chars and combine:

```cpp
constexpr auto two =
    smd::schemepoc::lift2(
        [](char a, char b) { return std::array{a, b}; },
        smd::schemepoc::char_p('a'),
        smd::schemepoc::char_p('b'));

static_assert(two(smd::schemepoc::cursor{"abc"}).value().value[0] == 'a');
static_assert(two(smd::schemepoc::cursor{"abc"}).value().value[1] == 'b');
```

## Merge criteria

This step should still not parse Scheme.

It proves applicative composition.

---

# Step 5 — Alternative, repetition, and lexeme

## Goal

Add parser choice and repetition.

This makes the reader grammar pleasant.

## Files

```txt
src/smd/schemepoc/parser_alternative.hpp
src/smd/schemepoc/parser_alternative.test.cpp
```

## APIs

```cpp
namespace smd::schemepoc {

template <class PA, class PB>
[[nodiscard]] constexpr auto alt(PA pa, PB pb);

template <int Capacity, class P>
[[nodiscard]] constexpr auto many(P p);

template <int Capacity, class P>
[[nodiscard]] constexpr auto some(P p);

template <class P>
[[nodiscard]] constexpr auto optional(P p);

template <class P>
[[nodiscard]] constexpr auto lexeme(P p);

}
```

For `many`, return `static_vector<T, Capacity>`.

Important behavior:

```txt
many never fails.
some fails if the first element fails.
alt returns the first success.
alt returns the first error initially.
Do not implement longest-error logic yet.
```

Implementation sketch for `lexeme`:

```cpp
template <class P>
[[nodiscard]] constexpr auto lexeme(P p)
{
    return parser{[p](cursor cur) constexpr {
        auto start = skip_intertoken_space(cur);
        auto r = p(start);

        if (!r.has_value()) {
            return r;
        }

        auto rest = skip_intertoken_space(r.value().rest);

        return parse_result<decltype(r.value().value)>{
            parse_state<decltype(r.value().value)>{r.value().value, rest}
        };
    }};
}
```

## Tests

Cover:

```txt
alt success on first
alt fallback to second
many parses zero
many parses several
some fails on zero
lexeme skips surrounding intertoken space
```

## Merge criteria

Tests cover success, fallback, and repetition cap behavior.

---

# Step 6 — Reader atom model

## Goal

Introduce datum atom types without recursive lists yet.

## Files

```txt
src/smd/schemepoc/datum.hpp
src/smd/schemepoc/datum_atom.test.cpp
```

## Types

```cpp
namespace smd::schemepoc {

struct symbol {
    std::string_view text{};

    friend constexpr auto operator==(symbol, symbol) -> bool = default;
};

struct integer {
    int value{};

    friend constexpr auto operator==(integer, integer) -> bool = default;
};

struct boolean {
    bool value{};

    friend constexpr auto operator==(boolean, boolean) -> bool = default;
};

struct datum_atom {
    std::variant<integer, boolean, symbol> value{};

    friend constexpr auto operator==(datum_atom const&, datum_atom const&) -> bool = default;
};

}
```

## Atom parsers

Support:

```txt
#t
#f
integer: optional leading -, then digits
symbol: initial symbol char, then symbol chars
```

Expose:

```cpp
[[nodiscard]] constexpr auto integer_p();
[[nodiscard]] constexpr auto boolean_p();
[[nodiscard]] constexpr auto symbol_p();
[[nodiscard]] constexpr auto atom_p();
```

## Tests

```cpp
static_assert(read_atom("123").value() == datum_atom{integer{123}});
static_assert(read_atom("#t").value() == datum_atom{boolean{true}});
static_assert(read_atom("+").value() == datum_atom{symbol{"+"}});
```

## Merge criteria

No list parsing yet.

Atoms only.

---

# Step 7 — Fixed-capacity datum tree

## Goal

Add recursive datum tree with bounded child storage suitable for constexpr demos.

## Design choice

For the first implementation, do not use `std::indirect`, `std::vector`, or heap-backed recursion.

Use a fixed arena and node IDs.

This keeps the Godbolt path simple and avoids constexpr allocation persistence problems.

## Files

```txt
src/smd/schemepoc/datum_tree.hpp
src/smd/schemepoc/datum_tree.test.cpp
```

## Types

```cpp
namespace smd::schemepoc {

using node_id = int;

inline constexpr node_id invalid_node = -1;

template <int MaxNodes, int MaxList>
class datum_tree {
public:
    struct list {
        static_vector<node_id, MaxList> elements{};
    };

    struct quote {
        node_id quoted{invalid_node};
    };

    using node_value = std::variant<integer, boolean, symbol, list, quote>;

    constexpr auto add(node_value value) -> node_id;
    [[nodiscard]] constexpr auto get(node_id id) const -> node_value const&;
    [[nodiscard]] constexpr auto size() const -> int;

private:
    static_vector<node_value, MaxNodes> nodes_{};
};

}
```

## Tests

Manually build:

```cpp
constexpr auto t = [] {
    smd::schemepoc::datum_tree<8, 4> tree;
    auto one = tree.add(smd::schemepoc::integer{1});
    auto two = tree.add(smd::schemepoc::integer{2});

    smd::schemepoc::datum_tree<8, 4>::list xs;
    xs.elements.push_back(one);
    xs.elements.push_back(two);

    tree.add(xs);
    return tree;
}();

static_assert(t.size() == 3);
```

## handoff.md note

This is a deliberate divergence from the fully generic `Fix<F>` ideal.

The arena representation is the practical first milestone.

Later steps can add `Fix<F>` as a second representation.

---

# Step 8 — Datum reader for lists and quote

## Goal

Parse complete Scheme datum forms into the fixed-capacity `datum_tree`.

## Files

```txt
src/smd/schemepoc/reader.hpp
src/smd/schemepoc/reader.test.cpp
```

## API

```cpp
namespace smd::schemepoc {

template <int MaxNodes = 64, int MaxList = 16>
struct read_datum_result {
    datum_tree<MaxNodes, MaxList> tree{};
    node_id root{invalid_node};
    cursor rest{std::string_view{}};
};

template <int MaxNodes = 64, int MaxList = 16>
[[nodiscard]] constexpr auto read_datum(std::string_view input)
    -> result<read_datum_result<MaxNodes, MaxList>>;

}
```

Reader grammar:

```txt
datum  := atom | list | quote
list   := '(' datum* ')'
quote  := '\'' datum
atom   := integer | boolean | symbol
```

Quote lowering at reader phase:

```txt
'x parses as datum_tree quote{x}
```

Do not lower quote to `(quote x)` yet.

Preserve source reality.

## Tests

```cpp
constexpr auto r = smd::schemepoc::read_datum("(+ 1 (* 2 3))");

static_assert(r.has_value());
static_assert(r.value().tree.size() == 7);
```

The exact node count may vary.

Lock the count after implementation.

Test failures:

```txt
")" fails
"(" fails
"(1 2" fails
```

## Merge criteria

First visible milestone: constexpr reader for nested Scheme datums.

---

# Step 9 — Core language model

## Goal

Introduce elaborated semantic core separate from datum.

## Files

```txt
src/smd/schemepoc/core.hpp
src/smd/schemepoc/core.test.cpp
```

## Types

```cpp
namespace smd::schemepoc {

using core_id = int;

inline constexpr core_id invalid_core = -1;

struct var {
    symbol name{};
};

struct lit_int {
    int value{};
};

struct lit_bool {
    bool value{};
};

template <int MaxArgs>
struct call {
    core_id function{invalid_core};
    static_vector<core_id, MaxArgs> arguments{};
};

struct if_expr {
    core_id condition{invalid_core};
    core_id consequent{invalid_core};
    core_id alternative{invalid_core};
};

template <int MaxParams>
struct lambda {
    static_vector<symbol, MaxParams> params{};
    core_id body{invalid_core};
};

template <int MaxNodes, int MaxArgs, int MaxParams>
class core_tree {
public:
    using node_value = std::variant<
        var,
        lit_int,
        lit_bool,
        call<MaxArgs>,
        if_expr,
        lambda<MaxParams>>;

    constexpr auto add(node_value value) -> core_id;
    [[nodiscard]] constexpr auto get(core_id id) const -> node_value const&;
    [[nodiscard]] constexpr auto size() const -> int;

private:
    static_vector<node_value, MaxNodes> nodes_{};
};

}
```

No elaboration yet.

## Tests

Manually construct `(+ 1 2)` core.

## Merge criteria

Compile, test, lint.

No evaluator.

---

# Step 10 — Datum-to-core elaborator for literals, variables, calls

## Goal

Lower reader datums into semantic core for the smallest useful expression subset.

## Files

```txt
src/smd/schemepoc/elaborate.hpp
src/smd/schemepoc/elaborate_basic.test.cpp
```

## API

```cpp
namespace smd::schemepoc {

template <int MaxDatumNodes, int MaxList, int MaxCoreNodes, int MaxArgs, int MaxParams>
struct elaborate_result {
    core_tree<MaxCoreNodes, MaxArgs, MaxParams> core{};
    core_id root{invalid_core};
};

template <
    int MaxDatumNodes = 64,
    int MaxList = 16,
    int MaxCoreNodes = 64,
    int MaxArgs = 8,
    int MaxParams = 8>
[[nodiscard]] constexpr auto elaborate(
    datum_tree<MaxDatumNodes, MaxList> const& datum,
    node_id root)
    -> result<elaborate_result<MaxDatumNodes, MaxList, MaxCoreNodes, MaxArgs, MaxParams>>;

}
```

Rules:

```txt
integer datum -> lit_int
boolean datum -> lit_bool
symbol datum -> var
nonempty list -> call
empty list -> error: "empty application"
quote datum -> unsupported for now
```

## Test

```cpp
constexpr auto parsed = smd::schemepoc::read_datum("(+ 1 2)");
constexpr auto elaborated =
    smd::schemepoc::elaborate(parsed.value().tree, parsed.value().root);

static_assert(elaborated.has_value());
```

## Merge criteria

Can parse and elaborate `(+ 1 2)`.

---

# Step 11 — Direct evaluator for core arithmetic

## Goal

Before CPS, prove the front-end by interpreting core directly.

## Files

```txt
src/smd/schemepoc/value.hpp
src/smd/schemepoc/eval_direct.hpp
src/smd/schemepoc/eval_direct.test.cpp
```

## Runtime/constexpr value

```cpp
namespace smd::schemepoc {

struct builtin {
    enum kind {
        add,
        subtract,
        multiply,
        equal
    } op{};
};

struct value {
    std::variant<int, bool, builtin> data{};

    friend constexpr auto operator==(value const&, value const&) -> bool = default;
};

template <int MaxBindings>
class env {
public:
    constexpr auto define(symbol name, value val) -> void;
    [[nodiscard]] constexpr auto lookup(symbol name) const -> result<value>;

private:
    struct binding {
        symbol name{};
        value val{};
    };

    static_vector<binding, MaxBindings> bindings_{};
};

template <int MaxBindings>
constexpr auto default_env() -> env<MaxBindings>;

}
```

Evaluator:

```cpp
template <int MaxNodes, int MaxArgs, int MaxParams, int MaxBindings>
[[nodiscard]] constexpr auto eval_direct(
    core_tree<MaxNodes, MaxArgs, MaxParams> const& core,
    core_id root,
    env<MaxBindings> const& environment) -> result<value>;
```

Builtins:

```cpp
template <int MaxBindings>
constexpr auto default_env() -> env<MaxBindings>
{
    env<MaxBindings> e;
    e.define(symbol{"+"}, value{builtin{builtin::add}});
    e.define(symbol{"*"}, value{builtin{builtin::multiply}});
    return e;
}
```

## Tests

```cpp
constexpr auto run = [] {
    auto parsed = smd::schemepoc::read_datum("(+ 1 (* 2 3))").value();
    auto elab = smd::schemepoc::elaborate(parsed.tree, parsed.root).value();
    return smd::schemepoc::eval_direct(
        elab.core,
        elab.root,
        smd::schemepoc::default_env<16>()).value();
}();

static_assert(run == smd::schemepoc::value{7});
```

Adjust construction if `value{7}` needs explicit variant construction.

## Merge criteria

First end-to-end compile-time Scheme-light arithmetic.

---

# Step 12 — Elaborate `if`

## Goal

Add special-form recognition for `if`.

## Files

```txt
src/smd/schemepoc/elaborate_if.test.cpp
```

Modify:

```txt
src/smd/schemepoc/elaborate.hpp
src/smd/schemepoc/eval_direct.hpp
```

## Rules

```txt
(if condition consequent alternative) -> if_expr
wrong arity -> error
```

Evaluation truthiness:

```txt
#f is false
everything else is true
```

## Tests

```cpp
static_assert(run("(if #t 1 2)") == value{1});
static_assert(run("(if #f 1 2)") == value{2});
```

## Merge criteria

`if` works in direct evaluator.

---

# Step 13 — Typeclass-object facade for parser operations

## Goal

Introduce the project’s typeclass-object flavor without destabilizing the working parser.

This step wraps existing free functions rather than rewriting the parser.

## Files

```txt
src/smd/schemepoc/typeclass.hpp
src/smd/schemepoc/parser_ops.hpp
src/smd/schemepoc/parser_ops.test.cpp
```

## Sketch

```cpp
namespace smd::schemepoc {

template <class T>
inline constexpr bool always_false_v = false;

struct parser_applicative_ops {
    template <class T>
    [[nodiscard]] constexpr auto pure(T value) const
    {
        return smd::schemepoc::pure(value);
    }

    template <class P, class F>
    [[nodiscard]] constexpr auto map(P p, F f) const
    {
        return smd::schemepoc::map(p, f);
    }

    template <class PF, class PA>
    [[nodiscard]] constexpr auto apply(PF pf, PA pa) const
    {
        return smd::schemepoc::apply(pf, pa);
    }
};

struct parser_alternative_ops {
    template <class PA, class PB>
    [[nodiscard]] constexpr auto alt(PA pa, PB pb) const
    {
        return smd::schemepoc::alt(pa, pb);
    }
};

struct parser_ops : parser_applicative_ops, parser_alternative_ops {
};

inline constexpr parser_ops parser_v{};

}
```

## Usage test

```cpp
constexpr auto p =
    smd::schemepoc::parser_v.map(
        smd::schemepoc::char_p('x'),
        [](char) { return 42; });
```

## Merge criteria

No behavior changes.

This is a presentation/refactoring step.

---

# Step 14 — Generic fixpoint playground

## Goal

Add a small, isolated `Fix<F>` and `fold_fix` experiment without converting the main arena-based compiler yet.

## Why now

The project already works end-to-end for arithmetic.

This step introduces the research architecture in a safe sandbox.

## Files

```txt
src/smd/schemepoc/fix.hpp
src/smd/schemepoc/fix.test.cpp
```

## Sketch

```cpp
namespace smd::schemepoc {

template <template <class> class F>
class fix {
public:
    using layer_type = F<fix<F>>;

    constexpr explicit fix(layer_type layer);

    [[nodiscard]] constexpr auto layer() const -> layer_type const&;

private:
    layer_type layer_;
};

template <template <class> class F, class Algebra>
constexpr auto fold_fix(fix<F> const& tree, Algebra algebra)
{
    auto mapped = fmap(tree.layer(), [&](auto const& child) {
        return fold_fix(child, algebra);
    });

    return algebra(mapped);
}

}
```

Keep the test tiny.

Example expression family:

```cpp
template <class A>
struct int_f {
    int value{};
};

template <class A>
struct add_f {
    A left;
    A right;
};

template <class A>
using expr_f = std::variant<int_f<A>, add_f<A>>;
```

## Merge criteria

This step proves recursion schemes separately.

Do not refactor reader or core yet.

---

# Step 15 — CPS closure backend facade, arithmetic only

## Goal

Compile core expressions to continuation-style callable objects.

Do not handle lambdas yet.

## Files

```txt
src/smd/schemepoc/cps.hpp
src/smd/schemepoc/cps_arithmetic.test.cpp
```

## Minimal shape

Use concrete templated callables instead of `std::move_only_function` or `std::any`.

```cpp
namespace smd::schemepoc {

template <class F>
struct cps_code {
    F f;

    template <class Env, class K>
    constexpr auto operator()(Env const& env, K k) const
    {
        return f(env, k);
    }
};

template <class F>
cps_code(F) -> cps_code<F>;

template <int MaxNodes, int MaxArgs, int MaxParams>
[[nodiscard]] constexpr auto compile_cps(
    core_tree<MaxNodes, MaxArgs, MaxParams> const& core,
    core_id root);

}
```

For the first implementation, `compile_cps` may return an interpreter-backed CPS object that captures `core` and `root`:

```cpp
return cps_code{[core, root](auto const& env, auto k) constexpr {
    auto v = eval_direct(core, root, env);

    if (!v.has_value()) {
        return v;
    }

    return k(v.value());
}};
```

This is not yet a true bottom-up CPS compiler.

That is acceptable for this step.

The point is to lock the callable boundary and tests.

## Test

```cpp
constexpr auto result = code(default_env<16>(), [](value v) constexpr {
    return result<value>{v};
});

static_assert(result.value() == value{7});
```

## Merge criteria

End-to-end arithmetic via CPS facade.

---

# Step 16 — Bottom-up CPS direction decision

## Goal

Replace or supplement interpreter-backed CPS with a more explicit representation.

Because each node may produce a different closure type, this step decides between two paths:

```txt
A. continue higher-order typed closures for a small demo
B. introduce defunctionalized CPS program as inspectable data
```

Recommended choice: introduce defunctionalized CPS program.

## Files

Possible new files:

```txt
src/smd/schemepoc/cps_program.hpp
src/smd/schemepoc/cps_compile.hpp
src/smd/schemepoc/cps_eval.hpp
```

## Decision record

Write the decision in `handoff.md`.

Recommended wording:

```markdown
The project will use a defunctionalized CPS program as the durable compiled representation.
The closure backend will materialize an executable wrapper over this program.
This keeps the compiled artifact inspectable, constexpr-friendly, and small enough for Godbolt.
```

## Merge criteria

Tests remain green.

The selected CPS direction is documented.

Do not attempt all calls yet unless this is trivial.

---

# Step 17 — Defunctionalized CPS program for arithmetic calls

## Goal

Make CPS explicit as data.

## Files

```txt
src/smd/schemepoc/cps_program.hpp
src/smd/schemepoc/cps_compile.hpp
src/smd/schemepoc/cps_eval.hpp
src/smd/schemepoc/cps_program.test.cpp
```

## Program model

```cpp
namespace smd::schemepoc {

using instr_id = int;

inline constexpr instr_id invalid_instr = -1;

enum class instr_kind {
    literal,
    lookup,
    call
};

template <int MaxArgs>
struct instr {
    instr_kind kind{};
    value literal{};
    symbol name{};
    instr_id function{invalid_instr};
    static_vector<instr_id, MaxArgs> arguments{};
};

template <int MaxInstr, int MaxArgs>
class cps_program {
public:
    constexpr auto add(instr<MaxArgs> i) -> instr_id;
    [[nodiscard]] constexpr auto get(instr_id id) const -> instr<MaxArgs> const&;
    [[nodiscard]] constexpr auto size() const -> int;

private:
    static_vector<instr<MaxArgs>, MaxInstr> instrs_{};
};

template <int MaxInstr, int MaxArgs>
struct cps_compile_result {
    cps_program<MaxInstr, MaxArgs> program{};
    instr_id root{invalid_instr};
};

}
```

Compiler:

```cpp
template <int MaxNodes, int MaxArgs, int MaxParams, int MaxInstr>
constexpr auto compile_cps_program(
    core_tree<MaxNodes, MaxArgs, MaxParams> const& core,
    core_id root) -> result<cps_compile_result<MaxInstr, MaxArgs>>;
```

Evaluator can still be recursive initially:

```cpp
template <int MaxInstr, int MaxArgs, int MaxBindings>
constexpr auto eval_cps_program(
    cps_program<MaxInstr, MaxArgs> const& program,
    instr_id root,
    env<MaxBindings> const& environment) -> result<value>;
```

## Tests

Same arithmetic as before:

```scheme
(+ 1 (* 2 3))
```

Expected result:

```txt
7
```

## Merge criteria

CPS is now inspectable data.

---

# Step 18 — Closure materialization over CPS program

## Goal

Provide the “lambda with captures” backend as a tiny executable wrapper over generated CPS data.

## Files

```txt
src/smd/schemepoc/closure_backend.hpp
src/smd/schemepoc/closure_backend.test.cpp
```

## API

```cpp
namespace smd::schemepoc {

template <int MaxInstr, int MaxArgs>
struct closure_program {
    cps_program<MaxInstr, MaxArgs> program{};
    instr_id entry{};

    template <int MaxBindings>
    constexpr auto operator()(env<MaxBindings> const& environment) const
        -> result<value>
    {
        return eval_cps_program(program, entry, environment);
    }
};

template <int MaxInstr, int MaxArgs>
constexpr auto make_closure_program(
    cps_program<MaxInstr, MaxArgs> program,
    instr_id entry) -> closure_program<MaxInstr, MaxArgs>;

}
```

End-to-end helper:

```cpp
template <int MaxNodes = 64, int MaxList = 16, int MaxCore = 64, int MaxInstr = 64>
constexpr auto compile_to_closure(std::string_view source);
```

## Test

```cpp
constexpr auto compiled = compile_to_closure("(+ 1 (* 2 3))").value();
constexpr auto result = compiled(default_env<16>()).value();

static_assert(result == value{7});
```

## Merge criteria

First “compile-time compiler, runtime executable object” milestone.

---

# Step 19 — Public one-shot API

## Goal

Expose a clean API for the demo and future Godbolt snippets.

## Files

```txt
src/smd/schemepoc/schemepoc.hpp
src/smd/schemepoc/public_api.test.cpp
```

## API

```cpp
namespace smd::schemepoc {

template <std::size_t N>
struct source_literal {
    char text[N]{};

    constexpr source_literal(char const (&input)[N])
    {
        std::copy_n(input, N, text);
    }

    constexpr auto view() const -> std::string_view
    {
        return {text, N - 1};
    }
};

template <source_literal Source>
inline constexpr auto compiled_closure = compile_to_closure(Source.view()).value();

}
```

## Usage

```cpp
constexpr auto program =
    smd::schemepoc::compiled_closure<"(+ 1 (* 2 3))">;

int main()
{
    auto result = program(smd::schemepoc::default_env<16>()).value();
}
```

## Merge criteria

Public API compiles and tests.

---

# Step 20 — Add `lambda` syntax to elaborator, no closures yet

## Goal

Parse and elaborate lambda forms structurally.

The reader already supports lists and symbols.

## Elaboration rule

```txt
(lambda (param*) body) -> lambda node
```

Constraints:

```txt
params must be symbols
body is exactly one expression initially
duplicate params are an error
```

## Test input

```scheme
(lambda (x) (+ x 1))
```

Expected result: correct core shape.

Do not evaluate yet.

## Merge criteria

Core tree can represent lambda.

---

# Step 21 — Runtime closure values

## Goal

Add closures to `value`.

## Types

Initial sketch:

```cpp
template <int MaxParams>
struct closure_value {
    static_vector<symbol, MaxParams> params{};
    core_id body{};
};
```

This step may force `value` to become capacity-parameterized.

Recommended option:

```cpp
template <int MaxParams, int MaxBindings>
struct basic_value;
```

Small-agent fallback:

```txt
Support lambdas only in the CPS/closure backend first.
Do not force direct evaluator generality if it causes type explosion.
```

## Required decision

Document the selected value-capacity strategy in `handoff.md`.

## Merge criteria

Closure value construction is tested.

The capacity strategy is documented.

---

# Step 22 — Function application for user lambdas

## Goal

Evaluate:

```scheme
((lambda (x) (+ x 1)) 41)
```

Direct evaluator first if practical.

CPS backend can follow.

## Rules

```txt
evaluate function
evaluate args left-to-right
if builtin, apply builtin
if closure, extend captured environment with params
evaluate body
arity mismatch is error
```

## Tests

```cpp
static_assert(run("((lambda (x) (+ x 1)) 41)") == value{42});
```

## Merge criteria

Simple user function application works.

---

# Step 23 — Lexical closure capture

## Goal

Support lexical capture:

```scheme
((lambda (x) ((lambda (y) (+ x y)) 2)) 40)
```

## Environment behavior

```txt
lambda evaluation captures current env
application extends captured env
```

## Tests

```cpp
static_assert(run("((lambda (x) ((lambda (y) (+ x y)) 2)) 40)") == value{42});
```

## Merge criteria

Lexical scoping demonstrated.

---

# Step 24 — Quote elaboration and quoted data value

## Goal

Support quote for atoms first:

```scheme
'foo
'123
'#t
```

Recommended split:

```txt
Step 24: quote symbols, integers, booleans
Later: quote lists
```

## Tests

```cpp
static_assert(run("'foo") == value{symbol{"foo"}});
```

Adjust `value` to support quoted symbols.

## Merge criteria

Quote no longer errors for atoms.

---

# Step 25 — Error quality pass

## Goal

Make parser, elaborator, and evaluator errors useful enough for humans.

## Add messages for

```txt
unexpected end of input
expected ')'
expected datum
empty application
unknown special form shape
if arity
lambda malformed parameter list
duplicate parameter
unbound variable
attempted to call non-function
arity mismatch
```

## Tests

At least one error test per phase:

```txt
reader
elaborator
evaluator
```

Tests should check specific error messages.

## Merge criteria

Diagnostics are stable enough for tests.

---

# Step 26 — Negative compile tests

## Goal

Add negative compile tests for public constraints.

## Initial negative tests

```txt
source_literal rejects non-string-literal usage where applicable
static_vector overflow in consteval context produces intended diagnostic
parser combinator misuse produces specific concept diagnostic
```

Add concepts if necessary:

```cpp
template <class P>
concept parser_like = requires(P p, cursor c) {
    p(c);
};
```

Register negative tests according to the repository’s existing CMake/test convention.

For object-library negative tests:

```txt
OBJECT library
EXCLUDE_FROM_ALL
ctest invokes cmake --build --target <target>
PASS_REGULAR_EXPRESSION matches the specific diagnostic
```

## Merge criteria

Negative compile tests are registered and pass.

---

# Step 27 — Godbolt single-file extraction

## Goal

Create a small demonstration file suitable for Compiler Explorer.

## File

```txt
src/examples/godbolt_arithmetic.cpp
```

or, if local convention prefers:

```txt
examples/godbolt_arithmetic.cpp
```

Follow `docs/codestyle.org` for placement.

## Contents

```cpp
// src/examples/godbolt_arithmetic.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/schemepoc.hpp>

#include <iostream>

constexpr auto program =
    smd::schemepoc::compiled_closure<"(+ 1 (* 2 3))">;

int main()
{
    auto result = program(smd::schemepoc::default_env<16>()).value();
    std::cout << std::get<int>(result.data) << '\n';
}
```

Expected output:

```txt
7
```

## Merge criteria

Example builds under CMake.

Docs mention how to paste/use on Compiler Explorer.

---

# Step 28 — Vendor Beman Execution

## Goal

Add Beman Execution as the sender/receiver dependency for the project.

This is the project’s sender baseline because GCC16 does not yet provide the needed standard-library sender implementation.

## Files and directories

```txt
vendor/execution
.gitmodules
CMakeLists.txt
src/smd/schemepoc/CMakeLists.txt
handoff.md
handoff-next.md
checklist.md
```

## Required commands

```sh
git submodule add https://github.com/bemanproject/execution vendor/execution
git submodule update --init --recursive
```

If Beman Task is not needed yet, do not add it in this step.

## CMake integration

At the nearest appropriate project CMake level:

```cmake
add_subdirectory(vendor/execution)
```

Do not use:

```txt
FetchContent
vcpkg
find_package for this dependency
install-time discovery
git subtree
```

## Smoke test

Add a minimal build-only or unit test that includes the Beman Execution header used by the project.

Use the actual include spelling exposed by the vendored repository after inspecting it.

The smoke test should not implement the Scheme sender backend yet.

## Merge criteria

Run:

```bash
make compile
make test
make lint
```

All pass.

`handoff.md` records the exact Beman Execution target names and include spelling discovered during integration.

---

# Step 29 — Sender adapter over Beman Execution

## Goal

Add the project sender adapter backed by Beman Execution.

The adapter exists to keep SchemePoC backend code stable and readable.

It is not a portability layer for older C++ standards or non-GCC16 compilers.

## Files

```txt
src/smd/schemepoc/sender_adapter.hpp
src/smd/schemepoc/sender_adapter.test.cpp
```

## Required behavior

Expose project operations corresponding to:

```txt
just
then
let_value
when_all
sync_wait or equivalent completion bridge
```

Use Beman Execution as the implementation.

Do not use a mock sender as the primary implementation.

A tiny local fake may be used inside tests only if it helps test adapter-independent code, but the adapter itself must target Beman Execution.

## API sketch

```cpp
namespace smd::schemepoc {

struct sender_adapter {
    template <class T>
    constexpr auto just(T value) const;

    template <class S, class F>
    constexpr auto then(S sender, F f) const;

    template <class S, class F>
    constexpr auto let_value(S sender, F f) const;

    template <class... S>
    constexpr auto when_all(S... senders) const;

    template <class S>
    auto sync_wait(S sender) const;
};

inline constexpr sender_adapter sender_v{};

}
```

The exact return types should be deduced.

Do not type-erase senders unless a later step proves it is necessary.

Do not introduce `std::any`.

Do not introduce `std::move_only_function`.

## Tests

Test a tiny sender chain equivalent to:

```txt
just(41) then +1 -> 42
```

Use Catch2.

## Merge criteria

Run:

```bash
make compile
make test
make lint
```

All pass.

---

# Step 30 — Sender backend over CPS program using Beman Execution

## Goal

Compile the defunctionalized CPS program to Beman Execution sender graphs through the project adapter.

## Files

```txt
src/smd/schemepoc/sender_backend.hpp
src/smd/schemepoc/sender_backend.test.cpp
```

## Mapping

```txt
literal -> sender_v.just(value)
lookup  -> sender_v.just(env.lookup(name).value()) initially
call    -> sender_v.when_all(function_sender, arg_senders...)
           -> sender_v.then(... apply ...)
```

Use the project adapter from Step 29.

Do not bypass the adapter in the Scheme compiler backend.

Do not use `std::execution`.

Do not use `any_sender`.

Do not use `std::any`.

## Test

Compile and run:

```scheme
(+ 1 (* 2 3))
```

Expected result:

```txt
7
```

## Optional Beman Task rule

If implementation genuinely needs coroutine `task`, stop and document the need in `handoff.md`.

Then add a separate step to vendor:

```txt
https://github.com/bemanproject/task
```

as:

```txt
vendor/task
```

using git submodule and `add_subdirectory`.

Do not add Beman Task speculatively.

## Merge criteria

Run:

```bash
make compile
make test
make lint
```

All pass.

---

# Step 31 — Optional Beman Task integration, only if needed

## Goal

Add Beman Task only if Step 30 proves task/coroutine support is required.

## Rule

Skip this step if Beman Execution is sufficient.

## Required commands

```sh
git submodule add https://github.com/bemanproject/task vendor/task
git submodule update --init --recursive
```

## CMake

```cmake
add_subdirectory(vendor/task)
```

Do not use FetchContent, vcpkg, install-time discovery, or git subtree.

## Merge criteria

Run:

```bash
make compile
make test
make lint
```

All pass.

---

# Step 32 — Reflection spike

## Goal

Add an experimental C++26 reflection spike.

C++26 and GCC16 are baseline, so this is not optional for compiler-standard compatibility reasons.

It remains isolated because reflection is not needed for the first closure or sender backend.

## File

```txt
src/smd/schemepoc/reflection_reify.hpp
```

## Experiment

Given a compile-time description of a closure environment, generate an aggregate type with fields for captures.

## Rules

```txt
Reflection objects do not cross into runtime.
Reflection generates ordinary aggregate/storage shapes.
The core evaluator and closure backend must not depend on this spike yet.
```

## Merge criteria

Run:

```bash
make compile
make test
make lint
```

All pass.

---

# Step 33 — Documentation consolidation

## Goal

Turn the project state into execution docs.

## Files

```txt
docs/architecture.md
docs/godbolt.md
docs/steps.md
docs/limitations.md
```

Use one sentence per line in Markdown.

Do not hard-wrap prose.

## Required content

State clearly:

```txt
C++26 and GCC16 are baseline.
Tests use Catch2.
Parser is a reader, not an elaborator.
Arena tree is first implementation.
Generic Fix<F> exists separately.
CPS is the semantic center.
Closure backend is the stable demo.
Sender backend uses Beman Execution through the project adapter.
Beman dependencies are git submodules under vendor/.
Beman Task is added only if needed.
Reflection is a reifier, not the source of executable behavior.
```

## Merge criteria

Docs match implementation and tests.

---

# 10. Recommended merge rhythm

Visible milestones:

```txt
Step 8  -> constexpr reader for nested Scheme datums
Step 11 -> end-to-end compile-time arithmetic evaluator
Step 18 -> compile-time compiled runtime executable closure object
Step 23 -> lexical closures
Step 27 -> Godbolt demo
Step 28 -> FFI spike
Step 29 -> Beman Execution vendored and integrated
Step 31 -> sender-shaped backend through Beman Execution
Step 33 -> reflection spike
```

The plan intentionally gives repeated green milestones instead of one giant speculative branch.

---

# 11. Agent handoff template

Each completed step should leave `handoff-next.md` in this form:

```markdown
# Next step: Step N

## Goal

<one paragraph>

## Files expected to change

```txt
<paths>
```

## Context from previous step

<facts the next agent needs>

## Required implementation details

<bullet list or small code sketches>

## Required tests

<bullet list>

## Required commands

```bash
make compile
make test
make lint
```

## Do not do

- Do not proceed to Step N+1.
- Do not introduce optional dependencies unless this step requires them.
- Do not change architecture decisions without documenting the reason in `handoff.md`.
```

---

# 12. Important design constraints to preserve

## Reader vs elaborator

The reader parses data.

The elaborator assigns program meaning.

Do not collapse these layers.

## Applicative parser

The parser combinator layer should remain independent from Scheme semantics.

The parser representation must support captures.

Do not use raw function pointers for parser objects.

## Typeclass object pattern

Typeclass interfaces are records of named operations.

Use variable-template-selected lookup objects where appropriate.

Do not bake typeclass instances into core data structures unless the header is the designated adapter.

## Fixpoint trees

The first production path uses fixed-capacity arenas.

The generic `Fix<F>` machinery is introduced separately as a playground.

Do not prematurely refactor the working compiler onto `Fix<F>`.

## CPS

CPS is the semantic center.

A defunctionalized CPS program is the durable compiled representation.

The closure backend materializes an executable wrapper.

The sender backend maps CPS to Beman Execution through the project adapter.

## Sender backend

Use Beman Execution.

Do not use `std::execution`.

Do not use `any_sender`.

Do not use `std::any`.

Do not use `std::move_only_function`.

Do not vendor Beman Execution through FetchContent, vcpkg, install-time discovery, or subtree.

Use a git submodule under `vendor/execution`.

Add Beman Task only if needed.

## Reflection

Reflection is isolated.

Reflection generates aggregate/storage shape.

Reflection does not generate arbitrary new lambda syntax.

Reflection objects do not cross to runtime.

---

# 13. End state

The completed proof of concept should support:

```scheme
(+ 1 (* 2 3))
(if #t 1 2)
((lambda (x) (+ x 1)) 41)
((lambda (x) ((lambda (y) (+ x y)) 2)) 40)
'foo
```

The completed proof of concept should provide:

```txt
C++26/GCC16 baseline
Catch2 tests
constexpr reader
constexpr elaborator
constexpr direct evaluator
constexpr CPS compiler
runtime-executable closure program produced at compile time
Beman Execution sender backend
Beman Task only if needed
reflection spike
Godbolt demo
architecture docs
agent handoff docs
```

The core demo should remain small enough to show in Compiler Explorer.
