# Next steps: Common Lisp pivot, Steps L4 and L7 (parallel)

> **2026-07-18 update:** L4, L7, and L8 are now all landed (see the `DONE`
> markers below and `handoff.md`'s corresponding dated sections).
> The next unchecked steps per `checklist.md` are **L5** (Track A: CL atoms,
> depends on L4, done) and **L9** (Track B: Lisp-2 environment, depends on
> L8, done) — independent of each other per the plan's parallelism summary
> (section 9), so they can again run as two parallel workers in two
> worktrees.
> L9 in particular should start by reading L7's and L8's `handoff.md`
> entries for the `closure<Core>::captured` raw-pointer/`MaxBindings==16`
> situation the L9 sketch (plan section 9) explicitly asks it to
> parameterize.
> The rest of this file below is the (now historical, but still accurate
> for what it describes) L4/L7 dispatch note; it is left in place rather
> than rewritten, annotated only where its content went stale.

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

### Step L4 — CL lexical layer — DONE (2026-07-18, in worktree cl-pivot/l4-cl-chars)

Landed: `src/smd/smdlisp/reader/{cl_chars.hpp,cl_chars.test.cpp,CMakeLists.txt}`, `smdlisp.reader` CMake target, wired into `src/smd/smdlisp/CMakeLists.txt`.
See `handoff.md` "2026-07-18 Step L4: CL lexical layer landed" for the exposed predicate names and a naming hazard worth reading before L5/L6 touch this header: the cursor-consuming skip function is named `skip_cl_intertoken_space`, not `skip_intertoken_space`, to avoid a permanent ADL ambiguity against `smdscheme::parser::skip_intertoken_space` (same `cursor` argument type). Apply the same care to any new `smdlisp` function taking a `smdscheme::parser::cursor` by value.
`make compile`/`make test` (295/295)/`make lint` all green at landing; `src/smd/smdscheme/**` untouched.
No divergence doc was needed for this step (predicate shape and comment/case-fold behavior follow the plan directly).

### Step L7 — CL value model — DONE (2026-07-18, in worktree cl-pivot/l7-cl-value)

Landed: `src/smd/smdlisp/closure/{value.hpp,value.test.cpp,CMakeLists.txt}`, `smdlisp.closure` CMake target, wired into `src/smd/smdlisp/CMakeLists.txt`.
See `handoff.md` "2026-07-18 Step L7: CL value model landed" for the value-kind list and, importantly, the raw-pointer captured-env constraint in `closure<Core>` (`env<Core, 16> const *captured`) that L9 must address when it builds real `env`.
`make compile`/`make test` (296/296)/`make lint` all green at landing; `src/smd/smdscheme/**` untouched.

### Step L8 — Cons cells and list builtins — DONE (2026-07-18, in worktree cl-pivot/l8-cons-cells)

Landed: `src/smd/smdlisp/closure/value.hpp` gained `pair_ref`/`pair_cell`/`pair_heap` (adapted by copy from Scheme's PR #26) and `pair_ref` as a new `value<Core>`/`foreign_function<Core>::val_t` alternative; new files `src/smd/smdlisp/closure/{pairs.hpp,pairs.test.cpp}`, both wired into the existing `smdlisp_closure_headers` FILE_SET (no new CMake target — `pairs.hpp` joins `value.hpp` under `smdlisp.closure`).
See `handoff.md` "2026-07-18 Step L8: cons cells and list builtins landed" for the full API surface: `enum class list_op { cons, car, cdr, list, null, eq, eql, atom }` and `apply_prim<Core, MaxPairs>(list_op, std::span<value<Core> const>, pair_heap<Core, MaxPairs>*) -> foundation::result<value<Core>>`.
Read it before L9 or L10 touch pairs: it records the CL deltas (`nil`, not a separate `null_t`, is the empty list; `car`/`cdr` of `nil` is `nil`; predicates return the symbol `T` or `nil`, not `bool`) and points at `docs/divergences/DIV-0002-eq-eql-conflated.md` (new this step: `eq`/`eql` are implemented identically, same simplification Scheme made for `eq?`/`eqv?`, currently unobservable since there are no numeric types beyond `int`).
`list_op` is a free-standing enum, not reused from an `elaborator::prim_op` — `smdlisp` has no elaborator yet (L10). Whichever step adds a `core_prim`-equivalent node should map its primitive-symbol table onto `list_op` directly rather than reinventing the op set.
`make compile`/`make test` (316/316, including 11 new `PairsTest` cases)/`make lint` all green at landing; `src/smd/smdscheme/**` untouched (the `pairs.test.cpp` glob hit under `src/smd/smdscheme/` in build logs is a pre-existing, unrelated file at that path, not something this step created or touched).

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
