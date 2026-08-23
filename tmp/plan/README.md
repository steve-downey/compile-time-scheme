# Execution plan: reconcile the record, retire the trees, adopt combinators

**Not for worker agents.** A cleared worker reads `AGENT-PROMPT.md`,
`checklist.md`, its own `step-NN.md`, and its own inbound handoff. This file is
for the orchestrator and for the owner. It is not on any worker's read path,
which is why it is allowed to be discursive.

Produced 2026-08-23. It **supersedes** the thirteen-step plan (A1–A5, B1–B8)
committed to `main` in PR #50 and recoverable from `git log`. Most of that plan
survives here, edited rather than regenerated: A1–A5 and B1–B8 are the same
steps, and the parts that changed are named in "What moved, and why" below. The
new material is one step at the front and one decision it settles.

Nothing in the superseded plan had executed. This is a re-decomposition against
a tree that moved under it, not a mid-run correction.

---

## The shape

Fourteen implementation steps in three phases, plus an integration review.

**Phase 0 is one step and it is new.** The repository stopped describing
itself. `AGENTS.md` says to work the next unchecked step in `checklist.md`;
that file is ticked through R8 and the blog backfill and then stops, so an
agent following the project's own protocol finds nothing to do. Four smaller
records came loose alongside it. A0 fixes all five in one pass and merges to
`main`.

| step | one line | blog | DIV |
|---|---|---|---|
| A0 | rebase the record: root checklist, decision D32, three backlog items, a dated amendment to each scoping note | 33 | 0029 |

**Phase A retires the three-tree structure.** Unchanged from the superseded
plan in shape and order, including its largest decision: **deletion runs third,
not fifth.**

| step | one line | blog | DIV |
|---|---|---|---|
| A1 | freeze both iterations as tags; move their architecture prose to a pinned history doc | 34 | 0030 |
| A2 | neutralise the dead trees' build consumers (examples, install test, export list) | 35 | 0031 |
| A3 | **delete `smdscheme` and `smdlisp` from trunk**, executing D32 | 36 | 0032 |
| A4 | a `prin1`-shaped printer for `cl` datums, proved against SBCL in the same step | 37 | 0033 |
| A5 | broaden the SBCL reader differential into a real corpus | 38 | 0034 |

**Phase B adopts parser combinators in `cl`'s reader.** Unchanged from the
superseded plan except for renumbering and two paragraphs in B8.

| step | one line | blog | DIV |
|---|---|---|---|
| B1 | split `read.hpp` into component headers plus an umbrella | 39 | 0035 |
| B2 | `smd::kit::parser`: a Monad *instance*, threaded context, `read_radix_number` as first client | 40 | 0036 |
| B3 | intertoken space and comments onto the layer | 41 | 0037 |
| B4 | choice and bounded repetition; `read_delimited` onto the combinator layer | 42 | 0038 |
| B5 | text: strings and character literals | 43 | 0039 |
| B6 | forms: the quote family and token data | 44 | 0040 |
| B7 | sharpsign dispatch and `read_node` | 45 | 0041 |
| B8 | close the series: the D30 measurement, DIV-0028 dissolved, the architecture section | 46 | 0042 |

Blog phases run 33–46 and divergence numbers DIV-0029 through DIV-0042, both
reserved **per step up front** so nothing can collide on the next free number.
An unused reservation is expected and fine. Every step ships a post (D20) and
no implementing agent drafts one; the anchors are what they owe.

Note the one number that looks like a collision and is not: **decision D32** is
written at A0, and **DIV-0032** is A3's reserved divergence number. Both step
files say so explicitly, because an agent that conflates them writes the
governance record into the wrong directory.

`step-A0.md` is ~540 lines against the template's ~400 target, and that is a
deliberate call rather than an oversight. It could be split into governance and
record-cleanup halves, but its verify is ~440 s of cold build that nothing it
does can affect, so splitting pays that twice for two prose edits — and the two
halves are coupled, because the § 6 amendment in one half prescribes the very
mechanism D32 uses in the other. Length was the cheaper cost. `step-B2.md` is
~490 for the ordinary reason: it is the densest step in the plan.

---

## What moved, and why

Everything below was verified against `main` at `1d66c54` on 2026-08-23, not
carried over from the superseded plan's prose.

### Carried forward unchanged

- **The reorder.** Deletion third, not fifth. The argument is the owner's and
  it still holds: agents keep getting hung up on trees they must not touch and
  doing strategically wrong things, sometimes silently, while the trees are
  there to be gotten hung up on. That cost is continuous; a sequenced
  replacement oracle's benefit is not. A1 and A2 precede the deletion only
  because `docs/compiler_architecture.org` is worktree-verified and the build
  names targets that stop existing.
- **Two integration branches for A and B, and the human gate between them.**
  See "Integration branches" below; the D31 argument is unchanged.
- **`make testinstall` stays red and is not an acceptance signal.** Already red
  on `main` for a pre-existing reason — `smdscheme.closure`'s installed
  `FILE_SET` is missing `pairs.hpp`. A2 and A3 measure it for the trend; it
  gates nothing. Turning it green is `docs/backlog/BL-0005-…`, after Phase B.
- **The three code findings.** Re-verified: the kit's `many<Capacity>` succeeds
  at capacity where `read_delimited` diagnoses `"too many elements"`;
  `skip_block_comment` lets an unterminated `#|` run to end of input where the
  natural combinator fails at the comment; and `scan_token`/`classify_number`
  are out of scope for the whole B series, which makes DIV-0003 structurally
  unbreakable rather than merely tested against.
- **Blog transclusions are not a deletion hazard.** `verify-transclusions.sh`
  resolves posts against their `blog/phase-NN` tag with `git show`, and only
  `docs/compiler_architecture.org` against the worktree. A1's move of the dead
  trees' prose to a pinned history document is the right and sufficient fix.

### Rewritten

- **The governance record is D32, not D22, and A0 writes it, not A3.** The
  superseded plan chose D22 on an explicit premise — `step-A5.md:265` called
  the other one "the different, unratified D22" — and that premise is now
  false. `docs/cl-language-scoping.md` merged in PR #48 and its D22 is
  ratified. D32 is confirmed free: it appears nowhere in `docs/`, in
  `checklist.md`, or as any live decision, only inside the superseded plan as a
  number reserved and then withdrawn.

  D32 lands in `docs/cl-language-scoping.md`, not `docs/cl-rebuild-plan.md`,
  for two reasons. The rebuild plan is closed by its own closing note and is
  the wrong place for a live decision. And an override belongs physically
  beside the decision it overrides, so nobody can read D22 without meeting it.

  Splitting authorship from execution also shrinks A3, which was this plan's
  largest and riskiest step by a wide margin — 30,000 lines deleted, five
  documents rewritten, and a docs-generation config nothing in the test matrix
  touches. Taking the governance argument out of it is worth doing on those
  grounds alone.

- **A1's transclusion count, and the grep that was answering a different
  question.** `docs/compiler_architecture.org` has **39** `[[file:`
  occurrences, which is what the superseded plan reported — but only **37** are
  code transclusions. The other two are plain document hyperlinks to
  `cl-limitations.md` with no `::<uuid>`, which `verify-transclusions.sh`
  ignores entirely. Of the 37: 31 point at code A3 deletes (7 `smdscheme`, 21
  `smdlisp`, 3 `godbolt_lisp`), 6 survive. The 31 held across every count.

  A1's spot check used `grep -c`, which counts matching *lines* and would have
  given a third answer again. It now counts occurrences with `grep -o | wc -l`
  and filters on `::`. The two hyperlinks get their own warning, because they
  both live in the section that moves, the history document is one directory
  deeper, and nothing verifies them — a UUID-less link is invisible to the
  checker that would otherwise catch it.

- **`retire-three-trees` is stale and is not this plan's branch.** It points at
  `1ace4ad`, **nine commits behind `main`**. A worker branching from it would
  silently lose the Monad typeclass, both scoping notes, and the
  `read_delimited` cleanup — and B2's entire premise is that the Monad
  typeclass exists. Phase A uses **`cl-retire-trees`**, created off `main`
  after A0 merges. A1's step file says this in bold and tells the agent to halt
  rather than substitute.

- **B8 no longer says `docs/cl-parser-scoping.md` is unratified.** It is, and
  A0 amends it. B8's instruction not to edit it survives with a different
  reason: A0's amendment says how this plan differs from § 4's sketch, and B8's
  handoff says how execution differed from this plan. Two lists, two authors.

- **Every step from A1 ticks its own line in the root `checklist.md`.** A0 puts
  the entries there. `AGENT-PROMPT.md` carries this as a standing addition to
  every step's declared file scope rather than thirteen edits to thirteen scope
  blocks. It is safe here because **this plan has no parallel lanes** — every
  step merges before the next branches — so a shared file is not a merge point.

### Dropped

- **The "Recommended amendments to `docs/cl-parser-scoping.md`" appendix.** In
  the superseded plan it was a list at the bottom of this README, owed to
  nobody in particular and read by nobody on a read path. It is now the body of
  A0 part 4, where an agent will actually execute it, plus the § 6 mechanism
  fix the old list did not have.
- **"No prior calibration exists."** It still doesn't, but see "Baseline".

---

## The decision this plan had to make: `smdlisp` as an evaluator oracle

The superseded plan retired `smdlisp` on the owner's authority and on the
friction argument, and the supporting evidence it cited covered only
`reader/oracle_compare.test.cpp` — 28 fixed literals, weak, inputs covered
elsewhere. Nobody had assessed `smdlisp`'s value as an **evaluator** oracle.
`docs/cl-language-scoping.md`'s ratified D22 rests on exactly that value:
"every step of the spine has, in `smdlisp`, a working reference implementation
of the same operator with the same reader in front of it — a cheaper and more
exact oracle than SBCL for exactly the semantics that are subtlest, because it
already made this project's choices."

Three options were open and none had been chosen. This plan measured first.

**What the measurement found.** `src/smd/cl/conformance/corpus.hpp` holds 75
entries — 35 adapted from `pfdietz/ansi-test`, 40 spec-derived. `satisfies()`
runs `evaluate_program()`, so the corpus can only reach what elaborates: `cl`'s
7 special operators and 18 builtins, and nothing else. `smdlisp` implements 19
special operators plus `defmacro`, six host macros and backquote, and its test
suite holds roughly **120 genuinely novel, source-in / value-out evaluated
cases** — 96 in `closure/eval_direct.test.cpp`, 7 in
`closure/closure_program.test.cpp`, about half of `macroexpand/expander.test.cpp`
— covering `LET`, `LET*`, `SETQ`, `LAMBDA`, `FUNCALL`/`APPLY`, `DEFMACRO` and
backquote, `CATCH`/`THROW`, `TAGBODY`/`GO`, `DEFVAR`/`DEFPARAMETER`, and
multiple values. **The corpus has none of it.** So D22's premise is real and
the "harvest first" option is not obviously wrong. The remaining ~230 of
`smdlisp`'s 350 relevant test cases are either internal-API-shape tests against
a `core_type` variant `cl` deliberately does not share, or `cps_code.test.cpp`'s
69 cases duplicating `eval_direct`'s coverage through a second backend.

**The decision: retire it now, and defer the harvest to its consumer.** Not
because the friction argument outweighs the oracle argument — that framing is
what made this look like a close call — but because of what `smdlisp` can and
cannot supply.

It can supply **source programs**. It cannot supply **expectations**. D16 says
correctness comes from external authority; D22's own second limit says that
where `smdlisp` pins a `defect` it is "a wrong answer by design"; D26 says a
lane's entries are "derived from the specification and checked against SBCL,
not written afterwards to describe what was built." Taking `smdlisp`'s answers
as corpus expectations would need a third `provenance_kind` meaning "this
project's earlier implementation agreed", which puts "this implementation
agrees with itself" back into the one artefact R6 built to remove it. SBCL is
at `/usr/bin/sbcl` and the oracle harness already runs, so every harvested
entry would have to be re-adjudicated against SBCL anyway — at which point
`smdlisp` contributed the source string and nothing else.

And a tag supplies source strings losslessly, for free, forever. After A1,
`git show iteration/smdlisp-final:src/smd/smdlisp/closure/eval_direct.test.cpp`
prints all 96 cases. Harvesting them **now** would land ~120 entries for
operators that do not elaborate — either a red build, which violates this
plan's hardest structural rule, or inert data, which is what the tag already
is. It is also the layered-abstraction anti-pattern: define everything in step
1, first use it in step 9, maximise the guesses validated too late.

So D32 records the retirement **and** a standing obligation on every C-series
lane: *before writing a lane's conformance entries, read the corresponding
`smdlisp` test file out of `iteration/smdlisp-final` for source programs, and
take every expectation from SBCL or the specification.* That is D22's substance
kept, delivered at the moment it has a consumer, which is where D26 puts it
anyway. It costs no step.

**It is marked provisional in D32, and what would revisit it is named**: a
C-series lane finding that reading source programs out of a tag is awkward
enough in practice to justify a real harvest step for that one file. An
abstraction nobody dared touch and one nobody needed to touch look identical
from the outside.

One fact that makes the override cheaper than it looks and that D22 did not
have in front of it: **`smdlisp` could not have been kept anyway.**
`smdlisp.smdlisp` links `smdscheme.foundation` and `smdscheme.parser`, and six
of `smdlisp`'s seven sub-library `CMakeLists.txt` files link a `smdscheme.*`
target. Keeping `smdlisp` through the C-series meant keeping both trees, and
`smdscheme`'s deletion was never in question.

**This plan does not plan the C-series.** What it owes, and delivers at A0, is
that the C-series does not begin with `docs/cl-language-scoping.md` describing
a first phase whose oracle no longer exists.

---

## Integration branches — three points, not two

**A0: `plan-reconciliation`, off `main`, merging to `main`.**
**Phase A: `cl-retire-trees`, created off `main` after A0 merges.**
**Phase B: `cl-parser-combinators`, created off `main` after Phase A merges.**

A0 merging straight to trunk is the change from the superseded plan and it is
deliberate. Everything A0 does is a repair to how the repository describes
itself, and it has value whether or not another step of this plan ever runs. If
it landed on a phase branch instead, the root `checklist.md` would stay broken
for the whole of Phase A — and this project runs blog-drafting agents on `main`
between steps, so "nobody reads `main` during a phase" is not true here.

The A-to-B split is unchanged and its reason is D31. Phase B's central claim is
that it changes no observable behaviour, and its acceptance witness is that the
existing reader tests and the conformance corpus pass **unchanged**. Phase A
deliberately churns the test population — it deletes `oracle_compare.test.cpp`
and roughly 800 test cases with the two trees, and adds a printer and a
differential back. On one shared branch the "unchanged" claim is unverifiable
by diff. On separate branches B8 checks it mechanically:

```sh
git diff --stat $(git merge-base main cl-parser-combinators)..HEAD \
    -- 'src/smd/cl/**/*.test.cpp' 'src/smd/cl/conformance/'
```

Both gaps are also human gates, and the second one matters more than the first.
Phase A deletes 30,000 lines and retires a rule `AGENTS.md` states as a rule; a
settled policy question is not the same thing as "nothing here could have gone
wrong in execution." A1 and B1 are each told to write a `blocked-NN.md` if
their branch is absent, so an unattended run halts at a gate rather than
improvising.

**The orchestrator has one job before A1:** create `cl-retire-trees` off `main`
after A0's merge. Do **not** reuse `retire-three-trees`; delete it or leave it
alone, but do not branch from it.

---

## Baseline

Three rows in `metrics.jsonl`, all before any step. The file is append-only and
no worker reads it.

| row | date | what | wall | log bytes | ctest/leg |
|---|---|---|---|---|---|
| `00-baseline` | 2026-08-17 | cold fresh worktree, `make test-matrix` | 409 s | 546,966 | 1118 |
| `00-baseline-2` | 2026-08-20 | cold fresh worktree, post-`ae3c33a` | **440 s** | **549,492** | **1123** |
| `00-baseline-3` | 2026-08-23 | **warm**, in-tree, `main` at `1d66c54` | 57 s | 488,795 | 1123 |

**Row 2 is what this plan's steps are sized against**, and row 3 is why that is
legitimate rather than lazy. Row 3 was measured for this replanning and is a
green confirmation, not a sizing row: it reports the **same 1123** entries per
leg as row 2, so nothing between `ae3c33a` and `1d66c54` changed the test
population — the intervening commits are two documentation merges and one
`clang-format` fix. Row 2's cold figures therefore still apply and were not
re-measured.

Also measured on `main` at `1d66c54`: `make lint` 3 s, exit 0.
`scripts/verify-transclusions.sh` 2 s, exit 0, **142 resolved, 1 recorded
`WARN`, 0 failures** — the WARN is a UUID occurring three times in
`phase-12-cps.org`, documented in `docs/blog/pins.md`, and is expected at every
step. `make testinstall` was not re-measured; row 1's 109 s / 7,386,663 bytes /
exit 2 still stands and is not a gate.

Two structural facts drive every sizing decision in this plan.

**The verify log is roughly half a megabyte regardless of what changed**,
because it is ~1120 ctest lines printed twice. It is dominated by test
*reporting*, not compilation, so it does not shrink when a step is small. Every
step file tells the agent to redirect and grep and never read it raw.
`make testinstall`'s log is **seven megabytes** of template diagnostics and is
worse; nothing in this plan needs it green.

**A fresh worktree is a cold build.** `.build/` lives inside the source tree,
so every step pays full cold-build wall time twice — once for the baseline and
once after. The 440-vs-57 gap between rows 2 and 3 is that cost, isolated, and
it is the single most interesting number the integration review has to work
with: it is build cost, not test cost, and it is paid per worktree rather than
per change.

A3's deletion removes roughly 800 of the ~1123 tests, so Phase B's steps should
be substantially cheaper. A3's handoff carries the new number forward, because
that is what B1–B8 are actually sized against.

### No prior calibration exists

There is no `metrics/` directory in this repository and the project's memory
directory holds no fan-out calibration entry. The superseded plan's rows were
never consumed by an integration review, so they are baselines and not
calibration. **This run's step sizing is the baseline being measured, not a
sizing against measured numbers.** The integration review should read it that
way: the question is not "did the estimates hold" — there were no calibrated
estimates — but "what does a step cost here, and where did the guesses miss."

---

## Acceptance commands the orchestrator runs after each merge

The worker's GREEN was pre-merge and in isolation. These check a state that did
not exist when the worker ran them; they are not a re-run of its verification.

**Per step, cheap enough to always run:**

```sh
make lint                          # ~3 s warm, ~11 s cold
./scripts/verify-transclusions.sh  # ~2 s
git diff --name-only <prev>..HEAD  # against the step's declared file scope
```

The scope check should expect the root `checklist.md` in every diff from A1
onward — that is the standing tick, declared in `AGENT-PROMPT.md`, not drift.

**Per step, expensive but required — it is the merge criterion:**

```sh
make test-matrix > /tmp/accept.log 2>&1
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/accept.log
```

Both legs, Debug (`-O0`) and Asan (`-O3 -fsanitize=address,undefined,leak`).
`make test` alone is the Asan leg only and is **not** sufficient; see
`docs/verification-matrix.md`. On the integration branch this is warm rather
than cold, so it costs tens of seconds rather than several hundred — row 3 is
the measured warm figure.

**Per step where the step file says so:** `make compile-headers` (A3, and B1
onward, because `CMAKE_VERIFY_INTERFACE_HEADER_SETS` is what proves the new
headers are genuinely self-contained).

**`make testinstall` is not an acceptance command for any step.** A2 and A3 run
it and record its state for the trend — a defect disappearing, then no
replacement coverage appearing — but neither its before nor its after result
gates a merge. Treating a change in its exit code as a regression this plan
caused is the single most likely misreading during execution; both step files
say so directly.

**Once per fan-out, not per step:** `make coverage` is an instrumented rebuild
under the `Gcov` profile and is not cheap; run it after B8. `make docs` needs
`mrdocs` in `.tools/` and may not be installed here; A3 owns the configuration
it depends on and is told to try it and report.

---

## The two files called `checklist.md`

`tmp/plan/checklist.md` is this fan-out's step tracker. Workers mark it **in the
main checkout by absolute path**, so it never enters a step's diff.

The repository root `checklist.md` is a project deliverable. A0 adds this plan's
fourteen steps to it; from A1 onward every step ticks its own line inside its
worktree. A3 and B8 additionally edit it beyond their own line, and both say so.

---

## File index

```
AGENT-PROMPT.md        the universal prompt; the three-tier reading contract
checklist.md           step tracker; workers mark their own line
step-A0.md             Phase 0 — reconciliation, merges to main
step-A1..A5.md         Phase A — retire the trees
step-B1..B8.md         Phase B — parser combinators
handoff-NN.md          written by step N-1, read once by step N, then dead
metrics.jsonl          append-only; workers write, only the review reads
INTEGRATION-REVIEW.md  the final non-code step, on a stronger tier
README.md              this file
```

---

## Known pre-existing rot, deliberately out of scope

Found while exploring; none of it is caused by this plan and none is in any
step's scope. `docs/backlog/BL-0005-…` covers the postponed testinstall/export
work; the rest are still just notes and should become backlog entries at the
integration review.

- `schemepoc.org` transcludes `src/smd/schemepoc/*`, a directory that has not
  existed since the rename to `smdscheme`. It is not covered by
  `verify-transclusions.sh`'s default globs, which is why it has gone
  unnoticed.
- `scripts/amalgamate.py` and `scripts/deploy_godbolt_tree.py` both hardcode
  the same dead `src/smd/schemepoc/` path.
- `make testinstall` red on `main`, as above — postponed rather than fixed by
  this plan.
- The repository carries well over a hundred stale local branches, including
  the nine-commits-behind `retire-three-trees` this plan had to route around.
  Nothing here prunes them, and the next plan that assumes a named branch is
  current will hit the same thing.
