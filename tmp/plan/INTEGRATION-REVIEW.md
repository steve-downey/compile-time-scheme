# Integration review — the final step, and not a code step

**Run this on a stronger reasoning tier (Opus), not on a cleared Sonnet
worker.** It is the one step that is allowed to read broadly: the full diff
across both phases, `git log`, the step files it is judging, the living
architecture document, and `metrics.jsonl`. That breadth is a deliberate,
bounded exception — it happens once, not once per step — and it is why the
worker contract can stay as tight as it is.

It is a **judgement read, not a rerun of the GREEN checks**. Every step already
left both matrix legs passing. Re-running them proves nothing new; what no
single cleared agent could see is whether fourteen locally-correct steps add up
to one coherent change.

Write your findings into this file, replacing this brief.

---

## 1. Cross-step drift

Read the whole diff of both integration branches. Look for the residue of a
worker that worked around a problem instead of reporting it:

- **Two types doing one job, and a converter between them.** The plan's hardest
  rule is *never fork a shared abstraction*, and the tell is a nonce type with a
  good name. `parse_state`, `parse_result`, `cursor`, `result` and `child_list`
  are the ones with forking pressure on them.
- **An abstraction used by everything after step N and nothing before it.** That
  is an amendment that arrived as a rewrite instead of as an `amendment-NN.md`.
- **Naming inconsistency across the kit's parser layer.** It was built one
  primitive per step by six different agents; check that `skip_many` and the
  collecting repetition, and `alt` and `operator|`, are named on one principle
  rather than six.
- **A step-2 assumption invalidated by step-7.** Specifically: is
  `parse_context` still doing work by B7, or did it end up decoration? B2's step
  file warned that a vacuous concept would be worse than none. B3 was asked
  whether `readtable const` needed anything of it that B2 did not put there.
  Follow that thread.

An invalidated early assumption surfacing **here** rather than as a live
`amendment-NN.md` means the amendment mechanism failed. Say so explicitly if it
did; that is more useful than the finding itself.

Note also the inverse. A run with **no** amendments is not evidence of a good
decomposition — check whether nonce types appeared instead, or whether a step
quietly widened its scope rather than proposing a change to an earlier one.

## 2. Did the context discipline hold

- No file on the mandatory read path grew into a cumulative log. Check every
  `handoff-NN.md` that survives: forward-only, ≤ ~150 lines, no measurements, no
  "what I did" summary. A handoff that reads like a log had the wrong contents
  and the next worker paid for it.
- No step file told an agent to read `README.md`, `metrics.jsonl`, another
  step's file, or an earlier handoff.
- **Every transclusion still resolves.** `scripts/verify-transclusions.sh` exits
  0, and `docs/compiler_architecture.org` is the living document it checks
  against the worktree. Fourteen steps moved, split and deleted files; a rename
  silently orphans a transclude and turns the living document into dead
  narrative. **A1** is the step that restructured this most (it moved 31 of
  the document's 37 transclusions out to a pinned history document, ahead of
  A3's deletion); check its work last, against the final tree, not against
  the tree it ran on.
- The two `checklist.md` files stayed distinct: `tmp/plan/checklist.md` never
  appears in any step's diff.

## 3. The claims each phase made, checked as a whole

**Phase A claimed nothing was lost.** Verify:

```sh
git tag --list 'iteration/*'
git show iteration/smdscheme-final:src/smd/smdscheme/parser/parser.hpp | head -3
git show iteration/smdlisp-final:src/smd/smdlisp/smdlisp.hpp | head -3
```

Both tags resolve, and `docs/history/architecture-iterations.org` pins to them.

**Phase B claimed no observable behaviour changed.** That is D31 and it is
checkable:

```sh
git diff --stat $(git merge-base main cl-parser-combinators)..HEAD \
    -- 'src/smd/cl/**/*.test.cpp' 'src/smd/cl/conformance/'
git diff $(git merge-base main cl-parser-combinators)..HEAD \
    -- 'src/smd/cl/reader/token.hpp' 'src/smd/cl/reader/number.hpp'
```

The first should be empty or near-empty; investigate anything in it. The second
must be **exactly** empty — `scan_token` and `classify_number` were out of scope
for the entire series, and that is what makes DIV-0003 structurally safe rather
than merely tested. A change there is the most serious finding available.

Also read the diagnostic strings across the whole series. Every message in
`read.test.cpp`'s `reports_errors` chain is produced by rewritten code now.
Grep the final tree for each and confirm the position it reports at.

## 4. The measurements

You are the **only** consumer of `metrics.jsonl`. Read it and produce, here:

- Per-step and whole-run totals: wall time, verify wall time as a share of it,
  verify log bytes, attempts, diff size.
- **Which steps ran hottest, and why.** A large diff and a cheap edit gated by
  an expensive verify loop are different problems with opposite fixes: the first
  wants splitting, the second wants a narrower verify command or a cheaper
  baseline — and splitting it would make things worse by paying the verify cost
  more times. This repository is squarely the second case, so say so with
  numbers rather than in general.
- Whether step sizing held. **There was no prior calibration**, so this run is
  the baseline rather than a test of one; frame it that way. The question is
  what a step costs here and where the guesses missed, not whether estimates
  were met.
- Every `out_of_scope` entry. A repository that keeps provoking the same
  one-line fix should show up here as something to fix properly rather than
  repeatedly work around.

Three rows are worth naming individually:

- **B1** is the measured floor: pure code motion, no logic. Everything else
  costs at least that.
- **A0** and **A1** are documentation-only changes that could not possibly move
  a test and paid the full matrix anyway. A0 in particular touches no file
  under `src/`. If either cost as much as a code step, the per-step verify
  command is too coarse, and that is the single most actionable finding this
  run can produce — more actionable than anything in section 1.
- **A2**, **A3**, and **B8** each ran extra commands beyond the standard
  `make test-matrix` (`make testinstall` for A2 and A3 — recorded for the
  trend, not gating either step; the D30 before-and-after builds for B8) and
  should not be read as ordinary rows.

## 5. Promote and distil

**Raw rows — out of `tmp/`.** `tmp/plan/` is disposable; a log left there is not
durable and the next run wipes it. Append this run's records to
`metrics/fanout-runs.jsonl` in the repository, tagged with a run identifier and
the integration branch. **Append across runs, never overwrite** — one run's
numbers are an anecdote and the whole value is in comparison. That file may grow
without bound: nothing reads it on a hot path.

**Conclusions — one living memory entry.** Write the distilled calibration to

```
/home/sdowney/.claude/projects/-home-sdowney-src-steve-downey-compile-time-scheme-compile-time-scheme-git/memory/
```

as a `project`-type memory: what a step actually costs *in this repository*.
Verify cost is a property of the repository, not of the planning template, and
only the per-repo figure is actionable.

Capture what changes a future decision:

- The measured floor for one step, once the verify loop is paid.
- Which verify commands dominate, and whether a cheaper subset still catches
  regressions. Note in particular whether the ~440-second cold-worktree
  `make test-matrix` (re-measured 2026-08-20; row zero was 409 s against an
  earlier commit) could be replaced for most steps by a warm build or a
  narrower ctest selection, and what that would give up.
- The split between edit cost and verify cost, since that decides whether
  splitting a hot step helps or hurts.
- What Phase A's deletion did to the numbers: 1123 ctest entries before (not
  1118 — `metrics.jsonl`'s second baseline row is the one this run's steps
  were actually sized against), and whatever **A3** recorded after. That is
  a rare clean before-and-after on a build's
  cost and it should be in the memory entry.
- The amendments: which step's design was revised, prompted by which step, and
  what the revision cost. That is a better measure of decomposition quality than
  any token count — an amendment at step 7 against a step-2 decision says
  precisely how far apart an abstraction and its first real consumer were
  placed. This plan deliberately put each parser primitive in the same step as
  its first consumer; whether that produced fewer amendments than the layered
  alternative would have is the most interesting thing this run can teach.

**Update that entry in place; do not add a new one per run.** One memory file,
one fact, revised as evidence accumulates. Add the one-line `MEMORY.md` pointer
the first time it is created and leave it alone after.

## 6. What the owner needs, in the report

- **Whether the governance change is stated consistently in all six of its
  homes.** It is decision **D32**, written at A0 into
  `docs/cl-language-scoping.md`'s amendment section beside the ratified D22
  whose sequencing it overrides, and executed at A3. Check `AGENTS.md`,
  `CLAUDE.md`, the root `checklist.md`, `docs/backlog/README.md`'s Rules
  section, `docs/cl-rebuild-plan.md` § 8's dated resolution, and D32's own
  execution note. Six documents, one rule retired; any one of them still
  asserting the `smdlisp` freeze is a finding.
  Watch specifically for a worker that followed this plan's **earlier draft**,
  which had A3 author the retirement as "D22" in `docs/cl-rebuild-plan.md` § 2.
  If a second D22 exists anywhere, that draft leaked through, and it is a
  collision with a ratified decision rather than a cosmetic one.
- **Whether D32's deferral held up.** D32 did not discard `smdlisp`'s oracle
  value; it deferred the harvest to the C-series lanes and left the source
  programs at `iteration/smdlisp-final`. Confirm the tag resolves and that D32
  carries the standing lane obligation in checkable words. This is the one
  decision in the run that a later phase, not this one, will prove right or
  wrong — say what would show it was wrong, so the C-series planner has it.
- **Reconcile the two divergence lists against `docs/cl-parser-scoping.md`.**
  A0 appended an amendment saying how this plan differed from § 4's sketch;
  B8's handoff says how execution differed from this plan. They are different
  lists with different authors and the owner amends the note from both.
  Reconcile them into one rather than emitting two.
- The pre-existing rot named at the end of `README.md` — `schemepoc.org` and the
  two scripts hardcoding `src/smd/schemepoc/` — which nothing in this run
  touched and which should become backlog entries.
