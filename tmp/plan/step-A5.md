# Step A5 — delete `smdscheme` and `smdlisp` from trunk

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. It has carried three front-end trees: `src/smd/smdscheme/` (Scheme,
9,988 lines, 248 test cases), `src/smd/smdlisp/` (the Common Lisp pivot, 20,675
lines, 555 test cases), and `src/smd/cl/` (the live rebuild). A1–A4 removed
every consumer of the first two. This step removes the trees.

**Integration branch: `retire-three-trees`.** This is Phase A's last step; the
branch merges to `main` after it, and the owner reviews before Phase B opens.

**Reserved for this step:** blog phase **37**, divergence number **DIV-0033**,
and decision record **D32** (see part 4 for why not D22).

## Why

`CLAUDE.md` already says `src/smd/smdscheme/**` is "dead-ended and slated for
deletion from trunk". This executes that rather than deciding it.

`src/smd/smdlisp/**` is different and the difference must be faced, not
glossed. `AGENTS.md` says it "is the behavioural oracle for the rebuild and is
**never edited**, from step R1 onward. That one is the rule." The root
`checklist.md` says the same. Deleting it retires a stated rule, and a plan
that quietly deletes a tree the governance documents call permanent would be
doing the worst version of this.

Two things make the retirement honest. A2 removed the only code that used
`smdlisp` as an oracle and replaced it with SBCL, which is what decision D16
asked for in the first place — the rule protected a role that no longer exists.
And A3 froze the tree at `iteration/smdlisp-final`, so "never edited" becomes
literally true forever rather than approximately true until someone slips.
A tag is a stronger freeze than a rule.

There is also a mechanical reason the two go together and cannot be sequenced:
`smdlisp` **links** `smdscheme`. `smdlisp.smdlisp` depends on
`smdscheme.foundation` and `smdscheme.parser`, and every `smdlisp.*`
sub-library links `smdscheme.foundation`. Deleting one tree and keeping the
other does not build.

## Setup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-a5-delete-dead-trees -b step-a5-delete-dead-trees retire-three-trees
cd ../step-a5-delete-dead-trees
git submodule update --init --recursive
```

## Verify GREEN baseline

```sh
make test-matrix > /tmp/verify-A5-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A5-base.log
./scripts/verify-transclusions.sh
git tag --list 'iteration/*'
```

Both legs green. The transclusion check exits 0. **Both `iteration/*` tags must
be listed** — if they are not, A3's tags did not survive to here and you must
stop and write `blocked-A5.md`. Deleting the trees without the freeze in place
loses them, and that is not recoverable from a plan file.

## What already exists that this step builds on

- `iteration/smdscheme-final` and `iteration/smdlisp-final`, annotated tags at
  A3's merge, with `docs/history/architecture-iterations.org` pinned to them.
- No consumer left outside the two trees. Your inbound handoff carries A4's
  exported target set. Confirm the rest yourself in part 1.
- `docs/cl-rebuild-plan.md` § 2 holds decision records D11–D21, in the form
  `**D<n> — <one-line statement>.**` followed by a paragraph. Its § 5 shows the
  append-a-dated-correction convention this project uses instead of rewriting.

## The change

### 1. Confirm there is nothing left, before deleting anything

```sh
grep -rn "smdscheme\|smdlisp" \
  --include='*.cpp' --include='*.hpp' --include='CMakeLists.txt' \
  --include='*.cmake' --include='*.json' --include='*.yml' --include='*.yaml' \
  . | grep -v '^\./src/smd/smdscheme/' | grep -v '^\./src/smd/smdlisp/' \
    | grep -v '^\./\.build/' | grep -v '^\./tmp/'
```

Expected survivors, all handled below: `src/smd/CMakeLists.txt`'s two
`add_subdirectory` lines, `docs/mrdocs.yml`, `docs/antora.yml`, the provenance
comments in `src/smd/kit/foundation/*`, and comment prose in
`src/smd/cl/sender/sender_v.hpp` and `src/smd/cl/foundation/{alternative,
applicative,functor}.hpp`. Anything else is a consumer A1–A4 missed: stop and
write `blocked-A5.md` rather than improvising a fix inside a deletion step.

### 2. Delete

```sh
git rm -r src/smd/smdscheme src/smd/smdlisp
```

Remove `add_subdirectory(smdscheme)` and `add_subdirectory(smdlisp)` from
`src/smd/CMakeLists.txt`, leaving `kit`, `cl`, `fixpoint`.

### 3. The generated-documentation configuration

This is the part most likely to be forgotten, because nothing in
`make test-matrix` touches it and `.github/workflows/docs.yml` runs `make docs`
in CI where it will fail.

- `docs/mrdocs.yml` — its `input:` list is five roots, all under
  `../src/smd/smdscheme/`. Replace with the live equivalents:
  `../src/smd/kit/foundation`, `../src/smd/cl/foundation`,
  `../src/smd/cl/symbol`, `../src/smd/cl/reader`, `../src/smd/cl/printer`,
  `../src/smd/cl/core`, `../src/smd/cl/elaborator`, `../src/smd/cl/eval`.
  Check each path exists before committing.
- `docs/antora.yml` — `ext.cpp-reference.using-namespaces` is seven
  `smd::smdscheme*` entries. Replace with the `smd::cl*` and `smd::kit*`
  namespaces that match the new input roots.
- `docs/modules/ROOT/pages/architecture.adoc` — the whole page is one
  subsection per `smdscheme` module with a `Source: src/smd/smdscheme/<module>/`
  line. Nothing in it survives. Rewrite it against the live pipeline
  (`foundation`/`symbol`/`reader`/`printer`/`core`/`elaborator`/`eval`/`sender`
  under `src/smd/cl/`, plus `src/smd/kit/foundation/`), keeping the page's
  existing shape and length rather than expanding it. Do not transclude code
  here; the page uses prose plus a `Source:` line and should keep doing that.
- `docs/modules/ROOT/pages/index.adoc` — the landing-page usage example is
  `smd::smdscheme::compiled_closure<"(+ 1 2)">`. Replace it with the `cl`
  equivalent. A4's `src/examples/godbolt_cl.cpp` is the working version of
  exactly this; take the spelling from there rather than writing new code that
  has never been compiled. `docs/CODING_RULES.md`: "do not invent illustrative
  code that does not compile."

Try `make docs` and see how far it gets. It needs `mrdocs` in `.tools/`, which
may not be installed in this environment; if it is not, verify the config by
checking that every path in `mrdocs.yml`'s `input:` list exists on disk, and say
in your handoff that the CI docs job is unverified locally.

### 4. Retire the rule, in the places that state it

Four documents assert the three-tree structure. Change all four in one pass so
none is left contradicting the others.

- **`AGENTS.md`** § "Which trees you may edit" — three trees become one.
  `src/smd/cl/**` is the live tree and the only tree. Keep the section rather
  than deleting it; the rule that a fix belongs in `cl` whatever tree the bug
  was found in still means something, and the section should say where the two
  retired iterations went and that they are readable at their tags.
- **`CLAUDE.md`** — the § "Project" paragraph listing three packages, and the
  paragraph beginning "Retiring the freeze did not make `smdscheme` editable in
  practice". Also the § "Source layout" line naming `<package>` as
  "`smdscheme`, `smdlisp`, or `fixpoint`", which omits `cl` and `kit` already.
- **Root `checklist.md`** — the line asserting `src/smd/smdlisp/**` is frozen as
  a behavioural oracle and never edited. Correct it rather than deleting it;
  say what replaced the oracle role and when. Also add this phase's entries to
  whatever section tracks the rebuild's steps, matching the file's existing
  format.
- **`docs/cl-limitations.md`** and **`docs/cl-rebuild-plan.md`** — append a
  dated note, never edit in place, per this project's convention for these
  documents. `docs/cl-pivot-plan.md` is historical and is left alone.

### 5. The decision record, and why it is D32

Add to `docs/cl-rebuild-plan.md` § 2:

**D32 — The two earlier iterations are retired to tags, and trunk carries one
front end.** Say that `smdlisp`'s never-edited rule protected its role as the
rebuild's behavioural oracle; that A2 replaced that role with SBCL, which is
what D16 asked for; that a tag is a stronger freeze than a rule because it
cannot be slipped; and that `smdlisp` could not have been kept anyway once
`smdscheme` went, because it links it.

**Use D32, not D22.** D22–D26 are reserved by `docs/cl-language-scoping.md` and
D27–D31 by `docs/cl-parser-scoping.md`, both unratified proposals. Taking a
number one of them is holding would silently renumber a document you are not
editing. Say that in the record itself, in one sentence, so the gap is
explained rather than looking like a mistake.

### 6. The kit's provenance comments

Fourteen files under `src/smd/kit/foundation/` open with a comment naming the
`src/smd/smdscheme/foundation/<name>.hpp` they were extracted from. Those paths
stop existing here. Append the tag to each — `… at
iteration/smdscheme-final` — so the provenance stays checkable. It is one line
per file and mechanical; do it rather than leaving fourteen dangling references,
but if it turns out to be more than a one-line edit per file, leave them and
file it in your handoff instead of expanding this step.

### 7. Anchors

Nothing new to anchor: this step deletes code and edits prose. Say so in the
handoff. `scripts/verify-transclusions.sh` must be green, which after A3 it
will be — that is what A3 was for, and this step is its proof.

## Declared file scope

```
src/smd/smdscheme/**                        (deleted, ~9,988 lines)
src/smd/smdlisp/**                          (deleted, ~20,675 lines)
src/smd/CMakeLists.txt                      (two add_subdirectory lines)
docs/mrdocs.yml                             (input roots)
docs/antora.yml                             (using-namespaces)
docs/modules/ROOT/pages/architecture.adoc   (rewritten against cl and kit)
docs/modules/ROOT/pages/index.adoc          (usage example)
src/smd/kit/foundation/*.hpp                (14 provenance comments)
src/smd/kit/foundation/*.test.cpp           (provenance comments, same edit)
AGENTS.md                                   (three trees become one)
CLAUDE.md                                   (project and layout sections)
checklist.md                                (the frozen-oracle line; step entries)
docs/cl-limitations.md                      (dated note)
docs/cl-rebuild-plan.md                     (dated note; decision D32)
```

## Verify GREEN after

```sh
make test-matrix > /tmp/verify-A5-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A5-after.log
wc -c /tmp/verify-A5-after.log
make testinstall > /tmp/installtest-A5.log 2>&1; echo "exit=$?"
grep -E 'tests passed|Error|FAILED|error:' /tmp/installtest-A5.log | head -20
make lint
./scripts/verify-transclusions.sh
make compile-headers
```

**The test count drops sharply and that is the point of the step, not a
regression.** The baseline is 1118 ctest entries; roughly 800 of them come from
the two trees. What must hold is that both legs read `100% tests passed` and
that the surviving count matches `cl` plus `kit` plus `fixpoint` plus the one
example. Report the exact before and after numbers in your handoff — this is
the single most useful number Phase B will have, because it is what Phase B's
verify loop costs from here on.

`make testinstall`'s log runs to **megabytes** of template diagnostics. Never
`cat` or `tail` it; grep as shown. It was red on `main` for a pre-existing
reason (a header missing from `smdscheme.closure`'s installed `FILE_SET`) and
A4 should have turned it green by re-pointing the install tests at `cl` and
`kit`. It must still be green here — this step deletes the tree that caused
the original failure, so a red result now is new and is yours.

`make compile-headers` is worth running once here: the interface header sets
change shape when nine targets leave the export, and it is the cheap way to
catch a header that was only self-contained by accident.

## Spot checks

```sh
test ! -d src/smd/smdscheme && test ! -d src/smd/smdlisp && echo "gone"
ls src/smd/                       # cl, fixpoint, kit, CMakeLists.txt
git show iteration/smdlisp-final:src/smd/smdlisp/smdlisp.hpp | head -3
grep -rn "smdscheme\|smdlisp" --include='*.hpp' --include='*.cpp' \
  --include='CMakeLists.txt' --include='*.yml' src/ docs/mrdocs.yml docs/antora.yml \
  | grep -v 'iteration/'
```

The last must return only comment prose in `src/smd/cl/` that names the trees
historically, and nothing that is a path, an include, a target, or a namespace.
The `git show` proves the freeze holds after the deletion, which is the claim
this whole phase rests on.

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
Delete smdscheme and smdlisp from trunk

Thirty thousand lines and eight hundred test cases leave, and nothing
is lost: A3 froze both trees at iteration/smdscheme-final and
iteration/smdlisp-final, and the architecture prose about them now
pins to those tags rather than to the worktree.

smdscheme was already slated for this. smdlisp was not, and its rule
is worth retiring out loud rather than quietly. AGENTS.md said it is
the behavioural oracle for the rebuild and is never edited -- but that
rule protected a role, and A2 gave the role to SBCL, which is what D16
asked for from the start. A tag is a stronger freeze than a rule
because it cannot be slipped. The two could not have been separated
anyway: every smdlisp library links smdscheme.foundation.

D32 records that, taking a number past the block two unratified
proposals are holding rather than colliding with them.

The rest is the consumers nothing in the test matrix would have caught:
mrdocs' input roots, antora's namespace list, and the two Antora pages
that described the pipeline entirely in smdscheme's terms. Those run in
CI on a different job, and would have gone red a week later with no
obvious cause.
EOF

git checkout retire-three-trees
git merge --no-ff step-a5-delete-dead-trees
```

## Record measurements

After the merge, before cleanup.

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"A5","lane":null,"outcome":"green","wall_seconds":<measured>,"attempts":<n>,"verify":{"command":"make test-matrix + testinstall + compile-headers","exit_code":0,"wall_seconds":<measured>,"log_bytes":<sum>,"summary_lines_read":<n>},"diff":{"files_changed":<n>,"insertions":<n>,"deletions":<n>},"out_of_scope":[],"note":"ctest count <before> -> <after>"}
EOF
```

Put the before and after ctest counts in `note`. That number is the reason the
integration review will be able to say whether Phase B's steps were sized
against the right baseline.

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-a5-delete-dead-trees
```

Mark A5 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff — this one goes to a human, not to B1

Phase A ends here. `retire-three-trees` merges to `main` and the owner reviews
before Phase B opens; `cl-parser-combinators` does not exist yet and the B1
agent is told to halt if it is absent.

Write `tmp/plan/handoff-B1.md` anyway, addressed to B1, ≤ ~150 lines. Tell it:
the exact ctest count and the measured cold `make test-matrix` wall time after
the deletion, because every Phase B step is sized against those; that the
parser combinator layer no longer exists anywhere in the worktree and lives only
at `iteration/smdscheme-final`, so B2 will be copying from a tag rather than
from a path; the `git show` incantation that reads a file out of a tag; and
anything in `docs/mrdocs.yml` or the Antora pages that you could not verify
locally.

Also write, at the top of that handoff, a short **gate note** for the owner:
what Phase A changed, what it deleted, and the one decision — retiring
`smdlisp`'s never-edited rule — that is his to confirm rather than a worker's.
