# Step A3 — delete `smdscheme` and `smdlisp` from trunk

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. It has carried three front-end trees: `src/smd/smdscheme/` (Scheme,
9,988 lines, 248 test cases), `src/smd/smdlisp/` (the Common Lisp pivot,
20,675 lines, 555 test cases), and `src/smd/cl/` (the live rebuild). A1 froze
both dead trees as tags and moved their architecture prose to a pinned
document. A2 removed every build consumer that would otherwise break the
moment this step runs. This step removes the trees.

**Integration branch: `cl-retire-trees`.** This is Phase A's last step;
the branch merges to `main` after it, and the owner reviews before Phase B
opens.

**Reserved for this step:** blog phase **36**, divergence number **DIV-0032**.

**You do not write a decision record.** A0 already wrote **D32**, in
`docs/cl-language-scoping.md`'s dated amendment section; this step *executes*
it and appends the dated execution note. Part 5 says exactly what that means.
Note that D32 and DIV-0032 are unrelated things that happen to share a number:
`D32` is the decision, `DIV-0032` is your reserved divergence number, and
nothing requires you to spend the latter.

## Why this is Phase A's headline step, and why it runs third

The owner's direction on this plan is unambiguous and dominates every other
consideration here: agents keep getting hung up on not touching these two
trees and doing strategically wrong things, sometimes silently, while the
trees are still there to be gotten hung up on. That is why this plan deletes
first and builds replacement reader evidence (A4, A5) afterward, rather than
the other way around. A1 and A2 exist only to make this step a clean
subtraction — freezing what would otherwise be lost, and clearing every build
edge that would otherwise snap.

`CLAUDE.md` already says `src/smd/smdscheme/**` is "dead-ended and slated for
deletion from trunk". This step executes that rather than deciding it.

`src/smd/smdlisp/**` is different and the difference must be faced, not
glossed. `AGENTS.md` says it "is the behavioural oracle for the rebuild and
is **never edited**, from step R1 onward. That one is the rule." The root
`checklist.md` says the same. Deleting it retires a stated rule, and doing
that quietly, as an incidental consequence of tidying up, would be the worst
version of this. **It has already been decided, and not by you.** A0 wrote
decision **D32** — `smdlisp` is retired to a tag before the parity phase, not
as its merge criterion — which overrides the sequencing of the ratified D22 in
`docs/cl-language-scoping.md` while leaving D22's goal intact. Retiring the
never-edited rule is authorized outright and is not conditional on this step
building a replacement oracle first; A4 and A5 build that replacement, but
they follow this step rather than gating it. Read D32 in the amendment section
of `docs/cl-language-scoping.md` — that named section only, not the whole
document — before you start part 4. It is the argument you are executing and
it is not restated here.

There is also a mechanical reason the two trees cannot be separated:
`smdlisp` **links** `smdscheme`. `smdlisp.smdlisp` depends on
`smdscheme.foundation` and `smdscheme.parser`, and six of `smdlisp`'s seven
sub-library `CMakeLists.txt` files (`smdlisp`, `closure`, `elaborator`,
`macroexpand`, `reader`, `sender`) link a `smdscheme.*` target directly.
Deleting one tree and keeping the other does not build.

## Setup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-a3-delete-dead-trees -b step-a3-delete-dead-trees cl-retire-trees
cd ../step-a3-delete-dead-trees
git submodule update --init --recursive
```

## Verify GREEN baseline

```sh
make test-matrix > /tmp/verify-A3-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A3-base.log
./scripts/verify-transclusions.sh
git tag --list 'iteration/*'
```

Both legs green. The transclusion check exits 0. **Both `iteration/*` tags
must be listed** — if they are not, A1's tags did not survive to here and you
must stop and write `blocked-A3.md`. Deleting the trees without the freeze in
place loses them, and that is not recoverable from a plan file.

## What already exists that this step builds on

- `iteration/smdscheme-final` and `iteration/smdlisp-final`, annotated tags at
  A1's merge, with `docs/history/architecture-iterations.org` pinned to them.
- No consumer left outside the two trees, confirmed by A2's own sweep before
  it merged. Confirm the rest yourself in part 1 — do not trust the handoff
  alone for the step that deletes 30,000 lines.
- `docs/cl-rebuild-plan.md` § 2 holds decision records D11–D21, in the form
  `**D<n> — <one-line statement>.**` followed by a paragraph. Its § 5 and § 8
  show the append-a-dated-correction convention this project uses instead of
  rewriting. § 8's "Open questions" carries "Whether `smdlisp` is eventually
  retired or kept permanently as the pivot's artefact" — this step answers
  it; resolve that line rather than leaving it dangling.

## The change

### 1. Confirm there is nothing left, before deleting anything

```sh
grep -rl "smdscheme\|smdlisp" \
  --include='*.cpp' --include='*.hpp' --include='CMakeLists.txt' \
  --include='*.cmake' --include='*.json' --include='*.yml' --include='*.yaml' \
  . | grep -v '^\./src/smd/smdscheme/\|^src/smd/smdscheme/' \
    | grep -v '^\./src/smd/smdlisp/\|^src/smd/smdlisp/' \
    | grep -v '^\./\.build/\|^\.build/' | grep -v '^\./tmp/\|^tmp/'
```

Expected survivors, all handled below:

- `src/smd/CMakeLists.txt` — the two `add_subdirectory` lines.
- `docs/mrdocs.yml`, `docs/antora.yml`.
- Fourteen files under `src/smd/kit/foundation/*.hpp` and `*.test.cpp` —
  provenance comments naming the `smdscheme` path each was extracted from
  (`docs/backlog/BL-0004` enumerates them).
- Comment prose in **every** `src/smd/cl/foundation/*.hpp` R8 forwarding
  shim (`applicative`, `alternative`, `functor`, `static_vector`,
  `source_span`, `arena_box`, `source_pos` — verify the exact set yourself,
  it may have grown) and in `src/smd/cl/sender/sender_v.hpp`. These describe
  history — "once `cl` became a second real client of the same generic
  typeclass framework `smd::smdscheme` and `smd::forth` apply to" — and are
  not includes, qualified names, or targets. Leave them; there is nothing to
  fix and no divergence to file, per `AGENTS.md`'s own statement of this
  rule.

Anything else is a consumer A1–A2 missed: stop and write `blocked-A3.md`
rather than improvising a fix inside a deletion step.

### 2. Delete

```sh
git rm -r src/smd/smdscheme src/smd/smdlisp
```

Remove `add_subdirectory(smdscheme)` and `add_subdirectory(smdlisp)` from
`src/smd/CMakeLists.txt`, leaving `kit`, `cl`, `fixpoint`.

### 3. The generated-documentation configuration

This is the part most likely to be forgotten, because nothing in
`make test-matrix` touches it and `.github/workflows/docs.yml` runs
`make docs` in CI where it will fail.

- `docs/mrdocs.yml` — its `input:` list is five roots, all under
  `../src/smd/smdscheme/`. Replace with the live equivalents:
  `../src/smd/kit/foundation`, `../src/smd/cl/foundation`,
  `../src/smd/cl/symbol`, `../src/smd/cl/reader`, `../src/smd/cl/core`,
  `../src/smd/cl/elaborator`, `../src/smd/cl/eval`. (A4 adds
  `../src/smd/cl/printer` after this step merges; if A4 has already landed
  by the time you read this — it has not, in this plan's order — include it
  too. It has not yet, so do not invent the path now.)
- `docs/antora.yml` — `ext.cpp-reference.using-namespaces` is seven
  `smd::smdscheme*` entries. Replace with the `smd::cl*` and `smd::kit*`
  namespaces that match the new input roots.
- `docs/modules/ROOT/pages/architecture.adoc` — the whole page is one
  subsection per `smdscheme` module with a `Source: src/smd/smdscheme/<module>/`
  line. Nothing in it survives. Rewrite it against the live pipeline
  (`foundation`/`symbol`/`reader`/`core`/`elaborator`/`eval`/`sender` under
  `src/smd/cl/`, plus `src/smd/kit/foundation/`), keeping the page's existing
  shape and length rather than expanding it. Do not transclude code here; the
  page uses prose plus a `Source:` line and should keep doing that.
- `docs/modules/ROOT/pages/index.adoc` — the landing-page usage example is
  `smd::smdscheme::compiled_closure<"(+ 1 2)">`. Replace it with a `cl`
  equivalent drawn from an existing, compiling test or conformance case —
  `src/smd/cl/conformance/corpus.hpp` has several — rather than writing new
  code that has never been compiled. `docs/CODING_RULES.md`: "do not invent
  illustrative code that does not compile." (A4's `godbolt_cl.cpp` does not
  exist yet in this plan's order; do not wait for it or reference it here.)

Try `make docs` and see how far it gets. It needs `mrdocs` in `.tools/`,
which may not be installed in this environment; if it is not, verify the
config by checking that every path in `mrdocs.yml`'s `input:` list exists on
disk, and say in your handoff that the CI docs job is unverified locally.

### 4. Retire the rule, in the places that state it

**Five** documents assert the three-tree structure or the `smdlisp` freeze.
Change all five in one pass so none is left contradicting the others. A0
deliberately left every one of them alone, because the rule was still true
then; it stops being true here, all at once.

- **`AGENTS.md`** § "Which trees you may edit" — three trees become one.
  `src/smd/cl/**` is the live tree and the only tree. Keep the section rather
  than deleting it; the rule that a fix belongs in `cl` whatever tree the bug
  was found in still means something, and the section should say where the
  two retired iterations went and that they are readable at their tags.
- **`CLAUDE.md`** — the § "Project" paragraph listing three packages, and the
  paragraph beginning "Retiring the freeze did not make `smdscheme` editable
  in practice". Also the § "Source layout" line naming `<package>` as
  "`smdscheme`, `smdlisp`, or `fixpoint`", which omits `cl` and `kit`
  already.
- **Root `checklist.md`** — A0 rewrote the frozen-oracle sentence into a rule
  with a scheduled end date, naming D32 and this step. The date has arrived:
  make the sentence past tense and say what replaced the oracle role and when.
  A0 already added this plan's step entries, so you do **not** add a section —
  you tick your own A3 line like every other step.
- **`docs/backlog/README.md`** — its "## Rules" section says
  "`src/smd/smdlisp/**` is frozen from step R1 onward as the rebuild's
  behavioural oracle." That is a rule statement, not an item status, and it is
  the fifth place. A0 edited only that file's Items table and left this line
  for you deliberately. Backlog files are refined **in place**, not appended
  to — check `docs/backlog/README.md`'s own Rules section before editing it.
- **`docs/cl-limitations.md`** and **`docs/cl-rebuild-plan.md`** — append a
  dated note, never edit in place, per this project's convention for these
  documents. `docs/cl-rebuild-plan.md` § 8's "Whether `smdlisp` is eventually
  retired…" open question gets its dated resolution here, pointing at D32 in
  `docs/cl-language-scoping.md`. `docs/cl-pivot-plan.md` is historical and is
  left alone.

### 5. Execute D32, and record that you did

**Do not write a new decision record and do not renumber anything.** A0 wrote
**D32 — `smdlisp` is retired to a tag before the parity phase, not as its
merge criterion** — into `docs/cl-language-scoping.md`'s dated amendment
section, next to the ratified D22 whose sequencing it overrides. That is the
whole governance change and it is already on trunk. An earlier draft of this
plan had this step author the record as "D22", colliding with the existing
ratified D22; that draft was written when the language note was still an
unratified proposal, and it is wrong now. If you find yourself about to add a
decision to `docs/cl-rebuild-plan.md` § 2, stop — you are following the old
draft.

What you owe D32 is one thing: **append a dated execution note to it**, in the
same amendment section, saying that this step carried it out, at this commit,
and giving the two facts D32 itself could only predict —

- the two `iteration/*` tags exist and resolve (you verify this in the
  baseline and again in the spot checks), so the source programs D32 defers to
  the C-series lanes are genuinely reachable; and
- the ctest count before and after this deletion, because D32's cost argument
  was made in the abstract and this is the number it was about.

Keep it to a few lines. It is a dated note under an existing record, not a
second record.

One thing worth stating in that note because nobody else will be positioned
to: **`smdlisp` could not have been kept even if D22's sequencing had won.**
`smdlisp.smdlisp` links `smdscheme.foundation` and `smdscheme.parser`, and six
of `smdlisp`'s seven sub-library `CMakeLists.txt` files link a `smdscheme.*`
target. Keeping `smdlisp` through the C-series would have meant keeping
`smdscheme` too — both trees, not one — and `smdscheme`'s deletion was never
in question. That is a mechanical fact D22 did not have in front of it, and it
makes the override cheaper than it looked.

### 6. The kit's provenance comments

Fourteen files under `src/smd/kit/foundation/` open with a comment naming the
`src/smd/smdscheme/foundation/<name>.hpp` they were extracted from. Those
paths stop existing here. Append the tag to each — `… at
iteration/smdscheme-final` — so the provenance stays checkable. It is one
line per file and mechanical; do it rather than leaving fourteen dangling
references, but if it turns out to be more than a one-line edit per file,
leave them and file it in your handoff instead of expanding this step.

### 7. Anchors

Nothing new to anchor: this step deletes code and edits prose.
`scripts/verify-transclusions.sh` must be green, which after A1 it will be —
that is what A1 was for, and this step is its proof.

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
checklist.md                                (frozen-oracle line to past tense;
                                             tick your own A3 line)
docs/backlog/README.md                      (the Rules section's freeze line)
docs/cl-limitations.md                      (dated note)
docs/cl-rebuild-plan.md                     (dated note resolving § 8's open
                                             question, pointing at D32)
docs/cl-language-scoping.md                 (dated execution note under D32)
```

## Verify GREEN after

```sh
make test-matrix > /tmp/verify-A3-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A3-after.log
wc -c /tmp/verify-A3-after.log
make testinstall > /tmp/installtest-A3.log 2>&1; echo "exit=$?"
grep -E 'tests passed|Error|FAILED|error:' /tmp/installtest-A3.log | head -20
make lint
./scripts/verify-transclusions.sh
make compile-headers
```

**The test count drops sharply and that is the point of the step, not a
regression.** The pre-Phase-A baseline is 1123 ctest entries (measured
2026-08-18, after the Monad typeclass landed; the plan's original 1118 predates
it), and A2 already dropped roughly 17 of those (eleven example tests, six
`OracleCompareTest` cases). What is left after this step is `cl` plus `kit`
plus `fixpoint`'s own tests — expect a drop of several hundred more. What must
hold is that both legs read `100% tests passed`. Report the exact before and
after numbers in your handoff — this is the single most useful number Phase B
will have, because it is what Phase B's verify loop costs from here on.

`make testinstall` is **not a gate** — see A2's handoff for its exact
post-A2 state. It is fine, and expected, for it to still not report
meaningful coverage after this step; `docs/backlog/BL-0005-…` is where that
gets fixed, after Phase B.

`make compile-headers` is worth running once here: the interface header sets
change shape when the export list shrank in A2, and it is the cheap way to
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

The last must return only the expected comment-prose survivors named in
part 1, and nothing that is a path, an include, a target, or a namespace.
The `git show` proves the freeze holds after the deletion, which is the claim
this whole phase rests on.

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
Delete smdscheme and smdlisp from trunk

Thirty thousand lines and roughly eight hundred test cases leave, and
nothing is lost: A1 froze both trees at iteration/smdscheme-final and
iteration/smdlisp-final, and the architecture prose about them now
pins to those tags rather than to the worktree. A2 removed every
build consumer, so this is a pure subtraction.

This runs third rather than last in this plan, ahead of the printer
and the SBCL differential that will eventually replace what this
deletion removes as reader-layer evidence. The owner's priority is
that agents stop getting hung up on trees they must not touch, and
that outweighs the sequencing argument that would have built the
replacement oracle first.

smdscheme was already slated for this. smdlisp was not, and its rule
is worth retiring out loud rather than quietly -- which is why the
decision is D32, written in the step before this one, next to the
ratified D22 whose sequencing it overrides, rather than smuggled in
here as a consequence of tidying up. This step only carries it out,
and adds the two numbers D32 could not have: that both iteration tags
resolve, and what the deletion did to the test count.

One thing worth saying that D32 argued in the abstract. Keeping
smdlisp through a parity phase was never actually on the table:
smdlisp.smdlisp links smdscheme.foundation and smdscheme.parser, and
six of its seven sub-library CMakeLists files link a smdscheme target,
so keeping it meant keeping both trees. smdscheme's deletion was never
in question.

The rest is the consumers nothing in the test matrix would have
caught: mrdocs' input roots, antora's namespace list, and the two
Antora pages that described the pipeline entirely in smdscheme's
terms. Those run in CI on a different job, and would have gone red a
week later with no obvious cause.
EOF

git checkout cl-retire-trees
git merge --no-ff step-a3-delete-dead-trees
```

## Record measurements

After the merge, before cleanup.

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"A3","lane":null,"outcome":"green","wall_seconds":<measured>,"attempts":<n>,"verify":{"command":"make test-matrix + testinstall + compile-headers","exit_code":0,"wall_seconds":<measured>,"log_bytes":<sum>,"summary_lines_read":<n>},"diff":{"files_changed":<n>,"insertions":<n>,"deletions":<n>},"out_of_scope":[],"note":"ctest count <before> -> <after>"}
EOF
```

Put the before and after ctest counts in `note`. That number is the reason
the integration review will be able to say whether Phase B's steps were
sized against the right baseline.

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-a3-delete-dead-trees
```

Mark A3 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff

Read `tmp/plan/step-A4.md`, then write `tmp/plan/handoff-A4.md` (≤ ~150
lines). A4 builds the printer that starts restoring reader-layer differential
evidence. Tell it: the exact ctest count before and after this step, and the
cold `make test-matrix` wall time you measured, since A4 and A5 are sized
against those; that `src/smd/cl/` has never had a printer and nothing in it
mentions either dead tree except comment prose (name the survivor list you
confirmed in part 1, so A4 does not have to re-derive it); the exact `git
show` incantation for reading a file out of either `iteration/*` tag, since
A4's reference material — `src/smd/cl/conformance/sbcl_oracle.hpp`'s existing
`sbcl_prin1` — is unaffected by the deletion but A4 may want to compare
against how `smdscheme`'s printer, if any, worked; and anything in
`docs/mrdocs.yml` or the Antora pages you could not verify locally.
