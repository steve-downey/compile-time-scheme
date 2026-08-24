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

- State each item's blast radius against the current tree.
  `src/smd/cl/**` is the only tree; `src/smd/smdscheme/**` and `src/smd/smdlisp/**` were deleted from trunk at step A3 (D32) and are frozen at the tags `iteration/smdscheme-final` and `iteration/smdlisp-final`.
- Refine an item in place.
  Do not append status logs to it.
- Promoting an item: add a step to `checklist.md` and the plan, then record the step in the item's status line.
  Delete the file once the step merges — `git log` is the history.
- An item nobody will do should be closed as `declined` with a reason, not left open indefinitely.

## Items

| ID | Title | Status |
| --- | --- | --- |
| [BL-0001](BL-0001-elaborator-as-fold-fix-algebra.md) | Rewrite the elaborators as `fold_fix` algebras | landed — R1 deleted `fold_fix`, R3 shipped the schemes, R4 the elaborator |
| [BL-0002](BL-0002-compile-time-arena-sizing.md) | Size the node arena from a compile-time measurement | closed — superseded by D14, checked 2026-08-23 |
| [BL-0003](BL-0003-implicit-block-elision-as-a-cata.md) | Elide a `defun`'s implicit block by a `cata`, not an evaluator case | open — R6 landed and did not force it; revisit condition restated 2026-08-23 |
| [BL-0004](BL-0004-retire-the-cl-foundation-forwarding-shims.md) | Retire the eighteen `cl/foundation` forwarding shims left by R8 and the Monad typeclass | open — for removal, but not while the kit boundary could still move |
| [BL-0005](BL-0005-export-live-trees-and-turn-testinstall-green.md) | Export `kit` and `cl`, re-point `installtest/`, turn `make testinstall` green | open — postponed out of the combinator plan; `testinstall` now exits 0 with zero installed tests, which is worse than red |
| [BL-0006](BL-0006-printer-missing-from-generated-docs.md) | Add `cl/printer` to `mrdocs.yml`'s input roots and `antora.yml`'s namespaces | open — fell between A3's and A4's scopes; nothing red, docs silently incomplete |
| [BL-0007](BL-0007-sender-v-comment-cites-a-retired-rule.md) | `cl/sender/sender_v.hpp`'s comment justifies itself by the never-edited rule D32 retired | open — one sentence; for whatever step next edits `cl/sender/` |
