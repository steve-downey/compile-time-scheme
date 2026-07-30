# Backlog

Identified-but-unscheduled work. One file per item, `BL-NNNN-short-title.md`,
written from `TEMPLATE.md`.

## What this is for

An item lands here when it is a real, argued idea that nobody has committed to
doing. The point is to keep such ideas out of two places they do not belong:

- **Root-level plan files.** A `plan-*.md` at the repo root reads as an active
  plan. `plan-refactor-arena-box.md` sat there for two months after its content
  had already shipped, unreferenced by any doc, still naming the pre-pivot
  `src/smd/schemepoc/` paths. Anything not on `checklist.md` is not a plan.
- **`TODO` comments in source.** `AGENTS.md` forbids vague TODOs. A backlog file
  is where the argument goes instead, so the code stays clean and the reasoning
  survives.

## What this is not

- **Not a plan.** `checklist.md` is status; `docs/cl-pivot-plan.md` is the active
  plan. Nothing here is scheduled, and an item may be closed as declined.
- **Not an agent read path.** Not in any tier of the `AGENTS.md` reading
  contract. A worker agent working a step should never open this directory; it is
  read when *deciding* what to schedule, not when executing.
- **Not a divergence record.** `docs/divergences/` records what the
  implementation *did* differently from an authority. Backlog items record what
  the implementation has *not done yet*.

## Rules

- Every item states its blast radius against the D1 frozen-tree rule
  (`src/smd/smdscheme/**` is frozen for semantic changes). An item that requires
  a frozen-tree edit needs a divergence doc and orchestrator sign-off *before*
  it can be scheduled, and must say so.
- Refine an item in place. Do not append status logs to it.
- Promoting an item: add a step to `checklist.md` and the plan, then set the
  item's status to `scheduled` with the step number. Delete the file once the
  step merges — `git log` is the history.
- An item nobody will do should be closed as `declined` with a reason, not left
  open indefinitely.

## Items

| ID | Title | Status |
| --- | --- | --- |
| [BL-0001](BL-0001-elaborator-as-fold-fix-algebra.md) | Rewrite the elaborators as `fold_fix` algebras | open |
| [BL-0002](BL-0002-compile-time-arena-sizing.md) | Size the node arena from a compile-time measurement | open |
