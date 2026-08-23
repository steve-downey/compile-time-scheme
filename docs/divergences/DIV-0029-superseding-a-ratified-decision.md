# DIV-0029: superseding a ratified decision

- **Status:** accepted-permanent
- **Date:** 2026-08-23
- **Step:** `tmp/plan/step-A0.md`
- **Authority diverged from:** process — this project had no stated mechanism for superseding a *ratified* decision record.

## What diverged

`tmp/plan/step-A0.md` needed to override `docs/cl-language-scoping.md`'s ratified D22, which makes parity with `smdlisp` the first phase and retiring `smdlisp` that phase's merge criterion.
`tmp/plan/` step A3 deletes `smdlisp` before that phase opens, which overrides D22's sequencing.
No existing mechanism in this repository covers overriding a decision that has already been ratified by merge.
`docs/divergences/README.md`'s taxonomy classifies deviations from ANSI Common Lisp, the active plan, or a project rule; every one of its precedents is decision-on-decision written into a plan document, not a divergence record superseding a decision — D11 retiring D1, D21 relaxing what D11 carried forward.
DIV-0004 and DIV-0016's own "supersedes" sections supersede other divergence records, never a decision.

## Why

Filing the override as a divergence record would have classified a deliberate change of course as a deviation from authority, which is a category error, and would have hidden the override from anyone reading the decision list rather than the divergence list.

## Consequences

The pattern this record establishes: a decision that overrides an earlier ratified one is written as a new, numbered decision record, appended to the note that owns the decision being overridden, naming it explicitly and stating which part of it survives.
`docs/cl-language-scoping.md`'s dated amendment of 2026-08-23 does exactly this for D22, in decision D32.
A step whose plan later needs to override a ratified decision follows the same shape rather than reaching for a divergence record.

## Revisit condition

None. This is a record of a convention, not an open question.
