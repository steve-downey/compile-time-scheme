# Fan-out checklist

Work the **first unchecked** line. Mark your own line done before you finish,
editing this file **in the main checkout by absolute path**, never inside your
worktree. This is the fan-out's step tracker; the repository root
`checklist.md` is a different file and a project deliverable.

## Phase A — retire the three-tree structure

Integration branch: `retire-three-trees` (already created, off `main`).

The deletion runs third, not last — see `tmp/plan/README.md` "The reorder" for
why. A1 and A2 exist only to make A3 a clean subtraction; A4 and A5 restore
reader-layer differential evidence afterward rather than gating the deletion.

- [ ] A1 — `tmp/plan/step-A1.md` — freeze both iterations as tags; move their architecture prose to a pinned history doc
- [ ] A2 — `tmp/plan/step-A2.md` — neutralise the dead trees' build consumers (examples, install test, export list)
- [ ] A3 — `tmp/plan/step-A3.md` — delete `smdscheme` and `smdlisp` from trunk (D22, ratified)
- [ ] A4 — `tmp/plan/step-A4.md` — a `prin1`-shaped printer for `cl` datums, proved against SBCL in the same step
- [ ] A5 — `tmp/plan/step-A5.md` — broaden the SBCL reader differential into a real corpus

**Gate.** Phase A merges to `main` and the owner reviews before Phase B opens.
Phase B's integration branch does not exist yet; the orchestrator creates
`cl-parser-combinators` off `main` after that merge. If you are the B1 agent and
that branch is absent, that is the gate, not a defect: write `blocked-B1.md`.

## Phase B — adopt parser combinators in `cl`'s reader

Integration branch: `cl-parser-combinators` (created after the Phase A gate).

- [ ] B1 — `tmp/plan/step-B1.md` — split `read.hpp` into component headers plus an umbrella (mechanical, zero behavioural diff)
- [ ] B2 — `tmp/plan/step-B2.md` — `smd::kit::parser`: a Monad instance over a context-threaded parser, with `read_radix_number` as its first client
- [ ] B3 — `tmp/plan/step-B3.md` — intertoken space and comments onto the layer
- [ ] B4 — `tmp/plan/step-B4.md` — choice and bounded repetition; `read_delimited` onto the combinator layer
- [ ] B5 — `tmp/plan/step-B5.md` — text: strings and character literals
- [ ] B6 — `tmp/plan/step-B6.md` — forms: the quote family and token data
- [ ] B7 — `tmp/plan/step-B7.md` — sharpsign dispatch and `read_node`
- [ ] B8 — `tmp/plan/step-B8.md` — integration: the D30 measurement, DIV-0028 dissolved, architecture section, project checklist

## Close-out

- [ ] INTEGRATION-REVIEW — `tmp/plan/INTEGRATION-REVIEW.md` — Opus, non-code, after B8
