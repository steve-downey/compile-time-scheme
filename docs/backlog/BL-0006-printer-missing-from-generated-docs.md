# BL-0006: `src/smd/cl/printer/` is missing from the generated-documentation roots

- **Status:** open — fell between two steps' scopes at Phase A; no owner
- **Date:** 2026-08-23
- **Origin:** Phase A close-out. Step A3's file predicted "A4 adds
  `../src/smd/cl/printer` after this step merges"; step A4's file never
  mentions `docs/mrdocs.yml` or `docs/antora.yml` at all, so no step owned it.
  A4 declined to act on the prediction rather than widen its declared scope,
  which was the correct call, and flagged it instead.
- **Frozen-tree impact:** none — both retired trees were deleted at A3 (D32).

## What

A3 repointed the generated-documentation configuration off `smdscheme` and
onto the live tree. A4 then created `src/smd/cl/printer/` — a new pipeline
component with a public header — and neither file learned about it.

Two one-line additions:

- `docs/mrdocs.yml`, the `input:` list — add `../src/smd/cl/printer`.
- `docs/antora.yml`, `ext.cpp-reference.using-namespaces` — add
  `smd::cl::printer`.

The two lists are parallel and are currently seven entries each; they must
stay in step, because the namespace list is what Antora resolves against what
mrdocs extracted.

## Why it is not done

Nothing is red. `mrdocs` is not installed in the development environment
(there is no `.tools/` directory and it is not on `PATH`), so `make docs`
cannot run locally at all, and the CI job in `.github/workflows/docs.yml` will
build successfully — it will simply produce documentation with no printer in
it. A silent omission in a generated artefact nobody reads locally is exactly
the kind of thing that stays broken, which is why it is written down rather
than left as a known-but-unrecorded gap.

## Evidence

Verified on `main` at the Phase A merge:

- `docs/mrdocs.yml`'s `input:` holds seven roots: `kit/foundation`,
  `cl/foundation`, `cl/symbol`, `cl/reader`, `cl/core`, `cl/elaborator`,
  `cl/eval`. `cl/printer` is absent.
- `docs/antora.yml`'s `using-namespaces` holds the seven matching namespaces.
  `smd::cl::printer` is absent.
- `src/smd/cl/printer/` exists and contains `prin1.hpp`, a public header in a
  `FILE_SET HEADERS` of the `cl.printer` target.
- `mrdocs` is not installed here, so the CI docs job's actual output is
  unverified locally — A3 recorded the same limitation.

## Open questions

- Should `src/smd/cl/sender/` and `src/smd/cl/conformance/` be in these lists
  too? Neither is there now. A3's step file prescribed the seven roots
  explicitly and did not include them, so their absence may be deliberate
  scoping of the documented surface rather than the same oversight. Settle
  this before editing, so the fix is one considered change rather than two.
- Is the generated documentation wanted at all in its current form? Nothing in
  the test matrix touches it and no one has read its output during this plan.

## Cost and risk

Two lines, plus whatever the answer to the first open question adds. Risk is
close to zero: neither file is compiled, imported, or read by
`make test-matrix`, `make lint`, or `scripts/verify-transclusions.sh`. The
only way to verify the result is to install `mrdocs` into `.tools/` and run
`make docs`, or to watch the CI job.

## Decision criteria

Worth scheduling as a few lines appended to whatever step next touches the
documentation configuration, or as part of BL-0005's install work. Worth
closing as `declined` if the answer to the second open question is that the
Antora/mrdocs output is not maintained — in which case the honest fix is to
retire the configuration, not to extend it.
