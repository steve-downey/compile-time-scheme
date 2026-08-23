# Step A0 — rebase the record against reality

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. `src/smd/cl/` is the live tree, `src/smd/kit/` the shared constexpr
substrate; `src/smd/smdscheme/` and `src/smd/smdlisp/` are two finished
iterations still sitting in the worktree. Three plans have closed —
the original steps, the pivot L0–L24, the rebuild R0–R8 — and two scoping
notes have been ratified by merge. Nothing describes what happens next.

**Integration branch: `plan-reconciliation`, and it merges to `main`.** This is
the only step in this plan that merges to `main` rather than to a phase branch,
and that is deliberate: everything below is a repair to how the repository
describes itself, and it is worth having on trunk whether or not the rest of
this plan ever runs. The orchestrator has the longer version of that argument;
this file is authoritative for you and you do not need it.

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-a0-reconcile -b plan-reconciliation main
cd ../step-a0-reconcile
git submodule update --init --recursive
```

Your worktree branch **is** the integration branch here; there is no separate
step branch to merge into one. You merge `plan-reconciliation` into `main`, and
you do that **from the main checkout**, not from the worktree — see "Commit and
merge back" for why that difference matters here and nowhere else.

**Reserved for this step:** blog phase **33**, divergence number **DIV-0029**.

## Why this step exists, and why it is first

`AGENTS.md` says, as its first rule of the task protocol: "Work only the next
unchecked step in `checklist.md`." The root `checklist.md` is checked all the
way through — the original steps, the pivot's L0–L24, the rebuild's R0–R8, and
the blog backfill's B5–B11 — and has no entry for anything after that. An agent
that follows the project's own protocol today finds nothing to do. That is the
defect this step exists to fix, and it is first because every later step in
this plan runs in a repository that currently mis-describes itself.

Four smaller things came loose at the same time and are fixed in the same pass,
because they are all one kind of thing — a record that stopped matching the
tree — and because paying this plan's verify cost five times for five prose
edits would be absurd:

- Two decisions both numbered **D22**, one ratified and one about to be
  written by A3.
- Two scoping notes that were ratified by merge but still say, in their own
  first lines, that they are unratified proposals.
- Three backlog items whose gating conditions passed without anyone noticing.
- A phase sketch in `docs/cl-parser-scoping.md` § 4 that describes a plan
  nobody is going to execute, and a § 6 that prescribes an amendment mechanism
  which no longer exists.

## Verify GREEN baseline

```sh
make test-matrix > /tmp/verify-A0-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A0-base.log
wc -c /tmp/verify-A0-base.log
make lint
./scripts/verify-transclusions.sh
```

Two `100% tests passed` lines at **1123** entries per leg, measured on `main`
at `1d66c54`. `make lint` exits 0. The transclusion check exits 0 and reports
**142** resolved and **1** recorded `WARN` about a UUID occurring three times
in `phase-12-cps.org` — that warning is expected, documented in
`docs/blog/pins.md`, and is not a failure. Not green ⇒ `blocked-A0.md`.

Nothing in this step can move the ctest count. If it does, you changed
something you should not have.

## What already exists that this step builds on

- `docs/cl-rebuild-plan.md` § 2 holds decision records **D11–D21**, in the form
  `**D<n> — <one-line statement>.**` followed by a paragraph. Its § 5 and § 8
  demonstrate this project's append-a-dated-correction convention.
- `docs/cl-language-scoping.md` holds proposed **D22–D26**;
  `docs/cl-parser-scoping.md` holds proposed **D27–D31**. Both were merged
  (PR #48 and PR #50) and are ratified in substance. **D32 is the next free
  decision number** and has never been used: it appears only inside this
  plan's own earlier draft, as a number reserved and then withdrawn.
- `docs/divergences/README.md` — five classifications: `defect`,
  `scope-decision`, `artifact-of-D1`, `toolchain`, `process`. Read the
  taxonomy section and the table; you add one row.
- `docs/divergences/TEMPLATE.md` — the shape of a divergence record. Divergence
  docs are **append-only**: a dated note, never an edit in place.
- `docs/backlog/README.md` — its "Rules" section says the opposite for backlog
  items: "**Refine an item in place. Do not append status logs to it.**" Two
  conventions, two directories; do not mix them up.
- `docs/backlog/TEMPLATE.md` — the status vocabulary is
  `open | scheduled as step LNN | declined | superseded by BL-MMMM`.

## The change

Six parts. None of them touches a line of C++.

### 1. The root `checklist.md` gains this plan

Add a new section after "## Blog backfill", matching the file's existing
format exactly — a heading, a short paragraph of context, then `- [ ]` lines.
Fourteen steps: A0 through A5, B1 through B8. One line each, in the wording
`tmp/plan/checklist.md` uses. Tick **A0's own line** and leave the other
thirteen unchecked; every later step ticks its own.

Name the two integration branches in the section's paragraph, and say that
this section's steps are executed as a `tmp/plan/` fan-out rather than
hand-driven, so a reader knows why the entries appeared all at once.

Then two corrections inside the "## Common Lisp rebuild" section above it.

**The frozen-oracle sentence.** Find, by content:

```
`src/smd/smdlisp/**` is frozen as a behavioural oracle from R1 onward and is never edited.
```

It is true today and stops being true at A3, so do **not** delete it and do not
pre-date its retirement. Replace it with a sentence that says both halves: that
the rule holds now, and that decision **D32** (part 2 below) retires it at step
A3 of the section you just added. A rule with a scheduled end date is a
different thing from a rule, and an agent reading only this file should be able
to tell which one it is looking at.

**The R8 line.** Find, by content:

```
- [x] Step R8: extract the kit — `smd::kit::foundation` (seventeen `foundation/` files; `parser/` has no `cl` client, DIV-0028) (+ phase 32)
```

"Seventeen" is a correct description of what R8 moved and a misleading one to
read today, because `src/smd/cl/foundation/` now holds **eighteen** forwarding
shims: `ae3c33a` added `monad.hpp`, which never had a `cl`-side definition to
forward from. Do not silently change seventeen to eighteen — R8 really did move
seventeen files. Say seventeen, and say the shim count is now eighteen, in the
fewest words the line's format allows. The `parser/` clause and the DIV-0028
citation are both still accurate; leave them.

### 2. Decision **D32**, and the D22 collision

This is the part with real consequences, so read the whole of this section
before writing anything.

`docs/cl-language-scoping.md` § 3 contains, ratified:

> **D22 — Parity with `smdlisp` is the first phase, and retiring `smdlisp` is
> its merge criterion.**

Its argument is that every step of the C-series spine has, in `smdlisp`, a
working reference implementation of the same operator behind the same reader —
"a cheaper and more exact oracle than SBCL for exactly the semantics that are
subtlest, because it already made this project's choices."

Step A3 of this plan deletes `smdlisp` **before** that phase begins. That is a
direct override of a ratified decision's sequencing, and it needs a decision
record of its own rather than being smuggled in as a consequence of tidying up.

**Write it as D32, in `docs/cl-language-scoping.md`, not in
`docs/cl-rebuild-plan.md`.** Two reasons, and state the second one in the
record: D32 is the next free number after D31, and the override belongs
physically next to the decision it overrides, where nobody can read one without
the other. `docs/cl-rebuild-plan.md` is a closed plan and is the wrong place to
add a live decision.

Append a dated amendment section to `docs/cl-language-scoping.md` — do not edit
§ 1–§ 6 in place, except the `Status:` line in part 3 below. The section holds:

**D32 — `smdlisp` is retired to a tag before the parity phase, not as its merge
criterion.** It supersedes D22's sequencing and leaves D22's goal intact: the
C-series still closes § 1's table, it just does so without `smdlisp` in the
worktree. Say all of the following, because a record that only states the
conclusion is what makes the next reader re-litigate it:

- **The deciding reason is agent friction, and it is the owner's.** Agents keep
  getting hung up on not touching the dead trees and doing strategically wrong
  things, sometimes silently, while the trees are there to be gotten hung up
  on. That cost is paid continuously; D22's benefit is not.
- **D22's oracle argument was measured before being overridden, and it was
  substantially right.** `conformance/corpus.hpp` holds 75 entries, and because
  `satisfies()` runs `evaluate_program()` they reach exactly `cl`'s seven
  special operators and eighteen builtins and nothing else. `smdlisp` holds
  roughly 120 source-in / value-out evaluated cases covering precisely the gap
  — `LET`, `LET*`, `SETQ`, `LAMBDA`, `FUNCALL`/`APPLY`, `DEFMACRO` and
  backquote, `CATCH`/`THROW`, `TAGBODY`/`GO`, `DEFVAR`/`DEFPARAMETER`, multiple
  values. The corpus has none of it. What D22 wanted to keep is real.
- **But `smdlisp` supplies source programs, never expectations.** D16 says
  correctness comes from external authority; D22's own second limit says that
  where `smdlisp` pins a `defect` it is "a wrong answer by design"; D26 says a
  lane's entries are "derived from the specification and checked against SBCL,
  not written afterwards to describe what was built." Harvesting its answers
  would need a provenance kind meaning "our earlier implementation agreed",
  putting "this implementation agrees with itself" back into the one artefact
  R6 built to remove it. SBCL is at `/usr/bin/sbcl` and the harness already
  runs, so every entry would be re-adjudicated against SBCL anyway.
- **A tag supplies those source programs losslessly and for free.** After A1,
  `git show iteration/smdlisp-final:.../closure/eval_direct.test.cpp` prints
  all 96 of its cases. Harvesting now would land ~120 entries for operators
  that do not elaborate — a red build, or inert data, and inert data is what
  the tag already is.
- **So the harvest is deferred to its consumer, which is where D26 puts it.**
  Record a standing obligation on every C-series lane, in those words: *before
  writing a lane's conformance entries, read the corresponding `smdlisp` test
  file out of `iteration/smdlisp-final` for source programs, and take every
  expectation from SBCL or the specification.*

**Mark D32 provisional, and say what would revisit it**: a C-series lane
finding that reading source programs out of a tag is awkward enough in practice
to be worth a real harvest step for that one file. An abstraction nobody dared
touch and one nobody needed to touch look identical from outside.

Also add a short note under D22 itself — one sentence, in the same amendment
section, not inline in § 3 — pointing at D32, so a reader arriving at D22 is
not left with a decision that quietly stopped being followed.

**Do not confuse D32 with DIV-0032.** They are unrelated: `D32` is a decision
record and belongs to this step; `DIV-0032` is A3's reserved divergence number.
Step A3 executes D32 and appends the dated "executed at ..." note to it; you
write the decision, A3 does not re-write it.

### 3. Both scoping notes say they are unratified, and they are not

`docs/cl-language-scoping.md` and `docs/cl-parser-scoping.md` each open with:

```
- **Status:** proposal, awaiting ratification. Nothing here is authoritative yet.
```

Both were ratified by merge (PR #48 and PR #50 respectively) and this plan is
built on their decisions. Edit that one line in each — this is a fact about the
document, not prose, and it is the one in-place edit part 2 excepts. Replace it
with a status that says ratified, by which merge, and on what date, following
whatever spelling the two notes' "Successor to" / "Precedes" lines already use
for cross-references.

Use the owner's own framing rather than inventing a stronger one: these are as
provisionally ratified as anything here, and later facts and experience can
override early decisions. D32 in part 2 is the first exercise of exactly that,
which is worth one clause.

### 4. `docs/cl-parser-scoping.md` § 4 and § 6

§ 4 already carries one dated correction ("Corrected 2026-08-16, against the
execution plan in `tmp/plan/`"). That correction fixed a different set of
things — the P1-creates / P10-extracts contradiction, the numbers lane, why
intertoken space is serial — and its P1–P10 list is still there and still
describes a plan nobody will execute. Append a **second** dated amendment
section; do not edit the first one.

Six corrections, all of them found by writing this plan's steps against the
code rather than against the note:

1. **§ 4's P1–P10 assumed the three-tree repository.** Most of what made the
   sketch wide was keeping two dead front ends alive. Its shape — narrow, wide,
   narrow — survives; its content does not, and it becomes a series following a
   precursor phase.
2. **P3 should be deleted.** "`cl` retires its duplicates; `cursor.hpp` becomes
   a kit shim" presupposes a duplicate and there is no third tree.
   `src/smd/cl/reader/cursor.hpp` includes only
   `<smd/cl/foundation/source_pos.hpp>` and is fully independent: nothing to
   reconcile, only a decision about where the one cursor lives. That is one
   paragraph inside the step introducing the layer, not a phase.
3. **P1 and P2 should not exist as written.** "Kit parser core (serial)" then
   "kit choice and facade (serial)" builds the whole layer in two steps and
   first uses it in P5 or P6, maximising the guesses validated too late. The
   plan adds one primitive at a time, each in the step with the reader function
   that needs it: `bind` with `read_radix_number`, `skip_many` with the
   intertoken skippers, choice and bounded repetition with `read_delimited`. If
   the layer stops growing after the third, that is a fact worth having and the
   layered version could not have produced it.
4. **There is no `parser_ops` facade, and § 4's P2 should stop promising one.**
   The retired `ParserApplicative`/`ParserAlternative` facade had exactly one
   client, `smdscheme`, which is being deleted. Rebuilding it for a layer with
   one consumer is R8's lesson repeated; a facade wants a second client first.
5. **§ 4's numbering needs replacing.** It reserves "phases 33–42 and DIV-0029
   through DIV-0033" for ten steps. The plan is fourteen: blog phases **33–46**
   and **DIV-0029 through DIV-0042**.
6. **§ 2's Gap 1 and D28 need a landed-code correction.** § 2 says "the layer
   has no bind" and D28 proposes adding one. True of the retired `smdscheme`
   layer, and still is — but a Monad typeclass landed in `smd::kit::foundation`
   (`ae3c33a`) independently of this series, with `result<T>` already
   registered. D28's call is still right; the mechanism changes. The step
   registers `parser<F>` as a second instance of a typeclass that already
   exists rather than inventing `bind`, which is smaller and more constrained
   than either § 2 or this plan's first cut anticipated.

Two things in § 4 and § 5 were re-checked against the code and **hold exactly
as written**. Say so, because an amendment that lists only faults reads as a
repudiation and this is not one: there is no numbers lane and
`scan_token`/`classify_number` stay out of scope for the whole series, because
classifying a whole token *is* the mechanism by which DIV-0003 holds; and both
§ 5 traps are real — `many<Capacity>` stops at capacity and *succeeds* where
`read_delimited` diagnoses `"too many elements"`, and `skip_block_comment` lets
an unterminated `#|` run to end of input where the natural combinator fails at
the comment.

One correction that is not about § 4: **§ 3's D31 names
`reader/oracle_compare.test.cpp`'s "SBCL differential" as part of the
acceptance witness.** That file was never an SBCL differential — it compared
against `smdlisp`. Steps A4 and A5 build a real one; A2 deletes the retired
file. The witness D31 asks for exists after Phase A and not before.

**§ 6 prescribes a mechanism that no longer exists.** It says amending the
language note is "a correction to an unratified proposal rather than to a
landed decision, so it is a second commit on the same branch rather than a
divergence record." Both notes are ratified and merged and that branch is gone.
Record what this project actually does instead, which you can check for
yourself: `docs/divergences/` has **no** precedent for a divergence record
superseding a decision, and the only precedent for superseding one is
decision-on-decision written into the plan document — D11 retiring D1, D21
relaxing what D11 carried forward. So the mechanism is a **new decision record
appended to the note that owns the one being overridden**, which is exactly
what part 2 does with D32. Say that § 6's prescription is replaced by it.

### 5. Three backlog items whose gates fired

Backlog items are refined **in place**. Do not append dated notes to these.
Update `docs/backlog/README.md`'s "## Items" table row for each one you touch.

**BL-0002 — size the node arena from a compile-time measurement.** Its status
is "open — unblocked by D14, revisit during step R1", and its own body says
"Revisit this document during R1 ... and close it if D14 makes it moot." R1
landed. Nobody revisited it. The question is answerable against the code and
you should answer it rather than re-deferring: D14 says capacity is not part of
type identity, and the live claim to check is whether `src/smd/cl/`'s runtime
value types are in fact free of capacity template parameters, with capacity
living on storage instead. `docs/cpp-rules.md` states the rule; check whether
the tree keeps it. Note that `datum_tree<MaxNodes, MaxList>` being parameterised
is not a counter-example — that is storage, which is where D14 puts capacity.
Then either close the item (`superseded by D14`, with the reason) or restate its
status with a **new, checkable** revisit condition. "Revisit later" is not one.
Its measurement tables were taken on `smdscheme` and stay useful as evidence for
what the old coupling cost; keep them and say that is what they now are.

**BL-0003 — elide a `defun`'s implicit block by a `cata`.** Its README status is
"open — wants R6's corpus first"; R6 landed. Its body's reason was that
"eliding a block changes which programs fit in a given frame budget, and R6 is
where a program long enough to care gets written." Restate the status against
what R6 actually produced — 75 corpus entries, of which the relevant ones are
the `block`/`return-from` and recursive-`defun` cases — and give it a checkable
next condition. You are not implementing it; you are making its status true.

**BL-0004 — retire the `cl/foundation` forwarding shims.** Its origin line says
seventeen and its "What" section lists seventeen, but it also carries a
paragraph explaining there are eighteen since the Monad typeclass landed. That
internal disagreement is the defect: make the item say eighteen throughout, with
`monad.hpp` marked as the one that never had a `cl`-side definition to forward
from. Verify the count yourself:

```sh
grep -l 'Forwarding shim' src/smd/cl/foundation/*.hpp | wc -l
```

Its **"Frozen-tree impact"** paragraph reads "none ... `src/smd/smdscheme/**`
keeps its own independent copies and is not a client of the kit;
`src/smd/smdlisp/**` depends on `smdscheme.foundation` and is unaffected." Both
clauses stop meaning anything at A3. Rewrite the paragraph to state the
scheduled fact: A3 deletes both trees, after which the shims' only remaining
context is where the kit boundary sits, which is the actual question the item
turns on. Also update the README table's title for BL-0004, which says
"seventeen".

### 6. DIV-0029 — how a ratified decision gets superseded here

Write `docs/divergences/DIV-0029-<short-title>.md` from
`docs/divergences/TEMPLATE.md`, classified **`process`**, and add its row to
`docs/divergences/README.md`'s table.

What it records: this project had no stated mechanism for superseding a
*ratified* decision, and part 2 needed one. The taxonomy's existing precedents
are all decision-on-decision inside a plan document; no divergence record has
ever overridden a decision, and DIV-0004 and DIV-0016's "supersedes" sections
supersede other divergence records rather than decisions. So D32 establishes the
pattern by following the existing one — a new numbered decision, appended to the
note that owns the superseded decision, naming it explicitly and saying which
part survives. Say why the alternative was rejected: filing the override as a
divergence would have classified a deliberate change of course as a deviation
from authority, which is a category error, and would have hidden it from anyone
reading the decision list.

Keep it short. This is a record of a convention, not an essay.

### 7. Anchors

No source file changes, so no `uuidgen` output is required. If you anchor a
region of the amendment sections for the post to transclude, that is your
choice and not an obligation — `scripts/verify-transclusions.sh` must be green
either way, and it checks `docs/compiler_architecture.org` against the worktree,
which this step does not touch. Say in your handoff which you did.

## Declared file scope

```
checklist.md                              (new section; two corrections above it)
docs/cl-language-scoping.md               (Status line; dated amendment with D32)
docs/cl-parser-scoping.md                 (Status line; second dated amendment)
docs/backlog/BL-0002-compile-time-arena-sizing.md            (in place)
docs/backlog/BL-0003-implicit-block-elision-as-a-cata.md     (in place)
docs/backlog/BL-0004-retire-the-cl-foundation-forwarding-shims.md  (in place)
docs/backlog/README.md                    (Items table rows)
docs/divergences/DIV-0029-*.md            (new)
docs/divergences/README.md                (one table row)
```

**Not in scope, and each for a reason you should not talk yourself out of:**

- `AGENTS.md` and `CLAUDE.md` — they state the three-tree rule, which is still
  true until A3. A3 retires it in all its homes at once, so none is left
  contradicting the others.
- `docs/backlog/README.md`'s "Rules" line asserting the `smdlisp` freeze — same
  reason; it is A3's, and A3's step file names it. You edit only the Items
  table.
- `docs/cl-rebuild-plan.md` — closed, and D32 goes in the language note.
- Anything under `src/`.

## Verify GREEN after

```sh
make lint
./scripts/verify-transclusions.sh
make test-matrix > /tmp/verify-A0-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A0-after.log
wc -c /tmp/verify-A0-after.log
```

Run `make lint` early and often — it is three seconds and it runs `markdownlint`
and `codespell` over everything you are editing, which is the only real failure
mode this step has. The ctest count must be **exactly 1123** per leg: a
documentation change that moves it has done something else as well.

## Spot checks

```sh
grep -n 'D32' docs/cl-language-scoping.md
grep -rn 'awaiting ratification' docs/cl-language-scoping.md docs/cl-parser-scoping.md
grep -c '^- \[ \]' checklist.md
grep -n 'seventeen' docs/backlog/BL-0004-*.md docs/backlog/README.md checklist.md
ls docs/divergences/DIV-0029-*
grep -c 'Forwarding shim' src/smd/cl/foundation/*.hpp | grep -c ':1'
```

The second must return **nothing**. The third must be **13** — A0's own line is
ticked, thirteen remain. The fourth must show only the root `checklist.md`'s
deliberate "R8 moved seventeen" statement, and nothing in the backlog. The last
must print **18**.

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
docs: make the record describe the tree again

AGENTS.md says to work the next unchecked step in checklist.md. There
is not one. The file is ticked through the original steps, the pivot's
L0-L24, the rebuild's R0-R8 and the blog backfill, and then stops, so
an agent following the project's own protocol finds nothing to do.

Four other records came loose alongside it and are fixed together,
because they are one kind of thing and paying the test matrix five
times for five prose edits would be silly.

D32 is the substantive one. cl-language-scoping.md's ratified D22
makes parity with smdlisp the first phase and its retirement that
phase's merge criterion; this plan deletes smdlisp before the phase
starts. That is an override, so it gets a decision record rather than
arriving as a side effect of tidying up, and it sits in the note that
owns D22 because a reader arriving at D22 should not be able to miss
it. D22's argument was measured before being overridden and was
largely right -- the corpus's 75 entries reach cl's seven special
operators and nothing else, while smdlisp holds roughly 120 evaluated
cases covering exactly the gap. What smdlisp cannot supply is
expectations, and a tag prints its source programs for free forever.
So the harvest is deferred to the lane that will use it, which is
where D26 already put it.

The rest: both scoping notes still called themselves unratified
proposals after being merged; cl-parser-scoping.md's phase sketch
describes a plan nobody will run and its section 6 prescribes an
amendment mechanism whose branch no longer exists; and BL-0002,
BL-0003 and BL-0004 had their gating conditions pass unnoticed, with
BL-0004 disagreeing with itself about seventeen shims or eighteen.
DIV-0029 records the one thing with no precedent here, which is how a
ratified decision gets superseded.
EOF

cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git merge --no-ff plan-reconciliation
```

**Merge from the main checkout, not from inside your worktree.** `main` is
already checked out at
`/home/sdowney/src/steve-downey/compile-time-scheme/main`, so a `git checkout
main` inside the worktree fails outright — git refuses to check a branch out
twice. Every other step in this plan merges into a phase branch that is checked
out nowhere and can do it in place; A0 is the exception because its integration
branch is trunk.

Do not push. `AGENTS.md`: pushing is the orchestrator's, on instruction.

## Record measurements

After the merge, before cleanup.

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"A0","lane":null,"outcome":"green","wall_seconds":<measured>,"attempts":<n>,"verify":{"command":"make test-matrix + lint + verify-transclusions","exit_code":0,"wall_seconds":<measured>,"log_bytes":$(wc -c < /tmp/verify-A0-after.log),"summary_lines_read":<n>},"diff":{"files_changed":<n>,"insertions":<n>,"deletions":<n>},"out_of_scope":[],"note":""}
EOF
```

This row is the plan's documentation-only floor: no code, no possible effect on
any test, and the full matrix paid anyway. If the matrix dominated it, say so in
`note` in one line — the integration review is looking for exactly that shape
and it is the most actionable thing this run can produce.

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-a0-reconcile
```

Mark A0 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff

Read `tmp/plan/step-A1.md`, then write `tmp/plan/handoff-A1.md` (≤ ~150 lines).

A1 freezes both iterations as tags and moves their architecture prose to a
pinned history document. Tell it:

- That **`cl-retire-trees` may not exist yet** — the orchestrator creates it off
  `main` after your merge, because Phase A must branch off the corrected trunk.
  The pre-existing `retire-three-trees` branch points at `1ace4ad`, nine commits
  behind, and is **not** this plan's branch; A1 must not use it.
- The exact `[[file:...]]` link count you observed in
  `docs/compiler_architecture.org` and how it splits. A1's file says 37 total,
  31 pointing at code A3 deletes; confirm or correct it, because A1's own
  arithmetic check depends on the number being right.
- The wording you used for the two scoping notes' new `Status:` lines, so A1's
  and A3's cross-references match.
- Whether you closed BL-0002 or restated it, in one line — A1 does not need the
  reasoning, only the outcome, because nothing downstream should re-open it.
- Anything `make lint` complained about in a documentation file, since every
  later step writes prose into the same directories and `codespell` also scans
  `tmp/plan/`.
