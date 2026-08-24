# BL-0005: export `kit` and `cl`, and turn `make testinstall` green

- **Status:** open — deliberately postponed out of the tree-deletion plan
- **Date:** 2026-08-20
- **Origin:** `tmp/plan/` (the tree-retirement / parser-combinator fan-out). The
  original decomposition folded this into the step that repointed the
  examples and the install test; the owner split it out and postponed it
  because `make testinstall` is already red on `main` for an unrelated,
  pre-existing reason and turning it green is not a gate on anything else in
  the plan.
- **Frozen-tree impact:** none. Everything this item touches is
  `src/smd/kit/**`, `src/smd/cl/**`, `src/examples/`, `installtest/`, and the
  top-level `CMakeLists.txt` — all live tree.

## What

Three things, done together because they are one deliverable (an installable,
install-tested live package). Note that **none** of them is red any more, and
that is the problem rather than progress — see "Why it is not done":

1. **A new example**, `src/examples/godbolt_cl.cpp`, replacing the eleven
   `smdscheme`/`smdlisp` examples the deletion step removed outright (it did
   not replace them — see below). It should read, elaborate and evaluate a
   `smd::cl` form at compile time and pin the result with a `static_assert`,
   the way `godbolt_arithmetic.cpp` and `godbolt_lisp.cpp` did for the two
   retired trees. `src/smd/cl/conformance/driver.hpp`'s `evaluate_program` is
   almost certainly the entry point to use rather than assembling the
   pipeline by hand.
2. **Export `kit.foundation` and the `cl.*` targets** from the top-level
   `beman_install_library(schemepoc.schemepoc TARGETS …)` call. The tree
   deletion step left that list holding only `smd.fixpoint` — enough to keep
   `make install` configuring, nothing more — because a live-tree target
   goes into the export list only alongside install-test coverage that
   proves it is reachable, and that coverage is exactly what this item adds.
   Whether `cl.conformance` belongs in the list is a judgement call: it pulls
   in `sbcl_oracle.hpp`, which shells out to a subprocess, and an installed
   package that cannot run the conformance harness is a defensible scope in
   a way that one that fails to configure is not.
3. **Re-point `installtest/`** at the live trees. The tree-deletion step
   deleted the six `smdscheme`-only install-test files outright rather than
   replacing them, so `installtest/CMakeLists.txt`'s `add_installtest` helper
   currently has no callers. Write `installtest/test_kit_foundation.cpp`,
   `installtest/test_cl_reader.cpp`, and `installtest/test_cl_eval.cpp` —
   three install tests, not unit tests: each proves that a slice of the
   installed include tree is reachable and self-contained by compiling
   against it, the way the six retired files did for `smdscheme`. Update
   `add_installtest` to link the right target instead of `smd::smdscheme`,
   and check rather than assume whether `-freflection` was arriving
   transitively through `smd::smdscheme` — the existing comment says
   everything linked it "so they transitively pick up the `-freflection`
   flag and the full sub-library include tree", and the `cl.*` targets may
   not carry that flag at all, which would be fine (reflection lived in
   `smdscheme/reflection/` and left with it) but is worth confirming rather
   than assuming.

## Why it is not done

`make testinstall` was red on `main` before the tree-retirement plan ran,
independent of that plan:

```
.install/include/smd/smdscheme/closure/cps_code.hpp:6:10:
    fatal error: smd/smdscheme/closure/pairs.hpp: No such file or directory
```

**That failure is gone, and it did not get fixed.** Step A2 shrank the
top-level export list to `smd.fixpoint` alone and deleted the six install
tests; step A3 deleted the header the error names. `make testinstall` now
exits **0** — it configures against a package that no longer claims a
`smd::smdscheme` target, finds nothing to link, and passes with **zero
installed tests**. A red signal became a green one by having nothing left to
check, which is strictly worse than the failure it replaced: the failure was
visible, and this is not. Anyone reading a green `make testinstall` between
now and this item being scheduled is reading a vacuous truth.

`pairs.hpp` was never added to `smdscheme.closure`'s installed `FILE_SET`, so
the installed package has been unusable for as long as nobody ran the check.
Deleting the tree that caused this removes the symptom but not the underlying
gap it exposed: the live tree, `src/smd/cl/`, has never had installed-package
coverage either — no `cl.*` or `kit.*` target was ever in the export list.

The tree-deletion plan's own acceptance gate is `make test-matrix`, which does
not run `make testinstall` (they are different CMake projects; see
`docs/verification-matrix.md`). Fixing installed-package coverage is real
work — a new example, an export-list decision including the judgement call on
`cl.conformance`, and three new install tests — and none of it is required to
keep the deletion's own acceptance gate green. Bundling it into the deletion
step would have made the plan's largest, riskiest step also its most
speculative one. Splitting it out lets the deletion land on its own evidence
and leaves this as a positive deliverable with its own evidence once
scheduled.

## Evidence

- `make testinstall` measured red on `main` before the tree-deletion plan
  ran, exit 2, ~110 s, ~7.4 MB log of template diagnostics — see
  `tmp/plan/metrics.jsonl`'s row-zero baseline.
- `installtest/CMakeLists.txt`'s `add_installtest` helper links every test to
  `smd::smdscheme`; `grep -c add_installtest installtest/CMakeLists.txt`
  before the deletion step showed six calls, all against files that include
  only `smdscheme` headers.
- The top-level `CMakeLists.txt`'s `beman_install_library` call named eight
  `smdscheme.*` targets and `smd.fixpoint` before the deletion step, and zero
  `cl.*` or `kit.*` targets — confirmed by reading the file directly.

## Open questions

- Whether `cl.conformance` belongs in the export list, given it shells out to
  a subprocess for its SBCL oracle.
- Whether `-freflection` needs to travel with any `cl.*` target, or whether
  it left the tree entirely with `smdscheme/reflection/`.
- Whether three install tests (`kit.foundation`, `cl.reader` + `cl.printer`,
  `cl.eval`) are the right slice, or whether the elaborator and the sender
  backend want their own, mirroring the six the retired trees had.

## Cost and risk

One new example, an export-list edit, three new install-test files, and an
`add_installtest` signature change. `make testinstall`'s own log is large
(megabytes of template diagnostics) but the check itself is cheap to run
(~110 s) and is the whole verification surface — no semantics in the live
tree change. The risk is the same class of bug that caused the pre-existing
failure: a `FILE_SET HEADERS` list missing a header that only the installed
(not the in-tree) include path would catch, which is exactly what
`make testinstall` exists to find.

## Decision criteria

Schedule it once the parser-combinator work in `tmp/plan/` Phase B has
landed, so the new example and install tests are written against the reader
the project actually wants to demonstrate rather than against a shape B1–B8
are about to change underneath them. Close it as declined only if the
project decides installed-package coverage is not worth maintaining for a
proof-of-concept repository, which is not currently expected.
