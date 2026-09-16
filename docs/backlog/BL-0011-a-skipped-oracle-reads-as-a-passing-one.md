# BL-0011: a skipped oracle is indistinguishable from a passing one

- **Status:** open
- **Date:** 2026-09-15
- **Origin:** found twice independently — by step B5, which merged green while the oracle was contributing nothing, and again by phase 38's blog drafter, which ran the check A5 had written to prove the oracle worked and found it could not run.
- **Frozen-tree impact:** none. `src/smd/cl/conformance/` is a live-tree directory.

## What

Decide how a run records whether the SBCL differential actually ran, and
against which SBCL. Three things would settle it, and they are separable:

1. Make a skip visible in the matrix summary, or assert a floor on the
   assertion count, so that "the oracle did not run" cannot be read as "the
   oracle agreed".
2. Record the oracle's version on **success**, not only on failure.
3. Pin, or at least record, the SBCL build that the exclusion list was
   measured against.

The files are `src/smd/cl/conformance/reader_differential.test.cpp`,
`sbcl_differential.test.cpp`, `sbcl_oracle.hpp`, and whichever CMake sets the
test properties.

## Why it is not done

The skip itself is correct and deliberate. `sbcl_oracle.hpp:85` says so: a
runtime probe rather than a build-time dependency, so the test degrades to a
skip in an environment without SBCL instead of failing to configure. That is
the right call and this item does not propose reversing it.

What has never been decided is the *reporting* question that the skip creates.
A skip is the correct behaviour and a silent skip is not, and nothing in the
plan that built this oracle distinguished the two.

## Evidence

**A skip scores as a pass.** No `SKIP_IS_FAILURE` is set anywhere in the tree
(`grep -rn 'SKIP_RETURN_CODE\|SKIP_IS_FAILURE'` over the CMake finds nothing
outside `_deps/`), so `catch_discover_tests`' default leaves a skipped case
counted in the passing total.

**Measured, both ways.** With `sbcl` on `PATH`, `*ReaderDifferential*` reports
`All tests passed (180 assertions in 3 test cases)`. With it hidden:
`test cases: 3 | 1 passed | 2 skipped`, `assertions: 1 | 1 passed`. Both are
green. The matrix line that a step records as its merge criterion —
`311/311, both legs` at Phase A's close — is compatible with forty-six
subprocess comparisons and with zero.

**The diagnostic that would say which only prints on failure.**
`reader_differential.test.cpp:201` is `INFO("oracle: " << *version)`. Catch2
emits an `INFO` only when an assertion in its scope fails, so a passing run
never names the build it agreed with. The comment at `sbcl_oracle.hpp:91` says
the version string "is what a reported divergence should be attributed to",
which is exactly right for a divergence and leaves agreement unattributed.

**It has already cost something.** SBCL was present when A4 and A5 built the
differential and recorded agreement against **SBCL 2.2.9.debian**. It was
absent from this machine by step B5, which merged with both matrix legs green
and the oracle contributing nothing. It was reinstalled before B6 at **SBCL
2.6.0.debian** — a different build from the one every exclusion was measured
against — and the differential still passes at 180 assertions. That is a good
outcome and nothing in the code establishes it: no test, no comment and no
recorded number ties the exclusion list to a version.

**The one check that would have caught the gap could not run.** A5's step file
spot-checks the differential with `./.build/*/*/cl_conformance_test
"*ReaderDifferential*" 2>/dev/null | tail -5`. The binaries are six directory
levels down, not two, so the glob matches nothing, the shell's complaint goes
to `/dev/null`, and `tail` reads an empty stream and exits 0. See
[BL-0012](BL-0012-plan-spot-checks-ship-unexecuted.md), which is the general
case of that.

## Open questions

- Is an assertion-count floor the right shape, or a property on the test? A floor is one line and states the real invariant ("this test does work"); a property is more idiomatic CMake and less direct.
- Should a missing oracle be a failure in CI and a skip locally? That splits the environments the skip exists to accommodate, which may be the honest answer rather than a hack.
- Is `INFO` on success wrong to want — should the version go in a `WARN`, a `SUCCEED` message, or a recorded artifact rather than Catch2 output at all?
- Does pinning a version mean requiring it, or only recording it? Requiring it re-introduces the build-time dependency the probe exists to avoid.
- `sbcl_differential.test.cpp` has the same shape and is not separately assessed here.

## Cost and risk

Small for (1) and (2) — an assertion count and a message. Larger for (3),
because "pin" and "record" are different decisions with different costs, and
the exclusion lists in `reader_differential.test.cpp` and A4's three traps
would each need re-checking against whatever build is named.

The risk of leaving it is not that the oracle breaks. It is that a green
matrix keeps meaning less than it appears to, in the one place this project
has an outside authority at all.

Re-verify: both matrix legs with SBCL present and absent, and the
`*ReaderDifferential*` and `*Sbcl*` filters in each state.

## Decision criteria

Schedule (1) and (2) with whatever step next edits `src/smd/cl/conformance/`;
they are a few lines and the argument is written. (3) wants deciding on its
own, and the C-series is the consumer that will care, because its corpus
expectations come from the same oracle.
