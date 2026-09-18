# Fan-out checklist

Work the **first unchecked** line. Mark your own line done before you finish,
editing this file **in the main checkout by absolute path**, never inside your
worktree. This is the fan-out's step tracker; the repository root
`checklist.md` is a different file and a project deliverable.

Each step is named by its slug, given on its line below and in its step file's
title. The ordinal (A0, B5) is reading order and the step file's name, nothing
more: the root `checklist.md` carries a blog-backfill series whose B5–B8 are
other work, so a bare "B5" is never an identity.

From A1 onward you also tick your step's line in the **root** `checklist.md`,
inside your worktree, as part of your diff. A0 is what puts those lines there.
The two files are not redundant: this one drives the fan-out, that one is how
the repository describes itself to the next agent that reads `AGENTS.md`.

## Phase 0 — reconciliation, the first step of [tree-retirement](../../checklist.md#tree-retirement)

Integration branch: **`plan-reconciliation`, branched off `main`, and it merges
to `main`**, not to a phase branch. This is the only step in the plan that does.
See `tmp/plan/README.md` "Integration branches" for why.

- [x] A0 — record-reconciliation — `tmp/plan/step-A0.md` — rebase the record against reality: the root checklist, decision D32, three backlog items, and a dated amendment to each scoping note

## Phase A — [tree-retirement](../../checklist.md#tree-retirement)

Retire the three-tree structure.

Integration branch: **`cl-retire-trees`**, created off `main` by the
orchestrator **after A0 merges**. The older `retire-three-trees` branch points
at `1ace4ad` and is nine commits stale; it is not this plan's branch. If your
branch does not exist, that is the gate, not a defect: write `blocked-A1.md`.

The deletion runs third, not last — see `tmp/plan/README.md` "The reorder" for
why. A1 and A2 exist only to make A3 a clean subtraction; A4 and A5 restore
reader-layer differential evidence afterward rather than gating the deletion.

- [x] A1 — freeze-iterations-as-tags — `tmp/plan/step-A1.md` — freeze both iterations as tags; move their architecture prose to a pinned history doc
- [x] A2 — neutralise-build-consumers — `tmp/plan/step-A2.md` — neutralise the dead trees' build consumers (examples, install test, export list)
- [x] A3 — delete-dead-trees — `tmp/plan/step-A3.md` — delete `smdscheme` and `smdlisp` from trunk, executing D32
- [x] A4 — datum-printer — `tmp/plan/step-A4.md` — a `prin1`-shaped printer for `cl` datums, proved against SBCL in the same step
- [x] A5 — reader-differential-corpus — `tmp/plan/step-A5.md` — broaden the SBCL reader differential into a real corpus

**Gate.** Phase A merges to `main` and the owner reviews before Phase B opens.
Phase B's integration branch does not exist yet; the orchestrator creates
`cl-parser-combinators` off `main` after that merge. If you are the B1 agent and
that branch is absent, that is the gate, not a defect: write `blocked-B1.md`.

## Phase B — [reader-combinators](../../checklist.md#reader-combinators)

Adopt parser combinators in `cl`'s reader.

Integration branch: `cl-parser-combinators` (created after the Phase A gate).

- [x] B1 — split-read-header — `tmp/plan/step-B1.md` — split `read.hpp` into component headers plus an umbrella (mechanical, zero behavioural diff)
- [x] B2 — parser-monad — `tmp/plan/step-B2.md` — `smd::kit::parser`: a Monad instance over a context-threaded parser, with `read_radix_number` as its first client
- [x] B3 — intertoken-space — `tmp/plan/step-B3.md` — intertoken space and comments onto the layer
- [x] B4 — choice-and-repetition — `tmp/plan/step-B4.md` — choice and bounded repetition; `read_delimited` onto the combinator layer
- [x] typeclass-resync — `tmp/plan/step-typeclass-resync.md` — carry `main`'s typeclass rename onto the Phase B branch (inserted 2026-09-10; out-of-band maintenance, not one of the fourteen)
- [x] B5 — text-literals — `tmp/plan/step-B5.md` — text: strings and character literals
- [x] B6 — forms-and-tokens — `tmp/plan/step-B6.md` — forms: the quote family and token data
- [x] B7 — readtable-dispatch — `tmp/plan/step-B7.md` — sharpsign dispatch and `read_node`
- [x] B8 — combinator-integration — `tmp/plan/step-B8.md` — integration: the D30 measurement, DIV-0028 dissolved, architecture section, project checklist

## Close-out

- [x] INTEGRATION-REVIEW — `tmp/plan/INTEGRATION-REVIEW.md` — Opus, non-code, after B8
