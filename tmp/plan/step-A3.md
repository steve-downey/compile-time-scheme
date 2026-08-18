# Step A3 — freeze both iterations as tags, and pin their architecture prose

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. Three front-end trees exist: `src/smd/smdscheme/` (the original Scheme
front end, dead-ended), `src/smd/smdlisp/` (the Common Lisp pivot), and
`src/smd/cl/` (the live rebuild). Phase A retires the first two. This step is
the one that makes the retirement lossless.

**Integration branch: `retire-three-trees`.**

**Reserved for this step:** blog phase **35**, divergence number **DIV-0031**.

## Why

`docs/compiler_architecture.org` is the living architecture document. It carries
**37** org-transclusion links and **31** of them point at files A5 will delete —
seven under `smdscheme`, twenty-one under `smdlisp`, three at examples that go
with them. Only six point at surviving code.

`scripts/verify-transclusions.sh` checks that document against the **worktree**,
deliberately, because a living document is supposed to roll forward and a rename
that orphans an anchor should be caught before the prose goes dead. Delete the
trees with the document as it stands and the check emits 31 `FAIL … does not
exist in the worktree` lines. Suppressing that would be exactly the wrong fix:
the check is right, and the document is what is wrong.

It is wrong in a specific and interesting way. Roughly 230 of its 324 lines
describe two iterations that are finished. That prose does not roll forward and
never will. A document defined as rolling forward is the wrong home for it, and
the project already has the right mechanism for prose about frozen code — the
`orgit-file:` pin the blog posts use, resolved against a tag rather than the
worktree (`docs/blog/pins.md`, `docs/epistolary-pinning-plan.md`).

So this step does not delete the prose. It freezes the code it describes, moves
the prose to a document whose category matches what it is, and repoints its
links at the freeze. After this step the two trees can be deleted from trunk and
every word written about them still resolves.

## Setup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-a3-freeze-iterations -b step-a3-freeze-iterations retire-three-trees
cd ../step-a3-freeze-iterations
git submodule update --init --recursive
```

## Verify GREEN baseline

Cold: about **7 minutes**, **~550 KB** of log. Never read it raw.

```sh
make test-matrix > /tmp/verify-A3-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A3-base.log
./scripts/verify-transclusions.sh
```

Both legs `100% tests passed`. The transclusion check reports **0 failures**
and one recorded `WARN` about a UUID occurring three times in
`phase-12-cps.org` — that warning is expected, is documented in
`docs/blog/pins.md`, and is not a failure. Not green ⇒ `blocked-A3.md`.

## What already exists that this step builds on

- `docs/blog/pins.md` — the tag naming and pinning convention. Existing tags are
  `blog/phase-N`, annotated, one per post. Read the "The mapping" table and the
  paragraph beginning "Phase 21 is the first post whose tag was created *before*
  its prose"; that is the convention you are extending.
- `scripts/verify-transclusions.sh` — two document categories, two policies.
  `posts_glob` (default `docs/blog/phase-*.org`) resolves `[[orgit-file:REPO::REV::PATH::UUID]]`
  links against `REV` with `git show`. `living_glob` (default
  `docs/compiler_architecture.org`) resolves `[[file:PATH::UUID]]` links against
  the worktree, and **fails** if it finds an `orgit-file:` link. `md_globs` is
  already an array; `posts_glob` is a single string.
- Your inbound handoff carries A2's `smdlisp|smdscheme` grep result over
  `src/smd/cl/`, which by now should be comment prose only.

## The change

### 1. Two annotated tags, at this step's merge commit

The trees must still exist at the tagged commit, so tag **after** you merge this
step and **before** A5 deletes anything. Put the exact commands in your handoff
so the orchestrator runs them at the right moment, and put them here in the
commit message too:

```sh
git tag -a iteration/smdscheme-final -m 'The Scheme front end, final state before deletion from trunk.'
git tag -a iteration/smdlisp-final   -m 'The Common Lisp pivot, final state before deletion from trunk.'
```

The `iteration/` prefix is new and deliberate: these are not blog-post pins and
should not sort among `blog/phase-*`. Record the naming and its reason in
`docs/blog/pins.md` under a new heading — that file is where this repository
writes down what a tag means.

**Ordering problem, and how to solve it.** The links you write in part 2 must
name a revision that exists when you write them, but the tags name this step's
own merge commit, which does not exist yet. Do this: create the two tags on your
**worktree branch's** final commit before merging, verify the links resolve
against them, and note in your handoff that the `--no-ff` merge makes the tagged
commit an ancestor of `retire-three-trees`, which is all `git show REV:PATH`
needs. Do not move or re-point the tags afterwards.

### 2. A new pinned document, `docs/history/architecture-iterations.org`

Move, do not rewrite, these parts of `docs/compiler_architecture.org`:

- `* Phase 1: Applicative Parser Combinators` through
  `* Phase 7: Data Reification with C++26 Reflection` — seven top-level
  sections, seven transclusions, all into `smdscheme`.
- `* The Common Lisp Layer: =smd::smdlisp=` and its nine subsections — 24
  transclusions, 21 into `smdlisp` and 3 into
  `src/examples/godbolt_lisp.cpp` and `src/examples/godbolt_lisp_ffi.cpp`,
  which A4 deletes.

Give the new document a short preface saying what it is: prose about two
finished iterations, pinned at `iteration/smdscheme-final` and
`iteration/smdlisp-final`, kept because the argument it makes is still the
argument, and moved out of the living document because it does not roll forward.

Convert every moved link from the living form to the pinned form:

```org
[[file:../src/smd/smdlisp/closure/env.hpp::<uuid>]]
```
becomes
```org
[[orgit-file:<repo>::iteration/smdlisp-final::src/smd/smdlisp/closure/env.hpp::<uuid>]]
```

Take the exact `orgit-file:` spelling — the repository field in particular, and
whether the path is repo-relative — from a working example in
`docs/blog/phase-32-extracting-the-kit.org`. Do not invent the shape; copy it.
Note that pinned paths are **repo-relative** (`src/smd/…`) where living paths
are **document-relative** (`../src/smd/…`), because `git show REV:PATH` takes
the former. Getting that wrong is the likely failure of this step and the
verifier catches it.

What stays in `docs/compiler_architecture.org`: `* Introduction` (reworded to
say the earlier iterations moved, and where), and
`* The Common Lisp Rebuild: =smd::cl=` with its five surviving `cl` and `kit`
transclusions plus whatever A1 and A2 added.

Also fix the two plain `[[file:cl-limitations.md][…]]` hyperlinks if the section
holding them moves — those point at a document, not into a tree, and must keep
resolving from wherever they land.

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
expansion. Keep the `$1` override working — it exists so the failure paths can
be exercised against throwaway fixtures. Update the file's header comment, which
currently says the posts category is `docs/blog/phase-*.org`; it is now two
things and the comment should say which and why.

`make lint` runs `shellcheck`, so a mistake here is caught cheaply.

### 4. Correct the prose that this step falsifies

- `docs/cl-limitations.md` asserts that `docs/compiler_architecture.org` is
  unaffected by arguments about `smdlisp` being frozen. Find that line — it is
  around line 147 — and correct it: the document *did* transclude `smdlisp`, and
  as of this step that prose lives in `docs/history/architecture-iterations.org`,
  pinned. Append a dated note rather than editing in place if the surrounding
  document uses that convention; check the file's own style first.
- `docs/blog/pins.md` gains the `iteration/` heading from part 1.

Do **not** touch `AGENTS.md`, `CLAUDE.md` or the root `checklist.md` here. The
governance change — retiring the "`smdlisp` is never edited" rule — belongs with
the deletion in A5, where it takes effect.

### 5. Anchors

You are moving anchor *references*, not anchors. No source file changes, so no
new `uuidgen` output is needed unless you choose to anchor a region of the new
history document itself, which is not required. Say so in your handoff, so the
post's drafter knows this step's centre is a document move.

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
./scripts/verify-transclusions.sh > /tmp/vt-A3.log 2>&1; echo "exit=$?"
cat /tmp/vt-A3.log
make lint
make test-matrix > /tmp/verify-A3-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A3-after.log
wc -c /tmp/verify-A3-after.log
```

The transclusion check is the real gate for this step, and it is 2 seconds, so
run it early and often rather than only at the end. It must exit 0 and report
**more** resolved transclusions than the baseline did not fewer — the count
should be unchanged at 142 plus whatever A1 and A2 added, because you moved
links rather than dropping them. A drop in the count means a link was lost
silently. The one documented `WARN` remains.

`make test-matrix` cannot regress from a documentation change, but run it: this
step merges on the matrix like every other, and a surprise here would be worth
knowing about.

## Spot checks

```sh
git tag --list 'iteration/*'
git show iteration/smdlisp-final:src/smd/smdlisp/closure/env.hpp | head -3
grep -c 'orgit-file:' docs/history/architecture-iterations.org
grep -c 'file:\.\./src/smd/smdscheme\|file:\.\./src/smd/smdlisp' docs/compiler_architecture.org
```

The last must print **0**: no living link may name either dead tree.

```sh
grep -c 'transclude' docs/compiler_architecture.org docs/history/architecture-iterations.org
```

The two counts must sum to the baseline's 37 plus A1's and A2's additions.

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
docs: move the finished iterations out of the living document

compiler_architecture.org carries 37 transclusions and 31 of them point
into the two trees that are about to leave trunk. The verifier checks
that document against the worktree on purpose -- a living document is
supposed to roll forward, and an orphaned anchor should be caught before
the prose around it goes dead -- so the deletion would have produced 31
honest failures.

The prose is not what is wrong. Roughly 230 of the file's 324 lines
describe two iterations that are finished, and finished prose does not
roll forward. This project already has the mechanism for that case: the
orgit-file: pin the blog posts use, resolved against a tag instead of
the worktree.

So the sections move to docs/history/architecture-iterations.org and
their links repoint at iteration/smdscheme-final and
iteration/smdlisp-final, two annotated tags created here while the code
still exists. verify-transclusions.sh learns the new file belongs to the
pinned category, which is a two-line change because md_globs was already
an array and posts_glob was the odd one out.

Nothing is deleted and nothing is rewritten. After this the trees can go
and every word written about them still resolves.
EOF

git tag -a iteration/smdscheme-final -m 'The Scheme front end, final state before deletion from trunk.'
git tag -a iteration/smdlisp-final   -m 'The Common Lisp pivot, final state before deletion from trunk.'

./scripts/verify-transclusions.sh   # links must resolve against the new tags

git checkout retire-three-trees
git merge --no-ff step-a3-freeze-iterations
```

Do not push the tags; `AGENTS.md` says do not push unless instructed. Name them
in your handoff so the orchestrator pushes them with the branch.

## Record measurements

After the merge, before cleanup.

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"A3","lane":null,"outcome":"green","wall_seconds":<measured>,"attempts":<n>,"verify":{"command":"make test-matrix + verify-transclusions.sh","exit_code":0,"wall_seconds":<measured>,"log_bytes":$(wc -c < /tmp/verify-A3-after.log),"summary_lines_read":<n>},"diff":{"files_changed":<n>,"insertions":<n>,"deletions":<n>},"out_of_scope":[],"note":""}
EOF
```

This step is the plan's clearest test of whether a documentation-only change is
cheap in this repository, so the numbers matter more here than usual. If the
7-minute matrix dominated a change that could not possibly affect it, say so in
`note` — the integration review is looking for exactly that shape.

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-a3-freeze-iterations
```

Mark A3 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff

Read `tmp/plan/step-A4.md`, then write `tmp/plan/handoff-A4.md` (≤ ~150 lines).
A4 retires the examples, the install test, and the exported package. Tell it:
the two tag names, confirmed resolving; that
`src/examples/godbolt_lisp.cpp` and `godbolt_lisp_ffi.cpp` are now transcluded
only from the pinned history document, so deleting them is safe; the exact
`orgit-file:` spelling you used, in case A4 or A5 needs another pin; and any
surprise in `verify-transclusions.sh`'s glob handling.
