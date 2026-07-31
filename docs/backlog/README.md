# Backlog

Identified-but-unscheduled work.
One file per item, `BL-NNNN-short-title.md`, written from `TEMPLATE.md`.

## What this is for

An item lands here when it is a real, argued idea that nobody has committed to doing.
The point is to keep such ideas out of two places they do not belong.

**Root-level plan files.**
A `plan-*.md` at the repo root reads as an active plan.
`plan-refactor-arena-box.md` sat there for two months after its content had already shipped, unreferenced by any doc, still naming the pre-pivot `src/smd/schemepoc/` paths.
Anything not on `checklist.md` is not a plan.

**`TODO` comments in source.**
`AGENTS.md` forbids vague TODOs.
A backlog file is where the argument goes instead, so the code stays clean and the reasoning survives.

## What this is not

- **Not a plan.**
  `checklist.md` is status; `docs/cl-rebuild-plan.md` is the active plan.
  Nothing here is scheduled until an item says which step took it, and an item may be closed as declined.
- **Not an agent read path.**
  Not in any tier of the `AGENTS.md` reading contract.
  A worker agent working a step should never open this directory; it is read when *deciding* what to schedule, not when executing.
- **Not a divergence record.**
  `docs/divergences/` records what the implementation *did* differently from an authority, and classifies each as `defect` or `scope-decision`.
  Backlog items record what the implementation has *not done yet*.

## Rules

- State each item's blast radius against the current freezes.
  D1's freeze on `src/smd/smdscheme/**` is retired by D11; `src/smd/smdlisp/**` is frozen from step R1 onward as the rebuild's behavioural oracle.
- Refine an item in place.
  Do not append status logs to it.
- Promoting an item: add a step to `checklist.md` and the plan, then record the step in the item's status line.
  Delete the file once the step merges — `git log` is the history.
- An item nobody will do should be closed as `declined` with a reason, not left open indefinitely.

## Items

| ID | Title | Status |
| --- | --- | --- |
| [BL-0001](BL-0001-elaborator-as-fold-fix-algebra.md) | Rewrite the elaborators as `fold_fix` algebras | scheduled — R1 (delete `fold_fix`) and R4 (elaborator as traversal) |
| [BL-0002](BL-0002-compile-time-arena-sizing.md) | Size the node arena from a compile-time measurement | open — unblocked by D14, revisit in R1 |
