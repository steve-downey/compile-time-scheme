# DIV-0034: `sbcl_differential.test.cpp` validates the corpus, not `cl`

- **Status:** accepted-permanent
- **Date:** 2026-08-23
- **Step:** `tmp/plan/step-A5.md`
- **Authority diverged from:** process — the file's name and header comment describe a `cl`-vs-SBCL differential; what it checks is narrower.

## What diverged

`src/smd/cl/conformance/sbcl_differential.test.cpp`'s `check_against_sbcl` compares SBCL's `prin1` output against `expected_sbcl_text(c)` — a string transcribed by hand into the corpus case itself (`src/smd/cl/conformance/corpus.hpp`) — and skips every case whose channel is not `expect_fixnum` or `expect_boolean`.
It never calls `evaluate_program` or any other `cl` evaluation entry point.
So the file validates that the corpus's own hand-transcribed expectations match what a real Common Lisp produces; it does not compare `cl`'s behaviour against SBCL's at all.
That is a real and worthwhile check — it is just not the one the file's name and header comment describe.

## Why

The file predates `smd::cl::printer` (step A4).
Before a printer existed, there was no way to render what `cl`'s own evaluator produced as a comparable string, so a `cl`-vs-SBCL differential over evaluated values was not buildable yet.
Comparing SBCL against the corpus's own transcribed text was the check available at the time, and it was written under a name that anticipated the check the project did not yet have the pieces to write.

## Consequences

`src/smd/cl/conformance/reader_differential.test.cpp` (A4, broadened at A5) is the differential the file's name promises, at the reader layer: it reads and prints with `cl`, reads and prints with SBCL via `sbcl_read_print`, and compares the two, with no evaluator involved on either side.
The evaluator layer still has no such differential — only the corpus check this record describes.
A future step wanting a genuine `cl`-vs-SBCL differential over evaluated values needs a driver that renders `cl`'s evaluated result (not just its read datum) and compares it against `sbcl_prin1` of the same form, which does not exist yet.
`sbcl_differential.test.cpp` keeps its name and its `TEST_CASE`s unchanged; a rename would churn CMake and every recorded ctest name for no behavioural gain.
Its header comment gains a paragraph pointing here.

## Revisit condition

None for this record itself — it documents what the file already does.
The gap it identifies (no evaluated-value differential against SBCL) closes if and when a later step builds one; that is new work, not a fix to this file.
