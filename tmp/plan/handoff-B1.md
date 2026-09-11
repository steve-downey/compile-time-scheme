# Gate note for the owner (Phase A close-out)

Phase A ran in this order: freeze (A1: `smdscheme` and `smdlisp` tagged
`iteration/smdscheme-final`/`iteration/smdlisp-final`, their architecture
prose moved to a pinned history doc) → neutralise (A2: dead-tree build
consumers removed — examples, install test, export list, and
`reader/oracle_compare.test.cpp`, whose 28 checks were confirmed already
covered elsewhere before deletion) → delete (A3: `smdscheme` and `smdlisp`
removed from trunk outright, executing decision D32) → printer (A4: a
`prin1`-shaped printer for `cl` datums, proved against a real SBCL in the
same step) → differential (A5, this step: A4's eleven cases broadened into a
41-case corpus plus a 5-case error table, closing the gap A3's deletion
opened). `cl-retire-trees` is ready for `main`; that merge and Phase B's
branch creation are the orchestrator's, not a worker's.

**What's yours to confirm.** Decision D32 (written at A0, executed at A3)
retires `smdlisp` to a tag *before* the parity phase, rather than as that
phase's merge criterion — overriding the sequencing of the ratified D22 it
sits beside in `docs/cl-language-scoping.md`. What D32 deferred, not
discarded: roughly 120 evaluated-behaviour cases `smdlisp` holds for `LET`,
`SETQ`, `CATCH`, `TAGBODY`, `DEFMACRO`, and multiple values, none of which
the corpus's 75 entries reach. They're not gone — `iteration/smdlisp-final`
keeps them readable (`git show iteration/smdlisp-final:<path>`) for any
later C-series lane that wants source programs to work from. Whether
deferring that evidence rather than reaching parity first was the right
call is the judgement this gate exists for.

---

# Handoff to B1

## Sizing baseline

Phase A's final state (`cl-retire-trees`, merge commit `dcea38b`, plus a
one-line follow-up `48b6502` ticking root `checklist.md`'s Step A5 line,
which A5's own worktree diff missed): **311 ctest entries per leg, both legs
100% passing**, cold `make test-matrix` wall time **159s** (fresh worktree,
submodules initialised, no prior `.build`). Size every Phase B step against
these two numbers, not against A4's (304 entries, 159s) or A0's baseline row
in `metrics.jsonl`.

## The parser combinator layer exists only at a tag now

`src/smd/smdscheme/parser/` (the combinator layer this phase's B2–B4 steps
build `smd::kit::parser` from) is not in the worktree at all — it left with
the rest of `smdscheme` at A3. It survives at `iteration/smdscheme-final`:

```sh
git show iteration/smdscheme-final:src/smd/smdscheme/parser/parser.hpp
```

or to see the whole directory:

```sh
git ls-tree -r --name-only iteration/smdscheme-final -- src/smd/smdscheme/parser/
```

B2 is copying from a frozen tag, not refactoring a live path — there is
nothing under `src/smd/smdscheme/` to edit or delete, only to read at that
revision. DIV-0028 (`docs/divergences/README.md`) already records that the
combinator layer has no `cl` client yet; Phase B is what gives it one.

## One item A3 could not verify locally, still unowned

`docs/mrdocs.yml`'s `input:` list does not include `src/smd/cl/printer/`.
A3's step file anticipated A4 adding that root when the printer landed; A4's
own step file never mentioned `mrdocs.yml`, so nobody has picked it up.
It was out of A5's declared scope too. I have no further information on
Antora-page verification beyond this one `mrdocs.yml` gap — that is the
extent of what was passed to me at this step. Whether it blocks anything in
Phase B, or is just an accuracy debt to flag at the next docs-focused step,
is not something A5 could determine from its own file scope.

## A recurring pattern worth knowing about before you read your own step file

Six step-file inaccuracies were found and reconciled across A1–A4 before
this step; A5 found a seventh: the spot-check command
`./.build/*/*/cl_conformance_test` (two wildcard levels) does not match the
actual build layout, which is
`.build/build-gcc-16/src/smd/cl/conformance/<Config>/cl_conformance_test`
(four levels: toolchain dir, then the full source-relative path, then the
per-config subdirectory). Verify a spot-check command actually finds its
target with `find .build -name <binary>` before trusting a `*/*` glob in a
step file.
