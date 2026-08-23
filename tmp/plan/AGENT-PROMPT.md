# Agent prompt (identical for every step)

You are executing **one** step of a planned fan-out in the `compile-time-scheme`
repository at `/home/sdowney/src/steve-downey/compile-time-scheme/main`.

You have no memory of earlier steps and you must not try to acquire one.
Everything you need is on disk in the places named below.

## The three-tier reading contract

This is `AGENTS.md` § "Reading contract" applied to this fan-out.
**Nothing you read may grow with the number of completed steps.**

**Tier 1 — always. Stable, cached, never grows.**

1. `docs/codestyle.org` — authoritative on style; wins any conflict.
2. `AGENTS.md` — process, worktrees, anchors, the blog deliverable.
3. `docs/CODING_RULES.md`
4. `CLAUDE.md`
5. `docs/cpp-rules.md` — read this **last**, immediately before writing code.

**Tier 2 — this step only. Bounded, rewritten per step, consumed once.**

6. `tmp/plan/checklist.md` — find the **first unchecked step**. It names your
   step file exactly; step identifiers are `A0`–`A5` and `B1`–`B8`.
7. `tmp/plan/step-<id>.md` for that step, and **only** that one.
8. `tmp/plan/handoff-<id>.md` addressed to your step, if it exists.
   The first step of a phase has no inbound handoff; that is normal, not a
   defect. Everything below that says `NN` means your step's identifier:
   `blocked-<id>.md`, `amendment-<id>.md`, `handoff-<id>.md`.

**Tier 3 — on demand, by named anchor or section only, never wholesale.**

9. `docs/compiler_architecture.org` — the living architecture doc. Open the
   **named section** your step file points at. Never read it front to back.
10. `docs/cl-rebuild-plan.md`, `docs/cl-language-scoping.md`,
    `docs/cl-parser-scoping.md`, `docs/divergences/DIV-NNNN-*.md`,
    `docs/backlog/BL-NNNN-*.md` — only the specific record or named section your
    step points at. All five directories are large and none of them is a read
    path; `docs/backlog/README.md` says outright that a worker executing a step
    should never open that directory unless told to.

**Do not** read other steps' files, earlier handoffs, `tmp/plan/README.md`,
`tmp/plan/INTEGRATION-REVIEW.md`, or `docs/history/handoff-archive.md`.
**Never read `tmp/plan/metrics.jsonl`** — you append to it and nothing else.

If your step needs a fact it does not have, that is a defect in your inbound
handoff. Record it in the handoff you write, or in `blocked-NN.md`. Do not go
spelunking through `git log` or through earlier steps' files to reconstruct it.

## What you do

1. Read Tier 1, then `checklist.md`, then your step file and your handoff.
2. Create the worktree exactly as your step file says.
3. **Verify the GREEN baseline** before touching anything, with the command your
   step file gives. If the baseline is not green, stop and write `blocked-NN.md`.
4. Implement the change.
5. Verify GREEN again. Run the spot checks.
6. Commit, merge back to the integration branch with `--no-ff`. (A0 is the one
   step whose integration branch is `main` itself; its step file says so.)
7. **Append one record to `metrics.jsonl`** (see below) — after the merge,
   before cleanup, while the numbers and the worktree still exist.
8. Remove the worktree.
9. Mark your line in `tmp/plan/checklist.md` **in the main checkout, by absolute
   path** — `/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.
   Do not mark it inside your worktree; it must not become part of your diff.
10. Read the **next** step's file and write **one** handoff for it,
    `tmp/plan/handoff-NN.md`, in the main checkout by absolute path.

Note there are **two** files called `checklist.md` and they are different things.

`tmp/plan/checklist.md` is this fan-out's step tracker. You mark it in the main
checkout by absolute path and it is **never** part of a step's diff.

The repository root `checklist.md` is a project deliverable. Step A0 adds this
plan's fourteen steps to it; from A1 onward **every step ticks its own line
there, inside its worktree, as part of its diff.** Treat the root
`checklist.md` as a standing addition to your declared file scope — for your
own line and nothing else. It is not an out-of-scope touch and does not go in
`out_of_scope`. Anything more than your own line in that file needs your step
file to say so explicitly (A0 and A3 do).

The reason this is worth a paragraph: `AGENTS.md` says "work only the next
unchecked step in `checklist.md`", and until A0 runs there is no such step. A
root checklist that goes stale for fourteen steps is the defect this plan opens
by fixing.

## Verify commands, and how to read their output

The merge criterion for every step is `make test-matrix` — **both legs**, Debug
(`-O0`) and Asan (`-O3 -fsanitize=address,undefined,leak`). `make test` alone is
the Asan leg only and is **not** sufficient. See `docs/verification-matrix.md`.

The log is **hundreds of kilobytes to half a megabyte** — every ctest entry
printed once per configuration, and it is dominated by test reporting rather
than by compilation, so it does not shrink when your change is small. A fresh
worktree is also a cold build, so the first run is minutes rather than seconds.
**Never read the log raw.** Always redirect and grep:

```sh
make test-matrix > /tmp/verify-NN.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time|Errors while running|FAILED|\*\*\*' /tmp/verify-NN.log
```

Two `100% tests passed` lines, one per leg, is GREEN. On failure, read only the
failing test's block:

```sh
grep -n -A30 'The following tests FAILED' /tmp/verify-NN.log
```

Also required before you declare the step complete:

```sh
make lint                        # ~11s; codespell also scans tmp/plan/
./scripts/verify-transclusions.sh  # ~2s; must exit 0
```

`make lint` runs `codespell` over the whole tree including `tmp/plan/`, so a
typo in a handoff you write will turn the next worker's lint red.

## UUID anchors and the blog deliverable

Every step ships a blog post (decision D20), and **you do not write it**
(`AGENTS.md` § "The blog deliverable"). What you owe is the anchors.

- Land the UUID anchors the step's post will transclude, around the regions that
  are your step's centre, and name them in your handoff.
- New anchors are real `uuidgen` output. `uuidgen` is at `/usr/bin/uuidgen`.
- Per decision D21 an anchor set belongs to the step that lands it. You may
  delete, move, split, merge or nest any existing pair in a file you own. There
  is no duty of care toward an anchor because it already exists, and none toward
  published prose — posts resolve against their own `blog/phase-NN` tag.
- The only surviving obligation is empirical: `scripts/verify-transclusions.sh`
  is green at your merge. Do not restate the anchor rule in stronger form; an
  over-broad "never edit inside an anchor block" has twice made a worker design
  around a constraint that does not exist.

Your step file reserves a blog phase number and a `DIV-NNNN` number for you.
Use those and no others, so concurrent lanes cannot collide on the next number.
An unused reservation is fine and expected.

## Commit messages

Explain **why**, not just what. Your step file gives a HEREDOC draft — it is a
**draft**, not final text. Per `AGENTS.md`, commit and PR messages are reviewed
by a clean agent running the `voice` skill before use; the orchestrator arranges
that. Write the message, do not polish it into someone else's voice.

## Incidental fixes outside your declared file scope

Your step declares the files it expects to touch. That declaration exists so
widening is **visible**, not so it is **forbidden**.

The mechanical tell: **you are about to write more code to avoid a change than
the change itself would take.** An adapter, a wrapper, a duplicated function
differing in one detail, a cast that launders a type, a comment explaining why
you could not just do X — all the same signal. When it fires, classify:

### Pre-authorized in this repository — make it, minimally, record it

These cannot alter behaviour for any existing caller in this tree:

- Adding `const`, `constexpr`, `noexcept`, or `[[nodiscard]]` to a declaration
  where it is correct and every existing call site already satisfies it.
- Adding a missing `#include` to make a header self-contained. The project
  forbids relying on transitive includes, so this is always a fix, never a
  change. Canonical angle-bracket spelling only.
- Adding a file to an existing `FILE_SET HEADERS` / `target_sources` list in the
  `CMakeLists.txt` of the directory that file lives in, when you created the file.
- Adding a `.test.cpp` to the `add_executable`/`target_sources` list of the test
  target in its own directory.
- Reformatting only what `clang-format` or `gersemi` rewrites when you run
  `make lint` on a file you already edited. Do not run a formatting sweep over
  files you did not otherwise touch.
- Fixing a `codespell` hit that `make lint` reports in a file you already edited.
- Correcting a file-prolog path comment that disagrees with the file's actual
  repo-relative path.
- Widening a parameter you already call through from a concrete type to the
  concept it already models, when no other caller exists.
- Correcting a header-guard macro that disagrees with the file's repo-relative
  path, per `docs/cpp-rules.md`'s guard rule. (`src/smd/fixpoint/` uses
  `INCLUDED_SMD_FIXPOINT_*` by local convention and is **not** wrong.)
- Fixing a `markdownlint` violation `make lint` reports in a Markdown or org
  file you already edited.
- Adding the index-table row in `docs/divergences/README.md` or
  `docs/backlog/README.md` for a `DIV-` or `BL-` file you created in this step.
  Both directories require the row; creating the file without it is the
  incomplete edit, not the row.

Two documentation conventions are **not** incidental-fix territory and getting
them backwards is the most likely way to make this list do harm:

- `docs/divergences/**` is **append-only**. A dated note, never an edit in
  place. Its `README.md` index table is the exception — that is a table and
  you edit the row.
- `docs/backlog/**` is the opposite. Its own `README.md` says "Refine an item
  in place. Do not append status logs to it."

Record each pre-authorized fix in three places: your handoff, the commit
message, and the `out_of_scope` array of your `metrics.jsonl` record. The
standing root-`checklist.md` tick is the one exception and does not go there.

### Ask — halt and request permission

Anything that could change behaviour for an existing caller, alters an installed
or exported target, changes a diagnostic string, touches
`src/smd/smdlisp/**` (never edited, it is the behavioural oracle), or touches a
file another lane owns. You know the fix and it is small; you just do not own
the file. Say what you want to change, why the in-scope alternative is worse, and
how large each is. Running unattended with no answer, write `blocked-NN.md`.

### Never silently work around

A halted step is cheap to resume. A large workaround is expensive to unwind and
may never be found at all. Halting is the correct outcome, not a failure.

## Amending an earlier step's design

Earlier steps speculated about what you would need. Some of those guesses are
wrong, and **finding one wrong is the job, not a failure**. You are the first
real consumer; a consumer is the only thing that can produce that evidence.

**Never fork a shared abstraction.** Introducing a parallel type, module, or
helper that duplicates an existing one with a variation is never the answer, no
matter how well named or how local. Prefer, in order: extend the existing
abstraction in place; propose an amendment; halt. Forking is worse than halting.

To propose an amendment, write `tmp/plan/amendment-NN.md` and **stop before you
build around the gap**. State the abstraction, the concrete thing your step
needs that it lacks, why the in-scope alternative is worse, which earlier steps'
code would have to change, and a rough size. Base it on a need you have in hand
right now, not on an improvement a future step might want.

**An amendment is not a block.** `blocked-NN.md` means stuck — attempts failed,
the way forward is unclear. An amendment means the opposite: the way forward is
clear and the plan is what needs adjusting. They halt the chain the same way and
mean opposite things; mislabelling one buries the run's most useful signal.

## When to stop

Stop, do **not** mark the checklist, and write `tmp/plan/blocked-NN.md` if:

- the step has not reached GREEN after **3 genuinely different** attempts; or
- you are about to weaken the verification to make it pass — skipping a test,
  suppressing a warning, loosening an assertion, commenting out failing code,
  changing an existing test's expectation; or
- you would have to edit `src/smd/smdlisp/**`, which is never edited **until
  step A3 deletes it**. Before A3, editing it is a halt. From A3 onward the
  tree does not exist and the question cannot arise. Do not read this rule as
  permission to reach into `src/smd/smdscheme/**` either: it is not frozen by
  rule, but `AGENTS.md` is explicit that a bug found there is fixed in
  `src/smd/cl/`, never there.

Changing an existing test's expectation is a **halt**, always. Adding a new case
is fine.

Append a `metrics.jsonl` record with `"outcome": "blocked"` **before** removing
the worktree — cleanup would otherwise destroy exactly the measurements that
explain what went wrong.

Repetition is the thrashing signal, not raw token count. A step whose verify
loop is inherently expensive can legitimately spend a lot on verification alone;
that is not thrashing. Three failed attempts, or output that keeps growing
without new information, is.

## The handoff you write

Forward-only, rewritten fresh (never appended), consumed once, **≤ ~150 lines**.
Write it to the next step's number: `tmp/plan/handoff-NN.md`.

It contains only:

- the next step's goal and merge criterion, if its own step file does not fix them;
- the exact files that step will touch;
- dependencies already satisfied, **referenced by anchor** into
  `docs/compiler_architecture.org` — not re-narrated;
- what *this* step discovered that the next one needs and could not get from its
  own step file: a deviation, a surprise, a changed assumption, a gotcha;
- any out-of-scope file you touched, because the next step may rely on the old shape.

It is **not** a summary of what you did (that is the commit message and the
checklist), **not** architecture (that is `docs/compiler_architecture.org`,
updated in place by anchor), and **not** history (that is `git log`). If it reads
like a log, it has the wrong contents. **No measurements** — those go in
`metrics.jsonl` and nowhere else.

## metrics.jsonl

Append **one** line, on success and on block alike. The path is absolute and
outside every worktree, deliberately:

```
/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl
```

A relative path would resolve inside your worktree, where the record becomes
part of your diff and a blocked step's own `git worktree remove --force`
destroys it.

```json
{"step":"A1","lane":null,"outcome":"green","wall_seconds":0,"attempts":1,
 "verify":{"command":"make test-matrix","exit_code":0,"wall_seconds":0,
           "log_bytes":0,"summary_lines_read":6},
 "diff":{"files_changed":0,"insertions":0,"deletions":0},
 "out_of_scope":[],"note":""}
```

Every field is mechanically observed, never estimated: wall time from
`date +%s` around each phase, `log_bytes` from `wc -c` on the redirected verify
log, `diff` from `git diff --shortstat` against your step's base commit.
`note` is one line and only when something surprised you.

**Do not record your own token usage.** You cannot introspect it accurately and
a fabricated number would silently corrupt the calibration this log exists to
produce. The fields above are deliberately chosen as observable proxies.
