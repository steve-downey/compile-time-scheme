# BL-0013: two step series share one numbering in `checklist.md`

- **Status:** open — scheduled for after the Phase B merge, on the owner's direction
- **Date:** 2026-09-16
- **Origin:** found while drafting the phase 39 post, which needed to say which "Step B1" it meant and could not do it from the file alone.
- **Frozen-tree impact:** none. Documentation and cross-references only.

## What

`checklist.md` carries two independent step series under one numbering. The
blog-backfill section runs Step B5 through Step B11; the reader-combinator
section, about forty lines below it, runs Step B1 through Step B8. **Step B5,
B6, B7 and B8 each name two different pieces of work in the same file.**

Give both series slugs, keep the ordinals as reading order, and convert the
cross-references in `docs/history/blog-backfill-plan.md` and `tmp/plan/` to
anchors — the shape `CLAUDE.md` prescribes, `### Stage 3 — [slug](#slug)`,
carrying both and cross-referencing by slug only.

**Do this after the Phase B merge, not before.** Every `Step BN` line in the
reader-combinator section is ticked on `cl-parser-combinators` and unticked on
`main`, because each step ticked its own line inside its own worktree. Editing
those lines on `main` first puts a conflict on every line of the series.

## Why it is not done

Sequencing, and only sequencing. The work is mechanical and the destination is
already decided.

## Evidence

Four live collisions in one file: `checklist.md:119-125` against
`checklist.md:140-147`. The two series are reachable from different documents —
the backfill from `docs/history/blog-backfill-plan.md`, the other from
`tmp/plan/` — so a reader arriving from either has no signal about which one a
bare "Step B5" means.

**The same run demonstrated the alternative working.** When trunk moved under
the reader-combinator branch mid-run, a step was inserted between the fourth
and fifth: it was named `typeclass-resync`, took no number, and nothing
renumbered. Every existing reference to the steps after it stayed valid, at the
moment the plan was under the most pressure. A number would have forced a
choice between lying about the ordering, sorting to the wrong end, and
renumbering four step files, thirteen handoffs, the metrics rows and the
reserved blog phases together.

**The ordinal failed a second way in the same run.** Divergence numbers 0030
through 0033 were reserved per step and went unspent, because the steps did not
need them. `docs/divergences/` now has a four-wide hole that reads as four
missing records rather than four numbers nobody wanted.

## Open questions

- Do the reader-combinator steps keep their ordinals as reading order, given that eight published posts now describe them in sequence? The posts do not name the steps under the rule that applies to them (see [BL-0014](BL-0014-posts-cite-internal-identifiers.md)), so the coupling may be weaker than it looks.
- `tmp/plan/` is working state for a finished run. Is converting its cross-references worth anything, or should the pass stop at `checklist.md` and `docs/`?
- The divergence and blog-phase reservations are the same defect one level out. Does this item cover them, or do per-step number reservations want deciding separately?

## Cost and risk

Small: one file restructured, two documents' references rewritten, no code.
The risk is doing it in the wrong order — see above.

Re-verify: `make lint` for markdown, and that every converted cross-reference
resolves to an anchor that exists.

## Decision criteria

Do it once the Phase B merge has landed its ticks. There is no case for
declining; the file is ambiguous today and will not become less so.
