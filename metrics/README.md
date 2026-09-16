# Metrics

Calibration data from completed agent fan-outs. One file per kind of run,
JSON Lines, append-only.

## What this is for

A plan that decomposes work into steps for cleared agents has to guess how big
a step is. The first such plan in this repository had nothing to guess
against — `tmp/plan/README.md` says so outright, under "No prior calibration
exists", and asks its integration review to read the run as the baseline being
measured rather than as a test of estimates that were never calibrated.

This directory is where that answer lives after the plan directory it came
from is gone. `tmp/plan/` is working state for one run and is deleted or
replaced; the numbers outlive it.

## What this is not

- **Not build telemetry.** Nothing here is produced by a build, a test run, or
  CI. Every row is written by hand by an agent finishing a step, and the
  integration review is the only thing that reads them.
- **Not a dashboard.** There is no tooling over these files and none is
  proposed. They are read by a person, or by an agent asked a specific
  question.
- **Not authoritative about the code.** A row records what a step cost, not
  what it did. `git log` is the history.

## Files

| File | Run | Rows |
|---|---|---|
| `fanout-runs.jsonl` | the tree-retirement and parser-combinator fan-out, steps A0–A5 and B1–B8 plus `typeclass-resync`, 2026-08 to 2026-09 | 19 |

## Reading a row

Fields follow `tmp/plan/metrics.jsonl`'s shape: `step`, `outcome`,
`wall_seconds`, `attempts`, a `verify` object, a `diff` object, an
`out_of_scope` array, and a free-text `note`.

Three cautions, all of them things the first run got wrong and recorded
anyway:

- **`wall_seconds` is not comparable across steps without reading the note.**
  One row spans several apparent sandbox clock-date rollovers and says so.
  Another times only the verify-and-merge phase, because no timer was started
  before the edits.
- **A `verify.wall_seconds` is warm or cold depending on the step**, and the
  difference is most of the number. A fresh worktree is a cold build because
  `.build/` lives inside the source tree.
- **An `outcome` of `blocked` is kept deliberately.** One step appears twice.
  Deleting the blocked row would hide that the step took two attempts, which
  is the more useful fact.

The conclusions drawn from the first run are in
`tmp/plan/INTEGRATION-REVIEW.md` § 4 while that directory exists, and in the
project's memory directory after it does not.

## Adding a run

Append a new file rather than extending an old one, and add its row to the
table above. A run's rows are only comparable to each other; the cold-build
cost, the test population and the toolchain all move between runs.
