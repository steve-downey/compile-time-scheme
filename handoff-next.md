# Next steps: Common Lisp pivot, Steps L4 and L7 (parallel)

## Direction

The project is pivoting from Scheme-light to Common Lisp-light semantics.
Governance landed in Step L0.
The `smdlisp` skeleton landed in Step L1: `src/smd/smdlisp/` exists with a working `CMakeLists.txt`, `version.hpp`, `version.cpp`, `version.test.cpp`, wired into `src/smd/CMakeLists.txt`, and linking the `smdscheme.foundation` and `smdscheme.parser` targets successfully.

The full rationale, reuse inventory, gap analysis, decision records (D1-D10), orchestration protocol, and step plan are in:

```txt
docs/cl-pivot-plan.md
```

Read that file, plus `handoff.md`, before doing anything.

## What exists already

- `docs/cl-pivot-plan.md` — the plan.
- `docs/divergences/TEMPLATE.md` — divergence issue doc skeleton.
- `docs/divergences/DIV-0001-single-package-and-case.md` — first accepted ANSI divergence (D2/D7).
- `checklist.md` — Steps L0 and L1 are ticked; L2 onward are open.
- `handoff.md` — has the durable pivot facts plus the 2026-07-18 "Step L1: smdlisp skeleton landed" section recording the actual CMake target names.
- `src/smd/smdlisp/{CMakeLists.txt,version.hpp,version.cpp,version.test.cpp}` — the skeleton.

## Actual CMake target names from L1 (read before adding subdirectories)

```txt
smdlisp.smdlisp   -- STATIC library target defined in src/smd/smdlisp/CMakeLists.txt
smdlisp_test      -- Catch2 test executable, globs *test.cpp recursively under src/smd/smdlisp
```

`smdlisp.smdlisp` currently links `PUBLIC` against `smdscheme.foundation` and `smdscheme.parser`.
This target name pattern (`smdlisp.<subdir>`) is the one to follow when L4-L21 add subdirectories (`reader/`, `macroexpand/`, `elaborator/`, `closure/`, `sender/`): each subdirectory should define its own `smdlisp.<subdir>` target (e.g. `smdlisp.reader`), mirroring `smdscheme.foundation`, `smdscheme.parser`, `smdscheme.reader`, etc., and the top-level `src/smd/smdlisp/CMakeLists.txt` will eventually need `add_subdirectory(...)` calls added for each, plus `target_link_libraries(smdlisp.smdlisp PUBLIC smdlisp.<subdir> ...)` wiring, matching how `smdscheme.smdscheme` links `smdscheme.sender` and `smdscheme.reflection` today (see `src/smd/smdscheme/CMakeLists.txt`).

Both L4 and L7 below will need to *add* a subdirectory under `src/smd/smdlisp/` and a corresponding `add_subdirectory(...)` line in `src/smd/smdlisp/CMakeLists.txt`.
Since these two steps run in parallel in separate worktrees, expect a merge step to reconcile `src/smd/smdlisp/CMakeLists.txt` edits from both branches (both only add lines, so the conflict should be mechanical) — this is an orchestrator concern, not something either worker needs to solve alone.

## Next steps: L4 and L7 (parallel, independent of each other)

Per plan section 9, these two steps both depend only on L1 (done) and are explicitly independent of each other — the plan's parallelism summary (section 9, "Parallelism summary") puts L4 on Track A (code spine: L1 -> L4 -> L5 -> L6 -> ...) and L7 on Track B (values: L7 -> L8 -> L9, starting right after L1, parallel to L4-L6).
Dispatch them as separate workers in separate worktrees.

### Step L4 — CL lexical layer

Goal: `src/smd/smdlisp/reader/cl_chars.hpp` (+ test): CL constituent/terminating character predicates, case folding of a single char, `;`-comment skipping integrated with intertoken space.
Reuses `smdscheme::parser::cursor` unchanged (do not copy or modify it); supplies CL predicates instead of the Scheme ones that live in `smdscheme`'s `parser/cursor.hpp` usage sites — this step only adds the new CL-flavored predicate header, it does not touch `smdscheme`.
Merge criteria: static_asserts for folding, comment skipping, delimiter set.
Dependencies: L1 (done).
Full spec: `docs/cl-pivot-plan.md` section 9, "Step L4 — CL lexical layer".

### Step L7 — CL value model

Goal: `src/smd/smdlisp/closure/value.hpp` (+ test), adapted from the Scheme value model (`src/smd/smdscheme/closure/value.hpp`, read-only reference, do not edit): kinds `nil`, `integer`, `symbol`, `keyword`, `builtin`, `closure`, `foreign_function` (cons arrives later, in L8 — do not add it now).
One truthiness function, `constexpr auto is_true(value const&) -> bool` — `nil` is the sole false value (decision D3) — used by every later `if` site; do not reintroduce the Scheme per-site `#f` encoding.
Merge criteria: static_asserts for truthiness of `nil`, `0`, the `t` symbol, and keywords.
Dependencies: L1 (done); runs parallel with L4-L6.
Full spec: `docs/cl-pivot-plan.md` section 9, "Step L7 — CL value model".

## Standing constraints (apply to both steps)

- `src/smd/smdscheme/**` is frozen for semantic changes (decision D1); blog phases 5-12 transclude live code from it by UUID anchor.
  Neither L4 nor L7 should edit anything under `src/smd/smdscheme/**`; they only read it as a reference pattern or link against its existing targets.
- New work goes in `src/smd/smdlisp/`, namespace `smd::smdlisp`, one sub-namespace per directory (`smd::smdlisp::reader` for L4, `smd::smdlisp::closure` for L7), per plan section 7.
- Keep C++26 and GCC16 as the baseline; tests use Catch2; mirror the file prolog / include guard / canonical-include conventions already used by `src/smd/smdlisp/version.hpp` and by the corresponding `smdscheme` component.
- Each worker adds exactly one new subdirectory under `src/smd/smdlisp/` and one new `add_subdirectory(...)` line in `src/smd/smdlisp/CMakeLists.txt`; do not touch the other step's subdirectory.
- File a divergence doc under `docs/divergences/` (using `TEMPLATE.md`) for anything done differently than the plan specifies, or any knowing ANSI CL deviation.
- Before handoff, run and report: `make compile`, `make test`, `make lint`.
- Do not continue past the assigned step (L4 or L7) into later steps unless blocked; if blocked, document the blocker here instead.

## Leftover pre-pivot cleanup (background, low priority)

Carried over, still not addressed as of L1:

- Audit for remaining legacy `smd::schemepoc` references where `smd::smdscheme` is intended.
- Verify `docs/compiler_architecture.org` transclusions still point at intended files and UUID spans.
- Remove ad-hoc migration debugging scripts/logs not required for the build or docs flow.
- The root `CMakeLists.txt`'s `beman_install_library(schemepoc.schemepoc TARGETS ...)` list does not yet include any `smdlisp.*` target; revisit when `smdlisp` needs `make install`/`make testinstall` support (plausibly around L22).
