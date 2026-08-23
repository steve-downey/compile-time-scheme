# Step A2 — neutralise the dead trees' build consumers

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. `src/smd/cl/` is the live tree; `src/smd/kit/` is the shared constexpr
substrate. `src/smd/smdscheme/` and `src/smd/smdlisp/` are the two finished
iterations, frozen as tags by A1 and deleted by A3. This step is the one that
makes A3 a pure subtraction rather than a step that also has to fix a broken
build.

**Integration branch: `cl-retire-trees`.**

**Reserved for this step:** blog phase **35**, divergence number **DIV-0031**.

## Why this step, and why it does less than it sounds like

Every consumer this step touches builds against a tree A3 is about to remove,
so if this step does not run first, A3's own build breaks the moment
`git rm -r` runs — `src/examples/` links `smdscheme.smdscheme` and
`smdlisp.smdlisp` by name, and the top-level `beman_install_library` call
names eight `smdscheme.*` targets that would simply not exist as CMake
targets. **This is not optional cleanup; without it `make test-matrix`
cannot configure once A3 deletes the trees.**

What this step deliberately does **not** do is replace what it removes with a
`cl`-equivalent. A new example that demonstrates `smd::cl`, an export list
that installs `kit.foundation` and the `cl.*` targets, and install tests that
prove the installed headers are reachable are all real, positive work — and
all three are `docs/backlog/BL-0005-export-live-trees-and-turn-testinstall-green.md`,
scheduled after Phase B rather than inside this deletion. The reason is
`make testinstall`: it is **already red on `main`**, for a pre-existing
reason unrelated to anything in this plan —

```
.install/include/smd/smdscheme/closure/cps_code.hpp:6:10:
    fatal error: smd/smdscheme/closure/pairs.hpp: No such file or directory
```

— and `make testinstall` is not part of `make test-matrix`'s acceptance gate
(they are separate CMake projects; see `docs/verification-matrix.md`). Fixing
it is not a precondition for anything else in this plan, so bundling it into
the largest, riskiest step in the plan would only add risk for no gate this
plan actually needs to pass. **`make testinstall` stays red for the whole of
this plan and is not an acceptance signal** — say this again in your handoff,
because it is the fact most likely to be misread as a regression this step
introduced.

## Setup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-a2-neutralise-consumers -b step-a2-neutralise-consumers cl-retire-trees
cd ../step-a2-neutralise-consumers
git submodule update --init --recursive
```

## Verify GREEN baseline

```sh
make test-matrix > /tmp/verify-A2-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A2-base.log
make testinstall > /tmp/installtest-A2-base.log 2>&1; echo "exit=$?"
grep -nE 'fatal error|CMake Error|^FAILED|tests passed' /tmp/installtest-A2-base.log | head -20
```

Both `make test-matrix` legs green. `make testinstall` reproduces the
pre-existing exit-2 failure above — that is the expected baseline, not a
defect for you to fix. Never `cat` or `tail` its log; it runs to megabytes of
template diagnostics. If `make test-matrix` is not green, stop and write
`blocked-A2.md`.

## What already exists that this step builds on

- `src/examples/CMakeLists.txt` — 11 `add_executable` blocks. All eleven
  reference `smdscheme.smdscheme`, `smdscheme.sender`, `smdlisp.smdlisp`, or
  `smdlisp.sender`. `src/CMakeLists.txt` has an unconditional
  `add_subdirectory(examples)`; that call stays.
- `installtest/CMakeLists.txt` — a `find_package(schemepoc.schemepoc
  REQUIRED …)`, a helper `add_installtest(name src)` that links
  `smd::smdscheme`, and six calls to it: `test_smdscheme`, `test_foundation`,
  `test_reader`, `test_elaborator`, `test_closure`, `test_sender`, each
  backed by a `.cpp` file under `installtest/` that includes only
  `smdscheme` headers.
- Top-level `CMakeLists.txt` — one `beman_install_library(schemepoc.schemepoc
  TARGETS … NAMESPACE smd::)` block naming eight `smdscheme.*` targets plus
  `smd.fixpoint`.
- `src/smd/cl/reader/oracle_compare.test.cpp` — 28 `CHECK`s across six
  substantive `TEST_CASE`s, all fixed string literals, no generated input,
  and the **only** thing under `src/smd/cl/` that includes either dead tree:
  `#include <smd/smdlisp/reader/read_datum.hpp>` and
  `#include <smd/smdscheme/parser/cursor.hpp>` (verified by direct grep — it
  reaches into *both* trees, not only `smdlisp` as an earlier pass at this
  plan assumed). `src/smd/cl/reader/CMakeLists.txt` links `cl_reader_test`
  against `smdlisp.reader` alongside `cl.reader Catch2::Catch2WithMain`.
- `docs/compiler_architecture.org` § "The Common Lisp Rebuild: =smd::cl=",
  where A1 records the frozen-tree tags. Read that section, not the whole
  document.

## The change

### 1. Delete the eleven examples, without a replacement

```sh
git rm src/examples/*.cpp
```

Rewrite `src/examples/CMakeLists.txt` down to a stub: keep
`include(GNUInstallDirs)` and add a comment that the eleven `smdscheme`/
`smdlisp` examples are gone and a `cl`-equivalent is
`docs/backlog/BL-0005-export-live-trees-and-turn-testinstall-green.md`, not
this step. Remove every `add_executable`, `install(TARGETS …)`,
`add_test(NAME examples_… …)` and `set_tests_properties` block. The file
ends up close to empty; that is correct, not a sign something was missed.

### 2. Neutralise the install test

```sh
git rm installtest/test.cpp installtest/test_foundation.cpp \
       installtest/test_reader.cpp installtest/test_elaborator.cpp \
       installtest/test_closure.cpp installtest/test_sender.cpp
```

In `installtest/CMakeLists.txt`, remove the six `add_installtest(...)` call
lines **and** the `add_installtest` helper function definition — its body
hardcodes `target_link_libraries(${name} PRIVATE smd::smdscheme)`, and
leaving that string sitting unused is exactly the kind of stale reference the
sweep in A3 has to reason about. BL-0005 will write its own helper against
`kit::foundation`/`cl.*` targets rather than reuse this one, so nothing is
lost by removing it now. Keep the `find_package` call — the project still
needs to configure against the installed package — and add a one-line
comment pointing at BL-0005 where the helper and its six calls used to be.

Do not try to make `make testinstall` pass here. After this step it should at
least **configure** without error (`find_package` succeeds against a package
that no longer claims a `smd::smdscheme` target, and nothing tries to link
one), which is a different and lesser thing than green — zero installed
tests is expected, not a regression to chase.

### 3. Confirm nothing is lost, then delete `oracle_compare.test.cpp`

This has already been checked once, while this plan was being revised, and
the check is worth repeating yourself rather than trusting a plan file. The
file's six `TEST_CASE`s are:

- `Fixnums`: `42`, `-7`, `0`, `"  42"`, `"; comment\n42"`
- `SymbolsFoldAlike`: `foo`, `cAr`, `t`, `nil`, `+`, `1+`, `2buffer`
- `Keywords`: `:foo`, `:Bar`
- `Lists`: `()`, `(1 2 3)`, `( 1  2 )`, `(1 ; two\n 2)`
- `QuoteFamily`: `'x`, `#'car`, `` `x ``, `,x`, `,@x`
- `ErrorsAgree`: `""`, `)`, `(1 2`, `'`, `(`

Every one of these is already covered by `src/smd/cl/reader/read.test.cpp` or
`src/smd/cl/reader/number.test.cpp`. The bare fixnum literal `0` is pinned in
`number.test.cpp` (`fixnum_of("0", 10, 0)`), not in `read.test.cpp` — confirm
with `grep -n '"0"' src/smd/cl/reader/number.test.cpp` before relying on it.
The comment-inside-a-list case is covered by `read.test.cpp`'s
`skips_comments_and_whitespace`, which reads
`(1 ; comment\n 2 #|mid|# 3)` and checks the arity is 3 — confirm with
`grep -n skips_comments_and_whitespace -A6 src/smd/cl/reader/read.test.cpp`.
If you find a case genuinely uncovered that these two greps do not turn up,
port it into `read.test.cpp` rather than dropping it, and say so in your
handoff.

Then:

```sh
git rm src/smd/cl/reader/oracle_compare.test.cpp
```

Remove `oracle_compare.test.cpp` from `cl_reader_test`'s `target_sources` in
`src/smd/cl/reader/CMakeLists.txt`, and remove `smdlisp.reader` from that
target's `target_link_libraries`, leaving `cl.reader Catch2::Catch2WithMain`.

This is the step that makes A3's later claim true: after this change,
`grep -rn "smdlisp\|smdscheme" src/smd/cl/` should return **only** comment
prose — no `#include`, no qualified name, no CMake target. Run that grep
yourself before merging and put the result in your handoff; A3 depends on it
being empty of anything but prose.

### 4. Shrink the exported package to what still exists

In the top-level `CMakeLists.txt`, rewrite the `beman_install_library` call:

```cmake
beman_install_library(
    schemepoc.schemepoc
    TARGETS
        smd.fixpoint
    NAMESPACE smd::
)
```

This is **not** "export the live trees" — that is BL-0005, deliberately
postponed. This is the minimum edit that keeps `make install` (and therefore
`make testinstall`'s configure step) from naming a target that no longer
exists once A3 runs. `kit.foundation` and the `cl.*` targets stay unexported
until BL-0005 schedules the install-test coverage that justifies exporting
them.

### 5. Anchors

Nothing new to anchor: this step deletes build wiring and test code, it does
not add a component. Say so in your handoff.

## Declared file scope

```
src/examples/*.cpp                          (all eleven, deleted)
src/examples/CMakeLists.txt                 (reduced to a stub + a comment)
installtest/test.cpp                        (deleted)
installtest/test_foundation.cpp             (deleted)
installtest/test_reader.cpp                 (deleted)
installtest/test_elaborator.cpp             (deleted)
installtest/test_closure.cpp                (deleted)
installtest/test_sender.cpp                 (deleted)
installtest/CMakeLists.txt                  (six calls AND the helper removed;
                                             find_package kept — part 2 is
                                             authoritative, an earlier draft of
                                             this line said "helper kept" and
                                             contradicted it)
src/smd/cl/reader/oracle_compare.test.cpp   (deleted)
src/smd/cl/reader/CMakeLists.txt            (drop source and smdlisp.reader)
src/smd/cl/reader/read.test.cpp             (only if a case is genuinely uncovered)
CMakeLists.txt                              (beman_install_library TARGETS list)
```

The two dead trees themselves are **not** in scope. They still build and
still run their tests after this step; A3 deletes them. If you find yourself
needing to edit anything under `src/smd/smdscheme/` or `src/smd/smdlisp/`,
stop — `src/smd/smdlisp/**` is never edited, and needing to touch either tree
is a signal something is out of order. Write `blocked-A2.md`.

## Verify GREEN after

```sh
make test-matrix > /tmp/verify-A2-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A2-after.log
wc -c /tmp/verify-A2-after.log
make testinstall > /tmp/installtest-A2-after.log 2>&1; echo "exit=$?"
grep -nE 'fatal error|CMake Error|^FAILED|tests passed' /tmp/installtest-A2-after.log | head -20
make lint
./scripts/verify-transclusions.sh
```

The ctest count moves in two directions at once: eleven example tests leave
and six `OracleCompareTest` cases leave, but nothing replaces them yet — the
count drops and that is expected, not a regression. Both matrix legs must
still read `100% tests passed`.

`make testinstall`'s exact exit code may change from the baseline's 2 to
something else now that `find_package` no longer sees a claimed
`smd::smdscheme` target — **this is not a gate**. Do not spend the step
chasing it to green; that is BL-0005's job, scheduled after Phase B.

## Spot checks

```sh
grep -rn "smdlisp\|smdscheme" src/examples/ installtest/ CMakeLists.txt
```

Must return nothing.

```sh
grep -rn "smdlisp\|smdscheme" src/smd/cl/ | grep -v "^.*://"
```

Must return **only** comment prose — no `#include`, no qualified name, no
CMake target. This is what A3 depends on.

```sh
ls src/examples/*.cpp 2>/dev/null; echo "expect: no such file"
ls installtest/*.cpp 2>/dev/null; echo "expect: no such file"
test ! -f src/smd/cl/reader/oracle_compare.test.cpp && echo "retired"
grep -n "TARGETS" -A3 CMakeLists.txt
```

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
build: neutralise every consumer of the two dead trees

A3 deletes smdscheme and smdlisp. Without this step first, that
deletion breaks the build outright: eleven examples link smdscheme.*
or smdlisp.* by name, the top-level export list names eight
smdscheme.* targets that would not exist, and
reader/oracle_compare.test.cpp is the one file under src/smd/cl/ that
includes either dead tree at all -- both of them, not only smdlisp as
an earlier pass at this plan assumed.

This step does not replace what it removes. A cl-equivalent example,
an export list that installs kit and cl, and install tests that prove
the installed headers are reachable are real work, and all three are
docs/backlog/BL-0005, scheduled after the parser-combinator phase
rather than inside a tree deletion. The reason is that make testinstall
is already red on main for an unrelated pre-existing reason and is not
part of make test-matrix's acceptance gate, so there is no gate this
plan needs that bundling the positive work here would satisfy any
sooner -- only more risk in the plan's largest step.

oracle_compare.test.cpp's coverage was checked against
read.test.cpp and number.test.cpp before deleting it, not assumed:
the bare fixnum literal 0 and the comment-inside-a-list case are both
pinned elsewhere. Deleting it is what makes the next grep for
smdlisp/smdscheme under src/smd/cl/ come back prose-only, which is
what A3 depends on.
EOF

git checkout cl-retire-trees
git merge --no-ff step-a2-neutralise-consumers
```

## Record measurements

After the merge, before cleanup.

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"A2","lane":null,"outcome":"green","wall_seconds":<measured>,"attempts":<n>,"verify":{"command":"make test-matrix + make testinstall","exit_code":0,"wall_seconds":<measured>,"log_bytes":<sum of both logs>,"summary_lines_read":<n>},"diff":{"files_changed":<n>,"insertions":<n>,"deletions":<n>},"out_of_scope":[],"note":"testinstall is not a gate; recorded for the trend, not the verdict"}
EOF
```

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-a2-neutralise-consumers
```

Mark A2 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff

Read `tmp/plan/step-A3.md`, then write `tmp/plan/handoff-A3.md` (≤ ~150
lines). A3 deletes both trees. Tell it: the exact result of the
`smdlisp|smdscheme` grep over `src/smd/cl/` — it must be prose-only, and A3's
own deletion depends on that; whether you found a genuinely-uncovered
`oracle_compare.test.cpp` case and ported it; that `make testinstall` is not
a gate and its exact post-A2 exit code/behaviour, so A3 does not mistake a
change there for something it caused; and the new (shrunk) contents of the
top-level `beman_install_library` TARGETS list.
