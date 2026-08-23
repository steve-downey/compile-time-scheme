# Step A5 — broaden the SBCL differential into a real corpus

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. `src/smd/cl/` is the live tree. `smdscheme` and `smdlisp` were deleted
from trunk in A3 and survive only at `iteration/smdscheme-final` and
`iteration/smdlisp-final`. A4 gave the reader a printer and a first SBCL
oracle comparison. This is Phase A's last step.

**Integration branch: `cl-retire-trees`.**

**Reserved for this step:** blog phase **38**, divergence number **DIV-0034**.

## Why

Two things justify broadening what A4 started, and together they close the
gap this plan opened when A3 deleted the reader's only prior differential
evidence.

**A real corpus, not a handful of cases.** A4's eight-to-twelve cases proved
the printer and the oracle call work; they do not yet cover what
`reader/oracle_compare.test.cpp` used to cover before A2 deleted it, nor the
syntax that comparison never could, because `smdlisp` never had it (strings
with escapes, character literals, vectors, decimal tower spellings).

**Recorded, not assumed, is that nothing tested by the retired file has gone
untested.** That check was made once already while this plan was being
revised, and it is recorded in A2's step file and its handoff chain — this
step is where the promise A2 made (that everything `oracle_compare.test.cpp`
covered lives on in `read.test.cpp` and `number.test.cpp`) gets its
differential complement: the same inputs, checked against an outside
authority rather than only against this project's own tests.

## Setup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-a5-reader-differential -b step-a5-reader-differential cl-retire-trees
cd ../step-a5-reader-differential
git submodule update --init --recursive
```

## Verify GREEN baseline

```sh
make test-matrix > /tmp/verify-A5-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A5-base.log
wc -c /tmp/verify-A5-base.log
```

Both legs `100% tests passed`, at A4's count. Not green ⇒ `blocked-A5.md`.

## What already exists that this step builds on

- A4's `src/smd/cl/printer/prin1.hpp`, `sbcl_read_print` in
  `src/smd/cl/conformance/sbcl_oracle.hpp`, and
  `src/smd/cl/conformance/reader_differential.test.cpp` with its first
  handful of cases. Your inbound handoff names the anchors and any oracle
  surprise A4 found; `docs/compiler_architecture.org` § "The Common Lisp
  Rebuild: =smd::cl=" carries the durable facts, including the three oracle
  traps. Read that subsection by name, not the whole document.
- `src/smd/cl/reader/read.test.cpp` and `src/smd/cl/reader/number.test.cpp`
  — where A2 confirmed, before deleting `oracle_compare.test.cpp`, that
  every one of its 28 checks was already covered. That file no longer
  exists in the worktree; if you need to see its exact original content,
  `git log --all --oneline -- src/smd/cl/reader/oracle_compare.test.cpp`
  finds the commit that removed it, or read A2's own step file
  (`tmp/plan/step-A2.md` part 3), which lists the six `TEST_CASE`s verbatim.
  You should not need either — the table below already carries it forward.
- `src/smd/cl/conformance/corpus.hpp` — `satisfies(corpus_case const&)`
  calls `evaluate_program(c.source)`. It checks an **evaluated outcome** and
  never inspects a datum tree, so the corpus does not cover the reader layer
  at all and cannot be pointed at it. This is why the reader needs its own
  differential rather than a corpus flag.

## The change

### 1. Broaden `reader_differential.test.cpp` into a real corpus

Extend A4's handful into a table of source strings with a provenance-style
comment, in the same file. Aim for **forty to sixty** cases, not hundreds:
this shells out to SBCL once per case and each subprocess costs tens of
milliseconds.

Cover, at minimum, every input the retired `oracle_compare.test.cpp` tested:

- `Fixnums`: `42`, `-7`, `0`, `"  42"`, `"; comment\n42"`
- `SymbolsFoldAlike`: `foo`, `cAr`, `t`, `nil`, `+`, `1+`, `2buffer`
- `Keywords`: `:foo`, `:Bar`
- `Lists`: `()`, `(1 2 3)`, `( 1  2 )`, `(1 ; two\n 2)`
- `QuoteFamily`: `'x`, `#'car`, `` `x ``, `,x`, `,@x`

plus what it never could, because `smdlisp` has no such syntax: strings with
escapes, character literals, vectors, and the decimal tower spellings. Group
the table so a reader can see which cases are the retired file's inheritance
and which are new coverage.

Carry A4's exclusions forward and say why in a comment, so nobody re-adds
them: backquote, unquote and unquote-splice (SBCL renders these as
`SB-INT:QUASIQUOTE` and `#S(SB-IMPL::COMMA …)`, implementation-specific and
ANSI-permitted); non-decimal and non-canonical tower spellings (the printer
prints a spelling, SBCL prints a value); and `#\Space` (SBCL 2.2.9 prints
`#\` plus a literal space rather than the name).

**Errors are a separate table.** The retired file's `ErrorsAgree` checked
that both readers reject `""`, `")"`, `"(1 2"`, `"'"`, `"("`. The SBCL
equivalent is that `read-from-string` signals, which `sbcl_read_print` must
report distinguishably from a successful parse. Either return `nullopt` from
a signalling read or return a sentinel string the way `sbcl_prin1` returns
`SBCL-ERROR`; whichever A4 chose, follow it, and if A4 did not have to
choose, choose the sentinel and say so in your handoff. Then assert that
`cl` also fails, without comparing message text — `cl`'s diagnostics are its
own (DIV-0027) and pinning them against SBCL's condition text is out of
scope.

The SKIP discipline is unchanged and is not optional: guard on
`find_sbcl_version()`, `SKIP` when absent, never fail.

### 2. Record what `conformance/sbcl_differential.test.cpp` actually is

This is a correction, and it wants a divergence record rather than a rename.

Despite its name that file is **not** a cl-vs-SBCL differential.
`check_against_sbcl` compares SBCL's output against the corpus's own
expectation strings and never runs `cl` at all; it also skips every case
whose channel is not `expect_fixnum` or `expect_boolean`. What it validates
is the corpus — that the expectations transcribed from `pfdietz/ansi-test`
and from the specification are what a real Common Lisp actually produces.
That is genuinely worth having and it is not what the file name says.

Write `docs/divergences/DIV-0034-corpus-differential-validates-the-corpus.md`
following `docs/divergences/TEMPLATE.md`, classified `process` per
`docs/divergences/README.md`'s taxonomy, and add its row to that README's
table. Say what the file does, what its name implies, why the two came apart
(it was written before `cl` could print anything, so comparing `cl`'s output
was not available to it), and that A4 and this step supply the differential
the name promised at the reader layer while the evaluator layer still has
only the corpus check.

Do **not** rename the file or its `TEST_CASE`s. A rename churns CMake and
every recorded ctest name for no behavioural gain; add a corrected paragraph
to the file's own header comment pointing at DIV-0034, which is the
append-a-dated-note convention this project already uses.

### 3. Anchors and the architecture doc

Land anchors around the new corpus table and around the error table. Update
the A4 subsection of `docs/compiler_architecture.org` **in place** to record
that the reader's differential authority is now SBCL, that the `smdlisp`
comparison is gone along with the tree, and that this closes the gap A3's
deletion opened.

## Declared file scope

```
src/smd/cl/conformance/reader_differential.test.cpp      (broadened)
src/smd/cl/conformance/sbcl_oracle.hpp                   (error reporting, if A4 left it open)
src/smd/cl/conformance/sbcl_differential.test.cpp        (header comment only)
docs/divergences/DIV-0034-corpus-differential-validates-the-corpus.md   (new)
docs/divergences/README.md                               (one table row)
docs/compiler_architecture.org                           (in-place paragraph)
```

`src/smd/cl/reader/oracle_compare.test.cpp` and
`src/smd/cl/reader/CMakeLists.txt` are **not** in this step's scope — A2
already deleted the former and edited the latter. If either still exists or
still mentions `smdlisp`, that is A2's or A3's defect, not yours to silently
fix: write `blocked-A5.md`.

## Verify GREEN after

```sh
make test-matrix > /tmp/verify-A5-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A5-after.log
wc -c /tmp/verify-A5-after.log
make lint
./scripts/verify-transclusions.sh
```

The test count should only grow here — this step adds cases and deletes
none. Both legs must be `100% tests passed`.

## Spot checks

```sh
grep -c "SKIP" src/smd/cl/conformance/reader_differential.test.cpp
./.build/*/*/cl_conformance_test "*ReaderDifferential*" 2>/dev/null | tail -5
test ! -f src/smd/cl/reader/oracle_compare.test.cpp && echo "confirmed gone (A2's work, not yours)"
```

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
cl: broaden the SBCL reader differential into a real corpus

A2 deleted reader/oracle_compare.test.cpp and A3 deleted smdlisp
along with it; between then and A4 the reader had no differential
oracle at all. A4 proved a printer and a first SBCL comparison work.
This step is where the corpus catches up to what was lost and then
goes past it: every input the retired file tested, checked here
against SBCL instead of against smdlisp, plus the syntax smdlisp
never had at all -- strings with escapes, characters, vectors, decimal
towers.

conformance/sbcl_differential.test.cpp keeps its name and gains a
correction. It is not a cl-vs-SBCL differential and never was: it
checks SBCL against the corpus's own expectation strings, which
validates the corpus and is worth doing under an honest description.
DIV-0034 records that, and records why the name outran the code -- the
file predates the printer, so comparing cl's output was not something
it could do.
EOF

git checkout cl-retire-trees
git merge --no-ff step-a5-reader-differential
```

## Record measurements

After the merge, before cleanup.

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"A5","lane":null,"outcome":"green","wall_seconds":<measured>,"attempts":<n>,"verify":{"command":"make test-matrix","exit_code":0,"wall_seconds":<measured>,"log_bytes":$(wc -c < /tmp/verify-A5-after.log),"summary_lines_read":<n>},"diff":{"files_changed":<n>,"insertions":<n>,"deletions":<n>},"out_of_scope":[],"note":""}
EOF
```

`diff` from `git diff --shortstat <A4 merge commit>..HEAD`. One line of
valid JSON. Real measured numbers only.

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-a5-reader-differential
```

Mark A5 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff — this one goes to a human, not only to B1

Phase A ends here. `cl-retire-trees` merges to `main` and the owner
reviews before Phase B opens; `cl-parser-combinators` does not exist yet and
the B1 agent is told to halt if it is absent.

Write `tmp/plan/handoff-B1.md` anyway, addressed to B1, ≤ ~150 lines. Tell
it: the exact ctest count and the measured cold `make test-matrix` wall time
after the whole of Phase A, because every Phase B step is sized against
those; that the parser combinator layer no longer exists anywhere in the
worktree and lives only at `iteration/smdscheme-final`, so B2 will be
copying from a tag rather than from a path; the `git show` incantation that
reads a file out of a tag; and anything in `docs/mrdocs.yml` or the Antora
pages that A3 could not verify locally.

Also write, at the top of that handoff, a short **gate note** for the
owner: what Phase A changed, in the order it actually ran (freeze, neutralise,
delete, printer, differential — not the build-the-oracle-first order this
plan started with), what it deleted, and the one thing that is his to confirm
rather than a worker's — **decision D32**, written at A0 and executed at A3,
which retires `smdlisp` to a tag *before* the parity phase rather than as its
merge criterion, overriding the sequencing of the ratified D22 it sits beside
in `docs/cl-language-scoping.md`.

Say, in two or three lines and without arguing the case again, what D32
deferred rather than discarded: the roughly 120 evaluated behaviour cases
`smdlisp` holds for `LET`, `SETQ`, `CATCH`, `TAGBODY`, `DEFMACRO` and multiple
values, none of which the 75-entry corpus reaches, now readable out of
`iteration/smdlisp-final` by every C-series lane that wants source programs.
Whether that deferral was the right call is the judgement the gate exists for,
and the owner should be able to see it in the handoff rather than having to
find D32 to know it was made.
