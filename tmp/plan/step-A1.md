# Step A1 — freeze both iterations as tags, and pin their architecture prose

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. Three front-end trees exist: `src/smd/smdscheme/` (the original Scheme
front end, dead-ended), `src/smd/smdlisp/` (the Common Lisp pivot), and
`src/smd/cl/` (the live rebuild). Phase A retires the first two, **now, as its
first act** — see "Why this step is first" below — and A3 deletes them.

**Integration branch: `cl-retire-trees`,** created off `main` by the
orchestrator after A0 merged. **If it does not exist, stop and write
`blocked-A1.md`** — that is the gate between A0 and Phase A, not a defect. Do
**not** substitute the older `retire-three-trees` branch: it points at
`1ace4ad`, nine commits behind `main`, and branching from it would silently
lose the Monad typeclass and both scoping notes that this plan is built on.

**Reserved for this step:** blog phase **34**, divergence number **DIV-0030**.
An unused reservation is fine.

## Why this step is first

The original decomposition of this plan built a replacement reader oracle
(printer plus SBCL differential) before retiring anything, on the theory that
the old oracle should not go away before the new one exists. That theory is
wrong for this repository, for a reason that has nothing to do with oracles:
**the owner's overriding priority is that agents keep getting hung up on not
touching the dead trees and doing strategically wrong things, sometimes
silently, while those trees are still in the worktree.** That cost dominates
every other consideration in this plan's ordering. The deletion goes first.

It cannot go completely first, though, without breaking something this
project already promises: `docs/compiler_architecture.org` is a **living**
document, and `scripts/verify-transclusions.sh` deliberately resolves its
links against the worktree, not against a tag, because a living document is
supposed to roll forward and an orphaned link should be caught immediately.
Counted directly on `main` at `1d66c54`, and **count it the same way yourself
before you trust any number here**, because the obvious grep answers a
different question than the one that matters:

```sh
grep -o '\[\[file:' docs/compiler_architecture.org | wc -l          # 39
grep -o '\[\[file:[^]:]*::' docs/compiler_architecture.org | wc -l  # 37
```

**39 `[[file:` occurrences, of which 37 are code transclusions** — the ones
carrying a `::<uuid>` — and **2 are plain document hyperlinks** to
`cl-limitations.md`, which have no UUID and are not transclusions at all. An
earlier pass at this plan reported 39 transclusions; it was counting the first
grep. The verifier only cares about the 37.

Of those 37, **31 point at code A3 is about to delete**: 7 under
`src/smd/smdscheme/`, 21 under `src/smd/smdlisp/`, and 3 at
`src/examples/godbolt_lisp.cpp` / `godbolt_lisp_ffi.cpp`, which A2 removes. The
remaining **6** point into `src/smd/cl/` and `src/smd/kit/` and stay. Delete the
trees with the document as it stands and the very next
`verify-transclusions.sh` run — which every later
step's acceptance gate runs — reports 31 failures. Suppressing that check
would be exactly the wrong fix: the check is right, and the document is what
would be wrong.

So this step, and only this step, precedes the deletion. It does not delete
anything and it does not touch a line of C++. It freezes the code the
soon-to-be-dead prose describes, moves that prose to the document category
this project already has for finished, non-rolling-forward writing, and
repoints its links at the freeze. After this step the two trees can be
deleted and every word ever written about them still resolves.

## Setup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-a1-freeze-iterations -b step-a1-freeze-iterations cl-retire-trees
cd ../step-a1-freeze-iterations
git submodule update --init --recursive
```

## Verify GREEN baseline

Your inbound handoff from A0 carries the exact wall time and ctest count it
measured. The number to expect is **1123** entries per leg; A0 changed no code,
so nothing should have moved it.

```sh
make test-matrix > /tmp/verify-A1-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A1-base.log
./scripts/verify-transclusions.sh
```

Both legs `100% tests passed`. The transclusion check exits 0, reporting
**142** resolved transclusions and **1** recorded `WARN` about a UUID
occurring three times in `phase-12-cps.org` — that warning is expected, is
documented in `docs/blog/pins.md`, and is not a failure. Not green ⇒ write
`blocked-A1.md`.

## What already exists that this step builds on

- `docs/blog/pins.md` — the tag naming and pinning convention. Existing tags
  are `blog/phase-N`, annotated, one per post. Read the "The mapping" table
  and the paragraph beginning "Phase 21 is the first post whose tag was
  created *before* its prose"; that is the convention you are extending.
- `scripts/verify-transclusions.sh` — two document categories, two policies.
  `posts_glob` (default `docs/blog/phase-*.org`) resolves
  `[[orgit-file:REPO::REV::PATH::UUID]]` links against `REV` with `git show`.
  `living_glob` (default `docs/compiler_architecture.org`) resolves
  `[[file:PATH::UUID]]` links against the worktree, and **fails** if it finds
  an `orgit-file:` link. `md_globs` is already an array; `posts_glob` is a
  single string.
- `docs/compiler_architecture.org`, as it stands: `* Phase 1: Applicative
  Parser Combinators` through `* Phase 7: Data Reification with C++26
  Reflection` — seven top-level sections, seven transclusions, all into
  `smdscheme` — followed by `* The Common Lisp Layer: =smd::smdlisp=` and its
  nine subsections, 24 transclusions, 21 into `smdlisp` and 3 into
  `src/examples/godbolt_lisp.cpp`/`godbolt_lisp_ffi.cpp`. Seven plus 24 is the
  31 that move. What survives: `* Introduction` and `* The Common Lisp
  Rebuild: =smd::cl=` with its **six** `cl` and `kit` transclusions.

## The change

### 1. Two annotated tags, at this step's merge commit

The trees must still exist at the tagged commit, so tag **after** you merge
this step and **before** A3 deletes anything. Put the exact commands in your
handoff so the orchestrator runs them at the right moment, and put them here
in the commit message too:

```sh
git tag -a iteration/smdscheme-final -m 'The Scheme front end, final state before deletion from trunk.'
git tag -a iteration/smdlisp-final   -m 'The Common Lisp pivot, final state before deletion from trunk.'
```

The `iteration/` prefix is new and deliberate: these are not blog-post pins
and should not sort among `blog/phase-*`. Record the naming and its reason in
`docs/blog/pins.md` under a new heading.

**Ordering problem, and how to solve it.** The links you write in part 2 must
name a revision that exists when you write them, but the tags name this
step's own merge commit, which does not exist yet. Do this: create the two
tags on your **worktree branch's** final commit before merging, verify the
links resolve against them, and note in your handoff that the `--no-ff` merge
makes the tagged commit an ancestor of `cl-retire-trees`, which is all
`git show REV:PATH` needs. Do not move or re-point the tags afterwards.

### 2. A new pinned document, `docs/history/architecture-iterations.org`

Move, do not rewrite, the two blocks named above out of
`docs/compiler_architecture.org`.

Give the new document a short preface saying what it is: prose about two
finished iterations, pinned at `iteration/smdscheme-final` and
`iteration/smdlisp-final`, kept because the argument it makes is still the
argument, and moved out of the living document because it does not roll
forward.

Convert every moved link from the living form to the pinned form:

```org
[[file:../src/smd/smdlisp/closure/env.hpp::<uuid>]]
```

becomes

```org
[[orgit-file:<repo>::iteration/smdlisp-final::src/smd/smdlisp/closure/env.hpp::<uuid>]]
```

Take the exact `orgit-file:` spelling — the repository field in particular,
and whether the path is repo-relative — from a working example in
`docs/blog/phase-32-extracting-the-kit.org`. Do not invent the shape; copy it.
Note that pinned paths are **repo-relative** (`src/smd/…`) where living paths
are **document-relative** (`../src/smd/…`), because `git show REV:PATH` takes
the former. Getting that wrong is the likely failure of this step and the
verifier catches it.

What stays in `docs/compiler_architecture.org`: `* Introduction` (reworded to
say the earlier iterations moved, and where), and `* The Common Lisp Rebuild:
=smd::cl=` with its five surviving `cl` and `kit` transclusions.

**The two plain `[[file:cl-limitations.md][…]]` hyperlinks are a trap.** They
carry no UUID, so `verify-transclusions.sh` never looks at them and cannot tell
you if you break them. Both sit in the `smdlisp` section, so both move — and
the new document lives one directory deeper, in `docs/history/`, which means a
document-relative `cl-limitations.md` becomes `../cl-limitations.md`. Fix them
by hand and check them by hand; nothing downstream will.

### 3. Teach the verifier about the new document

`scripts/verify-transclusions.sh` must check the new file in the **posts**
category, not the living one. Turn `posts_glob` into a `posts_globs` array in
the same idiom the file already uses for `md_globs`:

```sh
if [[ -n "${1:-}" ]]; then
    posts_globs=("$1")
else
    posts_globs=('docs/blog/phase-*.org' 'docs/history/architecture-iterations.org')
fi
```

and iterate it the way `md_globs` is iterated, keeping the existing
`shellcheck disable=SC2048` comment pattern and the deliberate unquoted
expansion. Keep the `$1` override working. Update the file's header comment,
which currently says the posts category is `docs/blog/phase-*.org`; it is now
two things and the comment should say which and why.

`make lint` runs `shellcheck`, so a mistake here is caught cheaply.

### 4. Correct the prose that this step falsifies

- `docs/cl-limitations.md` asserts that `docs/compiler_architecture.org` is
  unaffected by `smdlisp` being frozen. Find that line — it is around line
  147 — and correct it: the document *did* transclude `smdlisp`, and as of
  this step that prose lives in `docs/history/architecture-iterations.org`,
  pinned. Append a dated note rather than editing in place if the file's own
  convention is append-only; check its style first.
- `docs/blog/pins.md` gains the `iteration/` heading from part 1.

Do **not** touch `AGENTS.md`, `CLAUDE.md`, the root `checklist.md` beyond
ticking your own A1 line, `docs/cl-rebuild-plan.md`, or either scoping note.
The governance change splits between two other steps and neither half is
yours: A0 already wrote decision **D32**, which retires `smdlisp`'s
never-edited rule, and A3 executes it in the four documents that state the
rule. Restating it here would leave three documents disagreeing for two
steps.

### 5. Anchors

You are moving anchor *references*, not anchors. No source file changes, so
no new `uuidgen` output is needed unless you choose to anchor a region of the
new history document itself, which is not required. Say so in your handoff.

## Declared file scope

```
docs/compiler_architecture.org                     (sections removed; Introduction reworded)
docs/history/architecture-iterations.org           (new; the moved sections, repinned)
docs/blog/pins.md                                  (new heading for iteration/ tags)
docs/cl-limitations.md                             (one corrected claim)
scripts/verify-transclusions.sh                    (posts_globs array + header comment)
```

No C++ changes. Two git tags, created on the branch tip before merge.

## Verify GREEN after

```sh
./scripts/verify-transclusions.sh > /tmp/vt-A1.log 2>&1; echo "exit=$?"
cat /tmp/vt-A1.log
make lint
make test-matrix > /tmp/verify-A1-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A1-after.log
wc -c /tmp/verify-A1-after.log
```

The transclusion check is the real gate for this step and it is 2 seconds, so
run it early and often. It must exit 0 and report the same 142 resolved
transclusions as the baseline — you moved links, you did not drop any. A drop
in the count means a link was lost silently. The one documented `WARN`
remains. `make test-matrix` cannot regress from a documentation change, but
run it: this step merges on the matrix like every other.

## Spot checks

```sh
git tag --list 'iteration/*'
git show iteration/smdlisp-final:src/smd/smdlisp/closure/env.hpp | head -3
grep -c 'orgit-file:' docs/history/architecture-iterations.org
grep -c 'file:\.\./src/smd/smdscheme\|file:\.\./src/smd/smdlisp' docs/compiler_architecture.org
```

The last must print **0**: no living link may name either dead tree.

```sh
grep -o '\[\[file:[^]:]*::' docs/compiler_architecture.org | wc -l   # expect 6
grep -o 'orgit-file:' docs/history/architecture-iterations.org | wc -l # expect 31
grep -c 'cl-limitations.md' docs/compiler_architecture.org \
        docs/history/architecture-iterations.org             # expect 2 across both
```

Six surviving transclusions plus 31 moved is the 37 you started with. Note the
greps count **occurrences**, not matching lines — `grep -c` counts lines and
would give you a different and wrong answer. Reconcile rather than adjust if
either number has drifted: a link that vanishes silently is the one failure
this step can have that nothing downstream will catch.

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
docs: move the finished iterations out of the living document, first

This plan retires both dead trees before it does anything else --
that ordering, not the order this fan-out originally shipped with, is
the point, because the cost of agents getting hung up on trees they
must not touch outweighs every sequencing argument that put a
replacement oracle first.

compiler_architecture.org carries 37 transclusions and 31 of them
point into the two trees that are about to leave trunk. The verifier
checks that document against the worktree on purpose -- a living
document is supposed to roll forward, and an orphaned anchor should be
caught before the prose around it goes dead -- so deleting the trees
with the document as it stands would have produced 31 honest failures
on the very next run.

The prose is not what is wrong. This project already has the
mechanism for finished writing about frozen code: the orgit-file: pin
the blog posts use, resolved against a tag instead of the worktree. So
the sections move to docs/history/architecture-iterations.org and
their links repoint at iteration/smdscheme-final and
iteration/smdlisp-final, two annotated tags created here while the
code still exists. verify-transclusions.sh learns the new file belongs
to the pinned category, a two-line change because md_globs was already
an array and posts_glob was the odd one out.

Nothing is deleted and nothing is rewritten. After this the trees can
go and every word written about them still resolves.
EOF

git tag -a iteration/smdscheme-final -m 'The Scheme front end, final state before deletion from trunk.'
git tag -a iteration/smdlisp-final   -m 'The Common Lisp pivot, final state before deletion from trunk.'

./scripts/verify-transclusions.sh   # links must resolve against the new tags

git checkout cl-retire-trees
git merge --no-ff step-a1-freeze-iterations
```

Do not push the tags; `AGENTS.md` says do not push unless instructed. Name
them in your handoff so the orchestrator pushes them with the branch.

## Record measurements

After the merge, before cleanup.

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"A1","lane":null,"outcome":"green","wall_seconds":<measured>,"attempts":<n>,"verify":{"command":"make test-matrix + verify-transclusions.sh","exit_code":0,"wall_seconds":<measured>,"log_bytes":$(wc -c < /tmp/verify-A1-after.log),"summary_lines_read":<n>},"diff":{"files_changed":<n>,"insertions":<n>,"deletions":<n>},"out_of_scope":[],"note":""}
EOF
```

If the 409-second-class matrix dominated a change that could not possibly
affect it, say so in `note` — the integration review is looking for exactly
that shape.

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-a1-freeze-iterations
```

Mark A1 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff

Read `tmp/plan/step-A2.md`, then write `tmp/plan/handoff-A2.md` (≤ ~150
lines). A2 neutralises every build consumer of the dead trees — examples,
the install test, the exported package — so the deletion in A3 is a pure
subtraction. Tell it: the two tag names, confirmed resolving; that
`src/examples/godbolt_lisp.cpp` and `godbolt_lisp_ffi.cpp` are now
transcluded only from the pinned history document, so deleting them outright
(A2 does not replace them — that is `docs/backlog/BL-0005-…`) breaks no
living link; the exact `orgit-file:` spelling you used, in case A2 or A3
needs another pin; and any surprise in `verify-transclusions.sh`'s glob
handling.
