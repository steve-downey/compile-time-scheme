# Next step: Common Lisp pivot, Step L1

## Direction

The project is pivoting from Scheme-light to Common Lisp-light semantics.
Governance for the pivot (checklist section, `handoff.md` durable facts, divergence-doc convention) landed in Step L0.
The full rationale, reuse inventory, gap analysis, decision records (D1-D10), orchestration protocol, and step plan are in:

```txt
docs/cl-pivot-plan.md
```

Read that file, plus `handoff.md`, before doing anything.

## What exists already

- `docs/cl-pivot-plan.md` — the plan.
- `docs/divergences/TEMPLATE.md` — divergence issue doc skeleton.
- `docs/divergences/DIV-0001-single-package-and-case.md` — first accepted ANSI divergence (D2/D7: single package, keywords only, uppercase-fold-only reader).
- `checklist.md` — has the "Common Lisp pivot" section; Step L0 is ticked, L1 onward are open.
- `handoff.md` — has the durable pivot facts (frozen `smdscheme` tree, `smdlisp` layout, D1-D10 by reference, divergence-doc convention, `nil` truthiness, one-shot exits).

## Next step: L1 — `smdlisp` skeleton

Per plan section 9, Step L1:

1. Create `src/smd/smdlisp/CMakeLists.txt`, `src/smd/smdlisp/version.hpp`, `src/smd/smdlisp/version.cpp`, `src/smd/smdlisp/version.test.cpp`.
2. Wire the new directory into `src/smd/CMakeLists.txt` via `add_subdirectory`.
3. Link the `smdlisp` target(s) against `smdscheme`'s `foundation` and `parser` CMake targets, via their canonical includes, to prove the dependency direction compiles.
   Do not copy `foundation`/`parser` source; consume them as-is (plan section 5, "Use as-is").
4. Follow the `smdscheme` component's own `version.hpp`/`version.cpp`/`version.test.cpp` as the pattern to mirror (same shape, `smd::smdlisp` namespace instead of `smd::schemepoc`).

Merge criteria: `make compile`, `make test`, `make lint` all green; `smdscheme` targets and sources untouched (verify with `git diff -- src/smd/smdscheme` showing nothing).

Dependencies: L0 (done).

## Standing constraints

- `src/smd/smdscheme/**` is frozen for semantic changes; blog phases 5-12 transclude live code from it by UUID anchor (decision D1).
  L1 must not edit anything under `src/smd/smdscheme/**`; it only links against its existing targets.
- New work goes in `src/smd/smdlisp/`, namespace `smd::smdlisp`, one sub-namespace per directory, reusing `smdscheme` `foundation` and `parser` as dependencies (not copies).
- Keep C++26 and GCC16 as the baseline; tests use Catch2.
- File a divergence doc under `docs/divergences/` (using `TEMPLATE.md`) for anything done differently than the plan specifies, or any knowing ANSI CL deviation.
- Before handoff, run and report: `make compile`, `make test`, `make lint`.
- Do not continue past L1 into L2+ unless L1 is blocked; if blocked, document the blocker here instead.

## Leftover pre-pivot cleanup (background, low priority)

Carried over from the namespace-migration stabilization; do opportunistically, not as a step:

- Audit for remaining legacy `smd::schemepoc` references where `smd::smdscheme` is intended.
- Verify `docs/compiler_architecture.org` transclusions still point at intended files and UUID spans.
- Remove ad-hoc migration debugging scripts/logs not required for the build or docs flow.
