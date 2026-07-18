# Next steps: Common Lisp pivot, Steps L4 and L7 (parallel)

> **2026-07-18 update:** L4, L7, and now **L5 are all DONE**. Track A's next
> unblocked step is **L6** (CL datum reader, depends on L5, done); Track B's
> next unblocked step is **L8** (cons cells and list builtins, depends on L7,
> done). See the "Step L5 — DONE" entry below, next to the L4 entry, and
> `handoff.md`'s "2026-07-18 Step L5: CL atoms landed" section for what L5
> actually exposes. The rest of this file's framing (written when L4/L7 were
> dispatched together) is left as historical context; do not re-run L4/L7/L5.

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

### Step L5 — CL atoms — DONE (2026-07-18, in worktree cl-pivot/l5-cl-atoms)

Landed: `src/smd/smdlisp/reader/{atom.hpp,atom.test.cpp}`, added to the existing `smdlisp.reader` target's `FILE_SET` (no new CMake target).
`atom = std::variant<atom_integer, atom_symbol, atom_keyword>`; `atom_keyword` is a distinct kind from `atom_symbol` per D7 (spelling stored without the leading `:`). Both hold a `folded_name` (own fixed storage, not a `string_view` into source — folding per D2 rewrites characters). Public parsers: `integer_p()`, `symbol_p()`, `keyword_p()`, `atom_p()` (tries them in that order), and `read_atom(std::string_view)` as the convenience entry point (`read_atom("foo")` → symbol `FOO`; `read_atom(":bar")` → keyword `BAR`). `t`/`nil` are not special-cased; they read as ordinary symbols `T`/`NIL`.

**Read before touching this file again, especially for L6:** integer recognition is NOT greedy-digit scanning (that was the initial port from `smdscheme::reader::integer_p` and it failed `make test` on `read_atom("1+")`, since CL symbols may start with a digit). The fix, and the reason, is `docs/divergences/DIV-0003-atom-maximal-munch.md` — atoms are read as a whole maximal-munch token first, then classified as integer-or-symbol. L6's datum reader should call `atom_p()` (or `read_atom` for whole-string cases) rather than reimplementing token scanning, or it will likely reintroduce the same bug.

`make compile`/`make test` (333/333, including 37 new `AtomTest` cases)/`make lint` all green at landing; `src/smd/smdscheme/**` untouched (`git diff -- src/smd/smdscheme` empty).
One divergence doc filed: `docs/divergences/DIV-0003-atom-maximal-munch.md` (plan sketch "same rules as Scheme side" for integers didn't survive contact with CL's digit-permissive symbol syntax; category 2 divergence — step implemented differently than the plan specifies).

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
