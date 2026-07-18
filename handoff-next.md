# Next step: Common Lisp pivot, Step L0

## Direction change

The project is pivoting from Scheme-light to Common Lisp-light semantics.
The sender backend cannot support multishot `call/cc` (sender completion is one-shot by contract), and full `call/cc` is unsound in combination with `dynamic-wind`/resource cleanup, so the Scheme road stops here.
Common Lisp's nonlocal control is dynamic-extent and one-shot, which is exactly what CPS and sender backends can express soundly.

The full rationale, reuse inventory, gap analysis, decision records (D1–D10), orchestration protocol, and step plan are in:

```txt
docs/cl-pivot-plan.md
```

Read that file before doing anything.

## What exists already

- `docs/cl-pivot-plan.md` — the plan.
- `docs/divergences/TEMPLATE.md` — divergence issue doc skeleton.
- `docs/divergences/DIV-0001-single-package-and-case.md` — first accepted ANSI divergence.

## Recently landed Scheme work the plan builds on

Local `main` was 12 commits behind `origin/main` when the pivot plan was first drafted; it has been fast-forwarded.
The landed work (PRs #23, #25, #26, 2026-07-12) added `set!` + `begin` special forms across all backends, a `store` of mutable binding cells, `pair_heap`-based pairs and list primitives, quoted-list elaboration, and blog `phase-14-set-bang.org`.
Consequences, already folded into the plan: CL blog phases start at 15, step L8 adapts the pairs machinery, step L12 adapts the `set!`/store machinery.

## Next step: L0 — governance install

Per plan section 9, Step L0:

1. Append the "Common Lisp pivot" checklist section (plan section 10) to `checklist.md`.
2. Record the durable pivot facts (plan section 11) in `handoff.md`.
3. Rewrite this file for Step L1 (smdlisp skeleton).

## Standing constraints

- `src/smd/smdscheme/**` is frozen for semantic changes; blog phases 5–12 transclude live code from it by UUID anchor (decision D1).
- New work goes in `src/smd/smdlisp/`, namespace `smd::smdlisp`, reusing `smdscheme` `foundation` and `parser` as dependencies.
- Keep C++26 and GCC16 as the baseline; tests use Catch2.
- Before handoff, run and report: `make compile`, `make test`, `make lint`.

## Leftover pre-pivot cleanup (background, low priority)

Carried over from the namespace-migration stabilization; do opportunistically, not as a step:

- Audit for remaining legacy `smd::schemepoc` references where `smd::smdscheme` is intended.
- Verify `docs/compiler_architecture.org` transclusions still point at intended files and UUID spans.
- Remove ad-hoc migration debugging scripts/logs not required for the build or docs flow.
