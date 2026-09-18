# BL-0015: per-step number reservations

- **Status:** open
- **Date:** 2026-09-17
- **Origin:** the third open question of the step-slug work that gave `checklist.md`'s later series slugs (done 2026-09-17, on the branch `step-series-slugs`), which asked whether that work covered the reservations too. It did not; they are the same defect one level out and want deciding on their own.
- **Frozen-tree impact:** none. Numbering and process only.

## What

The reader-combinator fan-out reserved a divergence number and a blog phase
per step before any step ran. The steps then did not need the divergence
numbers: DIV-0030 through DIV-0033 were never written, and `docs/divergences/`
now runs 0029, 0034. The blog phases were all spent, but only because every
step got a post; a step that had merged without one would have left the same
kind of hole in `docs/blog/pins.md`.

Decide whether a plan may reserve serial numbers per step at all, or whether a
number is taken only at the moment the record is written — and, if reservation
stays, whether an unspent reservation is released, tombstoned, or left.

## Why it is not done

It was found in the middle of the run that produced it, and the fix to the
numbering it sits inside (`checklist.md`'s series slugs) was the more urgent
half. Deciding it needs the divergence README's rules to change, which is a
process ruling, not a docs edit.

## Evidence

`ls docs/divergences/` shows `DIV-0029-…` followed by `DIV-0034-…`; nothing in
the directory or its README says the four between were reserved rather than
lost. The reservation is recorded only in `tmp/plan/README.md`'s step table,
which is the working state of a finished run and not on any reading path.

## Open questions

- Does `docs/divergences/README.md` need to say a gap is not a missing record, or should the gap be closed by tombstone files?
- Is a blog phase number different, given that `docs/blog/pins.md` already records phases pinned retroactively by construction?

## Cost and risk

Tiny either way: a paragraph in a README, or four one-line tombstones. The
risk is only in leaving it, which teaches the next plan to reserve again.

## Decision criteria

Rule on it before the next fan-out plan is written, since that is when the next
reservations would be made.
