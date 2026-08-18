# Execution plan: retire the three trees, then adopt parser combinators

**Not for worker agents.** A cleared worker reads `AGENT-PROMPT.md`,
`checklist.md`, its own `step-NN.md`, and its own inbound handoff. This file is
for the orchestrator and for the owner. It is not on any worker's read path,
which is why it is allowed to be discursive.

Produced 2026-08-17. Supersedes `tmp/plan-draft-v1/`, which was provisional
scaffolding.

---

## The shape

Two phases, thirteen implementation steps, one integration review.

**Phase A retires the three-tree structure.** It exists because it makes Phase
B dramatically smaller, and it must land first.

| step | one line | blog | DIV |
|---|---|---|---|
| A1 | a `prin1`-shaped printer for `cl` datums, proved against SBCL in the same step | 33 | 0029 |
| A2 | the reader-layer cl-vs-SBCL differential replaces the `smdlisp` one | 34 | 0030 |
| A3 | freeze both iterations as tags; move their architecture prose to a pinned history doc | 35 | 0031 |
| A4 | re-point the examples, the install test, and the exported package | 36 | 0032 |
| A5 | delete `smdscheme` and `smdlisp` from trunk | 37 | 0033 |

**Phase B adopts parser combinators in `cl`'s reader.**

| step | one line | blog | DIV |
|---|---|---|---|
| B1 | split `read.hpp` into component headers plus an umbrella | 38 | 0034 |
| B2 | `smd::kit::parser`: threaded context and `bind`, with `read_radix_number` as first client | 39 | 0035 |
| B3 | intertoken space and comments onto the layer | 40 | 0036 |
| B4 | choice and bounded repetition; `read_delimited` retires the D15 ladder | 41 | 0037 |
| B5 | text: strings and character literals | 42 | 0038 |
| B6 | forms: the quote family and token data | 43 | 0039 |
| B7 | sharpsign dispatch and `read_node` | 44 | 0040 |
| B8 | close the series: the D30 measurement, DIV-0028 dissolved, the architecture section | 45 | 0041 |

Blog phase numbers continue from 32 and divergence numbers from 0028; both are
reserved **per step up front** so nothing can collide on the next free number.
An unused reservation is expected and fine.

## Why Phase A first, and why it is not just tidying

`cl` reaches into the dead trees in exactly one place — `oracle_compare.test.cpp`
and the `smdlisp.reader` link line beside it. Everything else that mentions
`smdscheme` or `smdlisp` under `src/smd/cl/` is comment prose. So the coupling
that made the parser work look large was not real coupling.

What deleting the trees buys Phase B is that the parser layer has **exactly one
consumer**. Three consequences follow, and none of them should be re-litigated
inside a step:

- **D29 is answered: thread the context.** A parser is
  `parse_result<T>(cursor, Ctx&)` over a named `Ctx` concept, not
  `parse_result<T>(cursor)` with `&ctx` captured. Combinators reify parsers into
  storable values, so a captured `&ctx` can outlive its context in a way the
  current per-call `read_node(cur, ctx)` cannot, and DIV-0007 is this project's
  own record of paying for that bug class once. `no_context` may exist on its
  merits but is never a default argument; there is no compatibility to keep.
- **DIV-0028 dissolves rather than closing.** "The parser combinator layer has
  no `cl` client" was an artifact of keeping two dead front ends. B8 records
  that it ceased to apply. No step's deliverable is "close DIV-0028".
- **The draft's "cl retires its duplicates, cursor.hpp becomes a shim" step does
  not exist as a step.** There is no third tree to be a duplicate of. B2 moves
  `cl::reader::cursor` into the kit and leaves an R8-style shim, deliberately
  and in one paragraph, because keeping the name alive is what makes D31's
  "existing tests pass unchanged" checkable by diff.

## Integration branches — two, not one

**Phase A: `retire-three-trees`, created off `main` (exists).**
**Phase B: `cl-parser-combinators`, created off `main` by the orchestrator after
Phase A merges. It does not exist yet, deliberately.**

The reason is D31. Phase B's central claim is that it changes no observable
behaviour, and its acceptance witness is that the existing reader tests and the
conformance corpus pass **unchanged**. Phase A deliberately changes the test
population — it retires `oracle_compare.test.cpp`, adds a printer and a
differential, and deletes about 800 test cases with the two trees. On one shared
branch the "unchanged" claim is unverifiable by diff. On two, B8 can check it
mechanically:

```sh
git diff --stat $(git merge-base main cl-parser-combinators)..HEAD \
    -- 'src/smd/cl/**/*.test.cpp' 'src/smd/cl/conformance/'
```

The gap between them is also a deliberate human gate. Phase A deletes 30,000
lines and retires a rule `AGENTS.md` states as a rule; that is worth a look
before Phase B builds on it. Step B1 is told to write `blocked-B1.md` if
`cl-parser-combinators` is absent, so an unattended run halts at the gate rather
than improvising.

The current branch, `draft-parser-combinator`, holds `docs/cl-language-scoping.md`
and this plan. It is not an integration branch for either phase.

## Baseline, measured 2026-08-17 on this machine

GCC 16.1.1, `TOOLCHAIN=gcc-16`, WSL2.

| measurement | value |
|---|---|
| `make test-matrix`, **cold worktree**, both legs | **409 s** |
| `make test-matrix`, cold, log size | **546,966 bytes** |
| `make test-matrix`, warm after touching `read.hpp` | 40 s |
| ctest entries, per leg | **1118**, 100% passing |
| `make lint` | 11 s |
| `scripts/verify-transclusions.sh` | 2 s, 142 resolved, 1 recorded WARN, 0 failures |
| `make testinstall` | 109 s, **exits 2 — pre-existing failure** |
| `make testinstall` log size | 7,386,663 bytes |

Two of these shape the whole plan.

**The verify log is half a megabyte regardless of what changed**, because it is
1118 ctest lines twice. It is dominated by test *reporting*, not by compilation,
so it does not shrink when a step is small. Every step file therefore tells the
agent to redirect and grep and never to read it raw. `make testinstall`'s log is
**seven megabytes** of template diagnostics and is even more dangerous.

**A fresh worktree is a cold build**: `.build/` lives inside the source tree, so
every step pays ~409 s twice — once for the baseline and once after. That is
~14 minutes of verify per step before any thinking, and it is the reason steps
are sized the way they are. Phase A's deletion removes roughly 800 of the 1118
tests, so Phase B's steps should be substantially cheaper; A5's handoff is
required to carry the new number, because it is what B1–B8 are actually sized
against.

**`make testinstall` is already red on `main`**, with
`smd/smdscheme/closure/pairs.hpp: No such file or directory` — a header never
added to the installed `FILE_SET`. It is a defect in a tree being deleted. A4
should turn it green as a side effect of re-pointing the install tests at `cl`
and `kit`, and A4's step file says so; no step is asked to fix it in place.

## No prior calibration exists

There is no `metrics/` directory in this repository and the project's memory
directory holds no fan-out calibration entry. **This run's step sizing is the
baseline being measured, not a sizing against measured numbers.** The
integration review should read it that way: the question is not "did the
estimates hold" — there were no calibrated estimates — but "what does a step
cost here, and where did the guesses miss."

The one thing that *was* measured before decomposing is the table above, and
step sizing was driven by it: expensive verify, so fewer and more coherent
steps rather than many tiny ones, because splitting a step gated by a 409-second
cold build pays that cost again for no gain.

## Acceptance commands the orchestrator runs after each merge

The worker's GREEN was pre-merge and in isolation. These check a state that did
not exist when the worker ran them; they are not a re-run of its verification.

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
than cold, so it costs tens of seconds rather than 409.

**Per step where the step file says so:** `make compile-headers` (B1 onward,
because `CMAKE_VERIFY_INTERFACE_HEADER_SETS` is the check that the new headers
are genuinely self-contained), and `make testinstall` (A4 and A5 only).

**Once per fan-out, not per step:** `make coverage` is an instrumented rebuild
under the `Gcov` profile and is not cheap; run it after B8, not after each
merge. `make docs` needs `mrdocs` in `.tools/` and may not be installed here;
A5 owns the configuration it depends on and is told to try it and report.

## The two files called `checklist.md`

`tmp/plan/checklist.md` is this fan-out's step tracker. Workers mark it **in the
main checkout by absolute path**, so it never enters a step's diff and never
becomes a merge point.

The repository root `checklist.md` is a project deliverable and is edited inside
a worktree only where a step file says so — A5 and B8.

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

That document is a **proposal awaiting ratification** and nothing here edits it.
Its § 4 phase sketch was written before the steps were, and writing them against
the code found five things wrong with it. Collected here so they can be acted on
in one pass.

**1. § 4's P1–P10 assumed the three-tree repository. It should become a P-series
that follows a precursor phase.** The sketch's shape — narrow, wide, narrow —
survives; its content does not, because most of what made it wide was keeping
two dead front ends alive.

**2. P3 should be deleted.** "`cl` retires its duplicates; `cursor.hpp` becomes
a kit shim" presupposes a duplicate. There is no third tree.
`src/smd/cl/reader/cursor.hpp` includes only
`<smd/cl/foundation/source_pos.hpp>` and is fully independent, so there is
nothing to reconcile — only a decision about where the one cursor lives. It is
one paragraph inside the step that introduces the layer, not a phase.

**3. P1 and P2 should not exist as written.** "Kit parser core (serial)" followed
by "kit choice and facade (serial)" builds the entire layer in two steps and
first uses it in P5 or P6, which maximises the number of guesses validated too
late. The executed plan adds one primitive at a time, each in the same step as
the reader function that needs it: `bind` with `read_radix_number`, `skip_many`
with the intertoken skippers, choice and bounded repetition with
`read_delimited`. If the layer stops growing after B4, that is a fact worth
having, and the layered version could not have produced it.

**4. There is no `parser_ops` facade in the executed plan, and § 4's P2 should
stop promising one.** The retired `ParserApplicative`/`ParserAlternative`
typeclass-object facade had exactly one client, `smdscheme`, which no longer
exists. Rebuilding it for a layer with one consumer would be R8's lesson
repeated. If a facade is wanted later, it wants a second client first.

**5. § 4's "Every step ships a post … phases 33–42 and DIV-0029 through
DIV-0033" needs renumbering.** With the precursor phase there are thirteen
steps: blog phases 33–45 and DIV-0029 through DIV-0041.

Two things in § 4 and § 5 were checked against the code and **hold exactly as
written** — they are the most valuable paragraphs in the note and should not be
touched:

- **There is no numbers lane, and `scan_token`/`classify_number` are out of
  scope for the whole series.** Verified: `classify_number` is a whole-token
  classifier called from two sites, and classifying a whole token *is* the
  mechanism by which DIV-0003 holds. Every Phase B step file restates this as a
  standing constraint and B6's file scope forbids the two headers outright.
- **Both § 5 traps are real.** The retired `many<Capacity>` does stop at
  capacity and *succeed* (`while (result.size() < Capacity)` then a success
  return), where `read_delimited` diagnoses `"too many elements"`. And
  `skip_block_comment` does let an unterminated `#|` consume to end of input
  (`while (!cur.empty() && depth > 0)`), where the recursive combinator would
  fail at the comment. Each is named in the step that would break it — B4 and
  B3 respectively — with the reason it must stay worse.

One correction to § 3's D31 that is not about § 4: **D31 names
`reader/oracle_compare.test.cpp`'s SBCL differential as part of the acceptance
witness.** That file is not an SBCL differential; it compares against
`smdlisp`. A2 retires it and replaces it with a real SBCL one. The witness D31
wants exists after Phase A and not before.

## Governance decisions this plan assumes, which are the owner's to confirm

**Deleting `src/smd/smdlisp/**` retires a stated rule.** `AGENTS.md` § "Which
trees you may edit" says it "is the behavioural oracle for the rebuild and is
**never edited**, from step R1 onward. That one is the rule", and the root
`checklist.md` says the same. A5 retires it in all four documents that state it
and records decision **D32** for the retirement.

The plan's argument is that the rule protected a *role*: A2 gives that role to
SBCL, which is what D16 asked for in the first place, and A3's
`iteration/smdlisp-final` tag is a stronger freeze than a rule because it cannot
be slipped. It is also mechanically forced — every `smdlisp.*` library links
`smdscheme.foundation`, so the two trees cannot be separated.

That reasoning is offered, not assumed to be accepted. It is the one thing in
Phase A that is a judgement rather than an execution, and the gate between the
phases is where to say no.

**D32, not D22.** `docs/cl-language-scoping.md` proposes D22–D26 and
`docs/cl-parser-scoping.md` proposes D27–D31, both unratified. A ratified
decision taking D22 would silently renumber a document nobody is editing, so A5
records D32 and says why in one sentence.

## Known pre-existing rot, deliberately out of scope

Found while exploring; none of it is caused by this plan and none of it is in
any step's scope. Worth a backlog entry each.

- `schemepoc.org` transcludes `src/smd/schemepoc/*`, a directory that has not
  existed since the rename to `smdscheme`. It is not covered by
  `verify-transclusions.sh`'s default globs, which is why it has gone unnoticed.
- `scripts/amalgamate.py` and `scripts/deploy_godbolt_tree.py` both hardcode the
  same dead `src/smd/schemepoc/` path.
- `make testinstall` red on `main`, as above — this one A4 fixes incidentally.
