# Step A2 — the reader-layer cl-vs-SBCL differential replaces the `smdlisp` one

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. `src/smd/cl/` is the live tree. `src/smd/smdlisp/` is the pivot-era
front end: the behavioural oracle for the rebuild, and **never edited** — that
one is a rule, not a preference (`AGENTS.md` § "Which trees you may edit").
This step removes a *consumer* of it from `cl`; it does not touch it.

**Integration branch: `retire-three-trees`.**

**Reserved for this step:** blog phase **34**, divergence number **DIV-0030**.

## Why

Two things are true about `src/smd/cl/reader/oracle_compare.test.cpp`, and
together they say it has done its job.

**It is not external authority.** Decision D16 says correctness evidence comes
from "a differential oracle running a reference implementation". `smdlisp` is
not a reference implementation; it is the previous iteration of this same
project. A1 gave the reader a real one, and this step finishes the swap.

**Its independence is already eroding.** `agree_on_keyword` compares
`syms.name(my_kw->id).substr(1)` against `their_kw->name.view()`, because the
two readers represent a keyword's name differently. That `.substr(1)` is the
comparison being adjusted to keep agreeing — a differential that is being
tuned toward agreement has stopped being evidence, whichever side is right.

And it is the last thing in `src/smd/cl/` that reaches into either dead tree.
Verified by grep: apart from prose in comments, `oracle_compare.test.cpp` and
the `smdlisp.reader` link line in `src/smd/cl/reader/CMakeLists.txt` are the
only mentions of `smdscheme` or `smdlisp` anywhere under `src/smd/cl/`.
Retiring it is what makes A5's deletion a deletion rather than a migration.

## Setup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-a2-reader-differential -b step-a2-reader-differential retire-three-trees
cd ../step-a2-reader-differential
git submodule update --init --recursive
```

## Verify GREEN baseline

Cold: about **7 minutes**, **~550 KB** of log. Never read it raw.

```sh
make test-matrix > /tmp/verify-A2-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A2-base.log
wc -c /tmp/verify-A2-base.log
```

Both legs `100% tests passed`, at A1's count. Not green ⇒ `blocked-A2.md`.

## What already exists that this step builds on

- A1's `src/smd/cl/printer/prin1.hpp`, `sbcl_read_print` in
  `src/smd/cl/conformance/sbcl_oracle.hpp`, and
  `src/smd/cl/conformance/reader_differential.test.cpp` with its first handful
  of cases. Your inbound handoff names the anchors and any oracle surprise A1
  found; `docs/compiler_architecture.org` § "The Common Lisp Rebuild:
  =smd::cl=" carries the durable facts, including the three oracle traps.
  Read that subsection by name, not the whole document.
- `src/smd/cl/reader/read.test.cpp` — 490 lines, `constexpr` cases behind
  `static_assert` plus runtime `TEST_CASE` mirrors.
- `src/smd/cl/conformance/corpus.hpp` — `satisfies(corpus_case const&)` calls
  `evaluate_program(c.source)`. It checks an **evaluated outcome** and never
  inspects a datum tree, so the corpus does not cover the reader layer at all
  and cannot be pointed at it. This is why the reader needs its own
  differential rather than a corpus flag.

## The change

### 1. Broaden `reader_differential.test.cpp` into a real corpus

Extend A1's handful into a table of source strings with a provenance-style
comment, in the same file. Aim for **forty to sixty** cases, not hundreds: this
shells out to SBCL once per case and each subprocess costs tens of
milliseconds.

Cover, at minimum, every input `oracle_compare.test.cpp` tested — they are
listed in part 2 — plus what it never could, because `smdlisp` has no such
syntax: strings with escapes, character literals, vectors, and the decimal
tower spellings. Group the table so a reader can see which cases are the
retired file's inheritance and which are new coverage.

Carry A1's exclusions forward and say why in a comment, so nobody re-adds them:
backquote, unquote and unquote-splice (SBCL renders these as
`SB-INT:QUASIQUOTE` and `#S(SB-IMPL::COMMA …)`, implementation-specific and
ANSI-permitted); non-decimal and non-canonical tower spellings (the printer
prints a spelling, SBCL prints a value); and `#\Space` (SBCL 2.2.9 prints `#\`
plus a literal space rather than the name).

**Errors are a separate table.** `oracle_compare.test.cpp`'s `ErrorsAgree`
checked that both readers reject `""`, `")"`, `"(1 2"`, `"'"`, `"("`. The SBCL
equivalent is that `read-from-string` signals, which `sbcl_read_print` must
report distinguishably from a successful parse. Either return `nullopt` from a
signalling read or return a sentinel string the way `sbcl_prin1` returns
`SBCL-ERROR`; whichever A1 chose, follow it, and if A1 did not have to choose,
choose the sentinel and say so in your handoff. Then assert that `cl` also
fails, without comparing message text — `cl`'s diagnostics are its own
(DIV-0027) and pinning them against SBCL's condition text is out of scope.

The SKIP discipline is unchanged and is not optional: guard on
`find_sbcl_version()`, `SKIP` when absent, never fail.

### 2. Delete `src/smd/cl/reader/oracle_compare.test.cpp`

**First confirm nothing is lost.** The file is 28 `CHECK`s across six
substantive `TEST_CASE`s, all fixed string literals, no generated input:

- `Fixnums`: `42`, `-7`, `0`, `"  42"`, `"; comment\n42"`
- `SymbolsFoldAlike`: `foo`, `cAr`, `t`, `nil`, `+`, `1+`, `2buffer`
- `Keywords`: `:foo`, `:Bar`
- `Lists`: `()`, `(1 2 3)`, `( 1  2 )`, `(1 ; two\n 2)`
- `QuoteFamily`: `'x`, `#'car`, `` `x ``, `,x`, `,@x`
- `ErrorsAgree`: `""`, `)`, `(1 2`, `'`, `(`

Every one of these is already covered by `src/smd/cl/reader/read.test.cpp`.
The one that looked like an exception is not: the comment-inside-a-list case is
covered at `read.test.cpp`'s `skips_comments_and_whitespace`, which reads
`(1 ; comment\n 2 #|mid|# 3)` and checks the arity. **Verify that yourself**
before deleting — grep for `skips_comments_and_whitespace` — and if you find a
case genuinely uncovered, port it into `read.test.cpp` rather than dropping it.

Then remove `oracle_compare.test.cpp` from `cl_reader_test`'s `target_sources`
in `src/smd/cl/reader/CMakeLists.txt`, and remove `smdlisp.reader` from that
target's `target_link_libraries`, leaving `cl.reader Catch2::Catch2WithMain`.

### 3. Record what `conformance/sbcl_differential.test.cpp` actually is

This is a correction, and it wants a divergence record rather than a rename.

Despite its name that file is **not** a cl-vs-SBCL differential.
`check_against_sbcl` compares SBCL's output against the corpus's own
expectation strings and never runs `cl` at all; it also skips every case whose
channel is not `expect_fixnum` or `expect_boolean`. What it validates is the
corpus — that the expectations transcribed from `pfdietz/ansi-test` and from
the specification are what a real Common Lisp actually produces. That is
genuinely worth having and it is not what the file name says.

Write `docs/divergences/DIV-0030-corpus-differential-validates-the-corpus.md`
following `docs/divergences/TEMPLATE.md`, classified `process` per
`docs/divergences/README.md`'s taxonomy, and add its row to that README's
table. Say what the file does, what its name implies, why the two came apart
(it was written before `cl` could print anything, so comparing `cl`'s output
was not available to it), and that A1 and A2 supply the differential the name
promised at the reader layer while the evaluator layer still has only the
corpus check.

Do **not** rename the file or its `TEST_CASE`s. A rename churns CMake and every
recorded ctest name for no behavioural gain; add a corrected paragraph to the
file's own header comment pointing at DIV-0030, which is the append-a-dated-note
convention this project already uses.

### 4. Anchors and the architecture doc

Land anchors around the new corpus table and around the error table. Update the
A1 subsection of `docs/compiler_architecture.org` **in place** to record that
the reader's differential authority is now SBCL and that the `smdlisp`
comparison is retired — a paragraph, at the anchor, not a new section.

## Declared file scope

```
src/smd/cl/conformance/reader_differential.test.cpp      (broadened)
src/smd/cl/conformance/sbcl_oracle.hpp                   (error reporting, if A1 left it open)
src/smd/cl/conformance/sbcl_differential.test.cpp        (header comment only)
src/smd/cl/reader/oracle_compare.test.cpp                (deleted)
src/smd/cl/reader/CMakeLists.txt                         (drop source and smdlisp.reader)
src/smd/cl/reader/read.test.cpp                          (only if a case is genuinely uncovered)
docs/divergences/DIV-0030-corpus-differential-validates-the-corpus.md   (new)
docs/divergences/README.md                               (one table row)
docs/compiler_architecture.org                           (in-place paragraph)
```

## Verify GREEN after

```sh
make test-matrix > /tmp/verify-A2-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A2-after.log
wc -c /tmp/verify-A2-after.log
make lint
./scripts/verify-transclusions.sh
```

The test count moves in two directions at once here — six `TEST_CASE`s leave
and the differential's grow — so do not treat a lower count as a failure. What
must hold is that both legs are `100% tests passed` and that the named
`OracleCompareTest` cases are gone.

## Spot checks

```sh
grep -rn "smdlisp\|smdscheme" src/smd/cl/ | grep -v "^.*://"
```

This must return **only** comment prose. No `#include`, no
`smd::smdlisp::`/`smd::smdscheme::` qualified name, no CMake target. That
condition is what A5 depends on.

```sh
test ! -f src/smd/cl/reader/oracle_compare.test.cpp && echo "retired"
grep -n "skips_comments_and_whitespace" -A6 src/smd/cl/reader/read.test.cpp
grep -c "SKIP" src/smd/cl/conformance/reader_differential.test.cpp
./.build/*/*/cl_conformance_test "*ReaderDifferential*" 2>/dev/null | tail -5
```

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
cl: retire the smdlisp reader differential for an SBCL one

D16 asks for a differential oracle running a reference implementation.
oracle_compare.test.cpp compared the rebuild's reader against smdlisp,
which is the previous iteration of this same project rather than a
reference implementation, and the comparison had begun to be tuned to
keep agreeing: agree_on_keyword took .substr(1) off one side's name
because the two trees represent a keyword differently.

A1 made a real oracle reachable. This step builds it out to cover every
input the retired file tested -- each already covered by read.test.cpp,
checked before deleting -- plus the R3 syntax smdlisp never had, and it
drops the last edge from src/smd/cl/ to either dead tree.

conformance/sbcl_differential.test.cpp keeps its name and gains a
correction. It is not a cl-vs-SBCL differential and never was: it checks
SBCL against the corpus's own expectation strings, which validates the
corpus and is worth doing under an honest description. DIV-0030 records
that, and records why the name outran the code -- the file predates the
printer, so comparing cl's output was not something it could do.
EOF

git checkout retire-three-trees
git merge --no-ff step-a2-reader-differential
```

## Record measurements

After the merge, before cleanup.

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"A2","lane":null,"outcome":"green","wall_seconds":<measured>,"attempts":<n>,"verify":{"command":"make test-matrix","exit_code":0,"wall_seconds":<measured>,"log_bytes":$(wc -c < /tmp/verify-A2-after.log),"summary_lines_read":<n>},"diff":{"files_changed":<n>,"insertions":<n>,"deletions":<n>},"out_of_scope":[],"note":""}
EOF
```

`diff` from `git diff --shortstat <A1 merge commit>..HEAD`. One line of valid
JSON. Real measured numbers only.

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-a2-reader-differential
```

Mark A2 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff

Read `tmp/plan/step-A3.md`, then write `tmp/plan/handoff-A3.md` (≤ ~150 lines).
A3 tags both dead iterations and moves their architecture prose into a pinned
history document. Tell it: the exact output of the `smdlisp|smdscheme` grep in
the spot checks, because A3 and A5 both rely on that set being empty of real
references; how `sbcl_read_print` signals a read error, if you settled it; and
anything you learned about `docs/compiler_architecture.org`'s structure while
editing it in place.
