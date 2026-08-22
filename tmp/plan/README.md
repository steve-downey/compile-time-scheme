# Execution plan: retire the three trees, then adopt parser combinators

**Not for worker agents.** A cleared worker reads `AGENT-PROMPT.md`,
`checklist.md`, its own `step-NN.md`, and its own inbound handoff. This file is
for the orchestrator and for the owner. It is not on any worker's read path,
which is why it is allowed to be discursive.

Produced 2026-08-17. **Amended 2026-08-20**, before any step executed — the
tree moved under the plan (two commits landed on `main`: the Monad typeclass,
and `read_delimited`'s D15 ladder retired by hand), and the owner issued four
new directives that reorder Phase A and settle its open governance question.
Nothing in this plan had run when the amendment was made; this is a
re-decomposition, not a mid-run correction. Supersedes `tmp/plan-draft-v1/`,
which was provisional scaffolding, and supersedes the 2026-08-17 shape of
this same directory, preserved only in `git log`.

---

## The shape

Two phases, thirteen implementation steps, one integration review — unchanged
counts from the first cut of this plan. What changed is Phase A's internal
order and two of Phase B's step contents.

**Phase A retires the three-tree structure.** The largest reorder: **deletion
now runs third, not fifth.** See "The reorder" below for why, and for the
evidence it rests on.

| step | one line | blog | DIV |
|---|---|---|---|
| A1 | freeze both iterations as tags; move their architecture prose to a pinned history doc | 33 | 0029 |
| A2 | neutralise the dead trees' build consumers (examples, install test, export list) | 34 | 0030 |
| A3 | **delete `smdscheme` and `smdlisp` from trunk** (decision D22, ratified) | 35 | 0031 |
| A4 | a `prin1`-shaped printer for `cl` datums, proved against SBCL in the same step | 36 | 0032 |
| A5 | broaden the SBCL reader differential into a real corpus | 37 | 0033 |

**Phase B adopts parser combinators in `cl`'s reader.** Step count, order and
blog/DIV numbering are unchanged from the first cut; B2 and B4 have revised
content because the tree they act on moved (see "What the tree already did").

| step | one line | blog | DIV |
|---|---|---|---|
| B1 | split `read.hpp` into component headers plus an umbrella | 38 | 0034 |
| B2 | `smd::kit::parser`: a Monad *instance*, threaded context, `read_radix_number` as first client | 39 | 0035 |
| B3 | intertoken space and comments onto the layer | 40 | 0036 |
| B4 | choice and bounded repetition; `read_delimited` onto the combinator layer | 41 | 0037 |
| B5 | text: strings and character literals | 42 | 0038 |
| B6 | forms: the quote family and token data | 43 | 0039 |
| B7 | sharpsign dispatch and `read_node` | 44 | 0040 |
| B8 | close the series: the D30 measurement, DIV-0028 dissolved, the architecture section | 45 | 0041 |

Blog phase numbers continue from 32 and divergence numbers from 0028; both
are reserved **per step up front** so nothing can collide on the next free
number. An unused reservation is expected and fine.

---

## The reorder: deletion runs third, not fifth

The plan's first cut built a replacement reader oracle (printer, then an
SBCL differential) *before* retiring anything, so the old comparison against
`smdlisp` would never be without a successor even for one step. That
ordering is wrong for this repository, and the owner's own words are why:
**agents keep getting hung up on not touching `smdscheme` and `smdlisp` and
doing strategically wrong things, sometimes silently, while those trees are
still there to be gotten hung up on.** That cost now dominates every
sequencing argument in this plan. The deletion moves to the front of what
Phase A can safely do first.

It cannot move all the way to first, for a reason that has nothing to do
with oracles: `docs/compiler_architecture.org` is a living document, and its
verifier resolves links against the **worktree**, on purpose, so an orphaned
link is caught immediately rather than left to rot. Deleting the trees with
that document unchanged would fail the very next `verify-transclusions.sh`
run. So A1 (freeze the trees as tags, move their prose to a pinned document)
still has to precede the deletion, and A2 (neutralise every build consumer —
examples, install test, the export list, `oracle_compare.test.cpp`) still
has to precede it too, because the top-level `beman_install_library` call
and `src/examples/CMakeLists.txt` both name targets that stop existing the
moment `git rm -r` runs. Everything else — building the printer, building
the SBCL differential out into a real corpus — follows the deletion rather
than gating it. The evidence those two steps produce is a restoration of
reader-layer differential coverage, not a precondition for removing the old
one.

The evidence checked before committing to this order, all verified directly
against this tree on 2026-08-20 rather than trusted from the first cut's
prose:

- **`src/smd/cl/reader/oracle_compare.test.cpp` is 28 fixed string-literal
  checks across six `TEST_CASE`s, no generated input.** Confirmed by
  reading the file. It also `#include`s `<smd/smdscheme/parser/cursor.hpp>`
  directly, not only `<smd/smdlisp/reader/read_datum.hpp>` — the first cut
  of this plan said `cl`'s only edge to the dead trees ran through
  `smdlisp`; it actually runs through both, from the same one file. That
  does not change the ordering conclusion, but it changes A2's declared
  file scope and its "what already exists" section, both updated.
- **Its distinctive inputs are covered elsewhere in the reader's own
  tests.** The bare fixnum literal `0` is pinned in `number.test.cpp`
  (`fixnum_of("0", 10, 0)`), confirmed by grep, and is **not** in
  `read.test.cpp`. The comment-inside-a-list case is covered by
  `read.test.cpp`'s `skips_comments_and_whitespace`, confirmed by reading
  it: it reads `(1 ; comment\n 2 #|mid|# 3)` and checks the arity is 3.
- **Independence was already weak and is measurably eroding.**
  `agree_on_keyword` in the retired file does
  `syms.name(my_kw->id).substr(1) == their_kw->name.view()` — confirmed by
  reading the function — trimming one side's name to keep the comparison
  agreeing. A differential tuned toward agreement has stopped being
  evidence, whichever side is right.
- **The conformance corpus does not reach the datum-structure layer.**
  `corpus.hpp`'s `satisfies()` calls `evaluate_program()`, an evaluated
  outcome, and never inspects a datum tree. Confirmed by reading it. The
  reader has no other differential once `oracle_compare.test.cpp` is gone,
  which is exactly why A4/A5 exist and why they are not optional follow-up.

One correction to the first cut's own count, found while re-verifying:
`docs/compiler_architecture.org` carries **39** `[[file:…]]` transclusion
links today, not 37, of which **31** point at code A3 deletes — that part of
the original count held exactly (28 directly under the two trees, 3 more at
`src/examples/godbolt_lisp.cpp`/`godbolt_lisp_ffi.cpp`). Recount before
trusting either number again if the document has changed further.

## What the tree already did before this amendment: two commits, two step revisions

Two commits landed on `main` (`ae3c33a`, `93e4bc7`) between this plan's first
cut and this amendment. Both are read-only facts this plan now designs
around, not steps this plan performs.

**The Monad typeclass now exists** (`src/smd/kit/foundation/monad.hpp`,
`ae3c33a`): a `monad<Impl>` CRTP base, a `monad_typeclass<T>` lookup, and
`bind`/`join` CPOs, in the same shape `functor.hpp`/`applicative.hpp`
already used. `result<T>` is registered against it in
`result_instances.hpp`, and `and_then` is now one line calling the `bind`
CPO. `src/smd/cl/foundation/monad.hpp` is a pure forwarding shim (never had
a `cl`-side definition), the eighteenth candidate on
`docs/backlog/BL-0004-…`. **This changes B2.** The step no longer invents
`bind` as a free-standing function on `parser<F>`; it registers `parser<F>`
as a second Monad instance beside `result<T>`'s, through the same
`monad_typeclass` machinery, and every caller reaches it through the same
generic `bind` CPO. B2's step file names the exact shape and the
CPO-is-an-object qualification trap (`ae3c33a`'s own commit message names
it for `result`; it is more exposed for `parser`, which has no pre-existing
domain name like `and_then` to hide behind).

**`read_delimited`'s D15 ladder is already retired**, by hand, not by this
plan (`93e4bc7`): `if (!element.has_value()) { return element; }` is gone,
replaced by an `and_then` chain whose surviving `has_value()` is the
unfold's loop-continuation test, correctly distinguished in that commit's
own message from the ladder D15 forbids. **This changes B4, but does not
remove it.** What is left is the raw `while (true)` loop around that
`and_then` chain, still carrying a "substrate generic algorithm" comment
`docs/cpp-rules.md` does not license for reader code — the same claim B3
finds and rejects for `skip.hpp`. B4's remaining job is exactly the work
`93e4bc7`'s own message defers: "the loop itself becomes monadic … the
combinator layer's work and not this one's." B4's step file no longer
claims to retire a ladder; it claims to replace the last raw loop and its
false exemption, building the element step's repetition **from** the
existing `bind` rather than rewriting it.

A third, smaller correction found in the same pass: the plan's original
"five `smdlisp` `CMakeLists.txt` files link `smdscheme.*`" is **six**
(`smdlisp`, `closure`, `elaborator`, `macroexpand`, `reader`, `sender`),
confirmed by grep. It does not change any conclusion — the two trees are
still mechanically inseparable either way — but A3's step file states the
correct count.

## Governance: D22, ratified, and D32 withdrawn

The plan's first cut treated retiring `smdlisp`'s never-edited rule as "the
one thing in Phase A that is a judgement rather than an execution," and
reserved decision number **D32** for it specifically to avoid colliding with
D22–D26, proposed and unratified in `docs/cl-language-scoping.md` (PR #48,
not present on this branch — it lives on the sibling `cl-language-scoping`
branch).

**The owner has settled this directly**, in the plainest terms available:
retiring `smdlisp` and its never-edited rule is authorized outright, and
D-numbering is bookkeeping — "pick one, or both, and say they were done." A3
now records the retirement as **D22**, ratified, in
`docs/cl-rebuild-plan.md`, and records **D32 as withdrawn and folded into
D22** so the number is not reused later.

This creates a real, visible tension, and A3's step file says so rather than
papering over it: `docs/cl-language-scoping.md`'s own proposed D22 argues
the *opposite* sequencing — keep `smdlisp` as a differential oracle through
a full language-parity phase (its C1–C5 spine: function values, lambda
lists, lexical and special binding, macros), and retire it only once that
phase closes the gap between `cl`'s object language and `smdlisp`'s. This
plan's D22 supersedes that framing rather than agreeing with it. The
language-parity work `smdlisp` would have made cheaper still has to happen;
it happens without `smdlisp`'s help now, on whatever oracle authority SBCL
and the corpus can supply, at whatever cost that turns out to be. Recording
that cost honestly, rather than implying the two D22s agree, is A3's job and
is called out explicitly there. Whoever eventually ratifies
`docs/cl-language-scoping.md` renumbers its proposal around this D22, not
the reverse — this one lands on trunk first.

No step in this plan is blocked on any further governance decision.

## `make testinstall` stays red for this entire plan, and that is intended

The owner's direction: `make testinstall` is **postponed**, splits out of
this plan, and does not gate anything in it.

It is already red on `main`, for a defect that predates this plan entirely —
`smdscheme.closure`'s installed `FILE_SET` is missing `pairs.hpp`. That
defect disappears when the tree that has it is deleted, but nothing
positive replaces it inside this plan:

- **In scope, because the build must stay green:** A2 removes every example
  and install-test file that names a target `smdscheme`/`smdlisp` deletion
  would otherwise leave dangling, and shrinks the top-level export list to
  `smd.fixpoint` alone. This is necessary for `make test-matrix` (and for
  `make install`'s configure step) to keep working, and it is genuinely
  the minimum — it does not export the live trees or add a `cl` example.
- **Postponed, out of this plan:** `src/examples/godbolt_cl.cpp`,
  re-pointing `installtest/` at `kit`/`cl`, exporting `kit.foundation` and
  the `cl.*` targets, and turning `make testinstall` green. All of it is
  `docs/backlog/BL-0005-export-live-trees-and-turn-testinstall-green.md`,
  scheduled after Phase B rather than folded into the deletion.

`make testinstall` is **not** one of this plan's acceptance signals at any
step. A2's and A3's step files measure it, for the trend, and say plainly
that it is not a gate.

## Why Phase A retires the trees rather than merely tidying, restated

`cl` reaches into the dead trees, before A2, in exactly the places named
above: `oracle_compare.test.cpp` (both trees, by include) and the
`smdlisp.reader` link line beside it in `src/smd/cl/reader/CMakeLists.txt`.
Everything else that mentions `smdscheme` or `smdlisp` under `src/smd/cl/`
is comment prose. So the coupling that made the parser work look large was
never real coupling — Phase B's parser layer has exactly one consumer once
the trees are gone, and three consequences follow from that, restated from
the first cut because they are unaffected by the reorder:

- **D29 is answered: thread the context.** A parser is
  `parse_result<T>(cursor, Ctx&)` over a named `Ctx` concept, not
  `parse_result<T>(cursor)` with `&ctx` captured. Combinators reify parsers
  into storable values, so a captured `&ctx` can outlive its context in a
  way the current per-call `read_node(cur, ctx)` cannot, and DIV-0007 is
  this project's own record of paying for that bug class once. `no_context`
  may exist on its merits but is never a default argument; there is no
  compatibility to keep.
- **DIV-0028 dissolves rather than closing.** "The parser combinator layer
  has no `cl` client" was an artifact of keeping two dead front ends. B8
  records that it ceased to apply. No step's deliverable is "close
  DIV-0028".
- **There is no third-tree "cl retires its duplicates" step.** There is no
  third tree to be a duplicate of. B2 moves `cl::reader::cursor` into the
  kit and leaves an R8-style shim, deliberately and in one paragraph,
  because keeping the name alive is what makes D31's "existing tests pass
  unchanged" checkable by diff.

## Integration branches — two, not one

**Phase A: `retire-three-trees`, created off `main` (exists).**
**Phase B: `cl-parser-combinators`, created off `main` by the orchestrator
after Phase A merges. It does not exist yet, deliberately.**

The reason is D31, and it is unaffected by the reorder: Phase B's central
claim is that it changes no observable behaviour, and its acceptance witness
is that the existing reader tests and the conformance corpus pass
**unchanged**. Phase A deliberately changes the test population — it
deletes `oracle_compare.test.cpp` and roughly 800 test cases with the two
trees, and adds a printer and a differential back. On one shared branch the
"unchanged" claim is unverifiable by diff. On two, B8 can check it
mechanically:

```sh
git diff --stat $(git merge-base main cl-parser-combinators)..HEAD \
    -- 'src/smd/cl/**/*.test.cpp' 'src/smd/cl/conformance/'
```

The gap between them is also a deliberate human gate, and it is now the gate
that follows the deletion rather than following a tidying-up phase. Phase A
deletes 30,000 lines and retires a rule `AGENTS.md` states as a rule; that
is worth a look before Phase B builds on it, even with the governance
question itself already settled — a settled *policy* question is not the
same thing as "nothing here could have gone wrong in execution." Step B1 is
told to write `blocked-B1.md` if `cl-parser-combinators` is absent, so an
unattended run halts at the gate rather than improvising.

The current branch, `plan-combinator-smdscheme`, holds this plan and this
amendment. It is not an integration branch for either phase.

## Baseline

Two rows now, and the second supersedes the first for sizing Phase A — the
first is left in place because `metrics.jsonl` is append-only.

**Row zero, measured 2026-08-17,** before `ae3c33a`/`93e4bc7`. GCC 16.1.1,
`TOOLCHAIN=gcc-16`, WSL2.

| measurement | value |
|---|---|
| `make test-matrix`, cold fresh worktree, both legs | 409 s |
| `make test-matrix`, cold, log size | 546,966 bytes |
| `make test-matrix`, warm after touching `read.hpp` | 40 s |
| ctest entries, per leg | 1118, 100% passing |
| `make lint` | 11 s |
| `scripts/verify-transclusions.sh` | 2 s, 142 resolved, 1 recorded WARN, 0 failures |
| `make testinstall` | 109 s, exits 2 — pre-existing failure |
| `make testinstall` log size | 7,386,663 bytes |

**Row zero, re-measured 2026-08-20,** after `ae3c33a`/`93e4bc7` landed on
`main`. Same machine, same toolchain, same `git worktree add … HEAD --detach`
method as row zero.

| measurement | value |
|---|---|
| `make test-matrix`, cold fresh worktree, both legs | **440 s** |
| `make test-matrix`, cold, log size | **549,492 bytes** |
| `make test-matrix`, warm, same checkout, nothing touched | 43 s |
| ctest entries, per leg | **1123**, 100% passing (5 more: `monad.test.cpp` and friends) |

`verify-transclusions.sh` and `make testinstall` were not re-measured in the
second pass — nothing in the two commits touches documentation or the
install path, so row zero's figures for those two still apply. **This
second row is what Phase A's steps are sized against; A3's own step file
also records the post-deletion count, which is what Phase B is sized
against, since a ~800-test drop is expected there and is not comparable to
either baseline row.**

Two things from row zero shape the whole plan and are unaffected by the
re-measurement.

**The verify log is half a megabyte regardless of what changed**, because it
is ~1120 ctest lines twice. It is dominated by test *reporting*, not by
compilation, so it does not shrink when a step is small. Every step file
therefore tells the agent to redirect and grep and never to read it raw.
`make testinstall`'s log is **seven megabytes** of template diagnostics and
is even more dangerous — and, per the postponement above, is not something
any step in this plan needs to make green.

**A fresh worktree is a cold build**: `.build/` lives inside the source
tree, so every step pays the full cold-build wall time twice — once for the
baseline and once after. That is why steps are sized the way they are.
Phase A's deletion removes roughly 800 of the ~1120 tests, so Phase B's
steps should be substantially cheaper; A3's handoff carries the new number
forward, because it is what B1–B8 are actually sized against.

**`make testinstall` is already red on `main`**, with
`smd/smdscheme/closure/pairs.hpp: No such file or directory` — a header
never added to the installed `FILE_SET`. It is a defect in a tree being
deleted, and per the postponement section above, **nothing in this plan
turns it green** — that is `docs/backlog/BL-0005-…`, scheduled after Phase B.

## No prior calibration exists

There is no `metrics/` directory in this repository and the project's memory
directory holds no fan-out calibration entry. **This run's step sizing is
the baseline being measured, not a sizing against measured numbers.** The
integration review should read it that way: the question is not "did the
estimates hold" — there were no calibrated estimates — but "what does a step
cost here, and where did the guesses miss."

## Acceptance commands the orchestrator runs after each merge

The worker's GREEN was pre-merge and in isolation. These check a state that
did not exist when the worker ran them; they are not a re-run of its
verification.

**Per step, cheap enough to always run:**

```sh
make lint                        # ~11 s
./scripts/verify-transclusions.sh  # ~2 s
git diff --name-only <prev>..HEAD  # against the step's declared file scope
```

**Per step, expensive but required — it is the merge criterion:**

```sh
make test-matrix > /tmp/accept.log 2>&1
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/accept.log
```

Both legs, Debug (`-O0`) and Asan (`-O3 -fsanitize=address,undefined,leak`).
`make test` alone is the Asan leg only and is **not** sufficient; see
`docs/verification-matrix.md`. On the integration branch this is warm rather
than cold, so it costs tens of seconds rather than several hundred.

**Per step where the step file says so:** `make compile-headers` (B1
onward, because `CMAKE_VERIFY_INTERFACE_HEADER_SETS` is the check that the
new headers are genuinely self-contained).

**`make testinstall` is not an acceptance command for any step in this
plan.** A2 and A3 run it and record its state for the trend — a defect
disappearing, then no replacement coverage appearing — but neither its
before nor its after result gates a merge. Treating a change in its exit
code as a regression this plan caused is the single most likely
misreading during execution; both step files say so directly.

**Once per fan-out, not per step:** `make coverage` is an instrumented
rebuild under the `Gcov` profile and is not cheap; run it after B8, not
after each merge. `make docs` needs `mrdocs` in `.tools/` and may not be
installed here; A3 owns the configuration it depends on and is told to try
it and report.

## The two files called `checklist.md`

`tmp/plan/checklist.md` is this fan-out's step tracker. Workers mark it **in
the main checkout by absolute path**, so it never enters a step's diff and
never becomes a merge point.

The repository root `checklist.md` is a project deliverable and is edited
inside a worktree only where a step file says so — A3 and B8.

## File index

```
AGENT-PROMPT.md        the universal prompt; the three-tier reading contract
checklist.md           step tracker; workers mark their own line
step-A1..A5.md         Phase A
step-B1..B8.md         Phase B
handoff-NN.md          written by step N-1, read once by step N, then dead
metrics.jsonl          append-only; workers write, only the review reads
INTEGRATION-REVIEW.md  the final non-code step, on a stronger tier
README.md              this file
```

---

## Recommended amendments to `docs/cl-parser-scoping.md`

That document is a **proposal awaiting ratification** and nothing here edits
it. Its § 4 phase sketch was written before the steps were, and writing them
against the code found six things wrong with it — five from the first pass,
one more found while re-verifying for this amendment. Collected here so they
can be acted on in one pass.

**1. § 4's P1–P10 assumed the three-tree repository. It should become a
P-series that follows a precursor phase.** The sketch's shape — narrow,
wide, narrow — survives; its content does not, because most of what made it
wide was keeping two dead front ends alive.

**2. P3 should be deleted.** "`cl` retires its duplicates; `cursor.hpp`
becomes a kit shim" presupposes a duplicate. There is no third tree.
`src/smd/cl/reader/cursor.hpp` includes only
`<smd/cl/foundation/source_pos.hpp>` and is fully independent, so there is
nothing to reconcile — only a decision about where the one cursor lives. It
is one paragraph inside the step that introduces the layer, not a phase.

**3. P1 and P2 should not exist as written.** "Kit parser core (serial)"
followed by "kit choice and facade (serial)" builds the entire layer in two
steps and first uses it in P5 or P6, which maximises the number of guesses
validated too late. The executed plan adds one primitive at a time, each in
the same step as the reader function that needs it: `bind` with
`read_radix_number`, `skip_many` with the intertoken skippers, choice and
bounded repetition with `read_delimited`. If the layer stops growing after
B4, that is a fact worth having, and the layered version could not have
produced it.

**4. There is no `parser_ops` facade in the executed plan, and § 4's P2
should stop promising one.** The retired `ParserApplicative`/
`ParserAlternative` typeclass-object facade had exactly one client,
`smdscheme`, which no longer exists. Rebuilding it for a layer with one
consumer would be R8's lesson repeated. If a facade is wanted later, it
wants a second client first.

**5. § 4's "Every step ships a post … phases 33–42 and DIV-0029 through
DIV-0033" needs renumbering.** With the precursor phase there are thirteen
steps: blog phases 33–45 and DIV-0029 through DIV-0041. Unaffected by this
amendment's reorder — the total count and span were already right.

**6. § 2's Gap 1 and D28 need a landed-code correction, found during this
amendment.** § 2 says "the layer has no bind" and D28 proposes adding one.
That was true of the retired combinator layer (`smdscheme`'s
`parser<T>`/`alt.hpp`) and is still true of it — but a Monad typeclass has
since landed in `smd::kit::foundation` (`ae3c33a`), independently of this
series, with `result<T>` already registered against it. D28's "the parser
layer becomes Monadic, not merely Applicative" is still the right call for
`smd::kit::parser`; what changes is the mechanism — B2 registers `parser<F>`
as a second instance of the typeclass that already exists rather than
inventing `bind` from nothing, which is a smaller and more constrained step
than either § 2 or the first cut of this plan anticipated.

Two things in § 4 and § 5 were checked against the code and **hold exactly
as written** — they are the most valuable paragraphs in the note and should
not be touched:

- **There is no numbers lane, and `scan_token`/`classify_number` are out of
  scope for the whole series.** Verified: `classify_number` is a
  whole-token classifier called from two sites, and classifying a whole
  token *is* the mechanism by which DIV-0003 holds. Every Phase B step file
  restates this as a standing constraint and B6's file scope forbids the
  two headers outright.
- **Both § 5 traps are real**, and B4's step file additionally confirms
  the ladder half of its own reasoning is now moot for a different reason
  than the trap itself: the retired `many<Capacity>` does stop at capacity
  and *succeed* (`while (result.size() < Capacity)` then a success return),
  where `read_delimited` diagnoses `"too many elements"`. And
  `skip_block_comment` does let an unterminated `#|` consume to end of
  input (`while (!cur.empty() && depth > 0)`), where the recursive
  combinator would fail at the comment. Each is named in the step that
  would break it — B4 and B3 respectively — with the reason it must stay
  worse.

One correction to § 3's D31 that is not about § 4: **D31 names
`reader/oracle_compare.test.cpp`'s SBCL differential as part of the
acceptance witness.** That file was not an SBCL differential; it compared
against `smdlisp`. A4/A5 build a real one; A2 has since deleted the retired
file entirely, along with the rest of `smdlisp`. The witness D31 wants
exists after Phase A and not before, and after this amendment it exists
because the old file is *gone*, not merely superseded.

## Known pre-existing rot, deliberately out of scope

Found while exploring; none of it is caused by this plan and none of it is
in any step's scope. Worth a backlog entry each — `docs/backlog/BL-0005-…`
is the one this amendment actually wrote, for the postponed testinstall/
export work; the rest below are still just notes.

- `schemepoc.org` transcludes `src/smd/schemepoc/*`, a directory that has
  not existed since the rename to `smdscheme`. It is not covered by
  `verify-transclusions.sh`'s default globs, which is why it has gone
  unnoticed.
- `scripts/amalgamate.py` and `scripts/deploy_godbolt_tree.py` both
  hardcode the same dead `src/smd/schemepoc/` path.
- `make testinstall` red on `main`, as above — postponed rather than fixed
  by this plan; `docs/backlog/BL-0005-…` is where it gets fixed.
