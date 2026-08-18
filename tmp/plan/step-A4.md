# Step A4 — re-point the examples, the install test, and the exported package

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. `src/smd/cl/` is the live tree; `src/smd/kit/` is the shared constexpr
substrate extracted in step R8. `src/smd/smdscheme/` and `src/smd/smdlisp/`
are the two finished iterations, frozen as tags by A3 and deleted by A5.

**Integration branch: `retire-three-trees`.**

**Reserved for this step:** blog phase **36**, divergence number **DIV-0032**.

## Why

Everything the project shows the outside world still points at the dead trees.

All **eleven** programs under `src/examples/` use one or the other, so the
demonstration surface of a Common Lisp compiler that runs at compile time
demonstrates the *previous* Common Lisp compiler. All **six** files under
`installtest/` link `smd::smdscheme`, so the check that the installed package
is usable checks a package built from code that is about to stop existing. And
the exported package itself is eight `smdscheme.*` targets plus `smd.fixpoint`;
no `cl.*` or `kit.*` target is installed at all, which means the live tree has
never been install-tested.

That last one is the real finding. The deletion is not just removing dead
weight — it exposes that the tree the project actually works in has no
installed-package coverage. This step is where that gets fixed, and it is why
this step has a positive deliverable rather than being a deletion.

## Setup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-a4-repoint-consumers -b step-a4-repoint-consumers retire-three-trees
cd ../step-a4-repoint-consumers
git submodule update --init --recursive
```

## Verify GREEN baseline

Two commands here, not one. `make testinstall` is **not** part of
`make test-matrix` and this step is the reason that matters.

```sh
make test-matrix > /tmp/verify-A4-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A4-base.log
make testinstall > /tmp/installtest-A4-base.log 2>&1; echo "exit=$?"
grep -nE 'fatal error|CMake Error|^FAILED|tests passed' /tmp/installtest-A4-base.log | head -20
```

`make testinstall` does a `RelWithDebInfo` build, an install, and a separate
CMake configure of `installtest/` against the installed package. It takes about
110 seconds and its log is roughly **7 MB** of template diagnostics. Never
`cat` or `tail` it — grep as shown.

**`make testinstall` is already red, and this was measured on `main` before the
plan was written.** It exits 2 with:

```
.install/include/smd/smdscheme/closure/cps_code.hpp:6:10:
    fatal error: smd/smdscheme/closure/pairs.hpp: No such file or directory
```

`pairs.hpp` was never added to `smdscheme.closure`'s installed `FILE_SET`, so
the installed package has been unusable for as long as nobody ran the check.
Three of the six install tests fail on it.

**Do not fix that.** It is a defect in a tree being deleted, and this step
deletes its last consumer. What it means for you is that your baseline is red
in a known way, that your after-state should be **green** because the install
tests will no longer touch `smdscheme` at all, and that turning it green is a
real deliverable of this step rather than an accident. If your after-state is
red for a *different* reason, that is yours and you must fix it or halt.

If `make test-matrix` is not green, stop and write `blocked-A4.md`.

## What already exists that this step builds on

- `src/examples/CMakeLists.txt` — 11 `add_executable` blocks, each with an
  `install(TARGETS …)` under component `smdscheme.smdscheme.examples` or
  `smdlisp.smdlisp.examples`, an `add_test(NAME examples_<name> …)`, and
  `set_tests_properties`. Every one of the 11 references a dead tree. Read the
  file; copy the shape of one block rather than inventing a new one.
- `installtest/CMakeLists.txt` — `find_package(schemepoc.schemepoc REQUIRED …)`
  then a helper `add_installtest(name src)` that links `smd::smdscheme` and
  registers a ctest. Six calls.
- Top-level `CMakeLists.txt` — the `beman_install_library(schemepoc.schemepoc
  TARGETS … NAMESPACE smd::)` block listing eight `smdscheme.*` targets and
  `smd.fixpoint`.
- `Makefile` target `testinstall`, which drives the whole install check.
- `src/smd/cl/` targets, from `src/smd/cl/*/CMakeLists.txt`: `cl.foundation`,
  `cl.symbol`, `cl.reader`, `cl.core`, `cl.elaborator`, `cl.eval`,
  `cl.conformance`, `cl.sender`, plus A1's `cl.printer`. `src/smd/kit/` provides
  `kit.foundation`. All are `INTERFACE` libraries.
- Your inbound handoff confirms the two `iteration/*` tags resolve and that
  `godbolt_lisp.cpp` and `godbolt_lisp_ffi.cpp` are transcluded only from the
  pinned history document, so deleting them breaks nothing.

## The change

### 1. One new example, then delete the eleven

Write `src/examples/godbolt_cl.cpp` first. It demonstrates `smd::cl` doing at
compile time what `godbolt_arithmetic.cpp` and `godbolt_lisp.cpp` demonstrated
for the earlier trees: a source string, read and elaborated and evaluated in a
`constexpr` context, with the result pinned by a `static_assert` so the program
is its own proof. Keep it short — this is a demonstration artifact, and
`docs/CODING_RULES.md` asks for "short executable examples for transclusion".

Look at `src/smd/cl/conformance/driver.hpp`'s `evaluate_program` for the
entry point that reads, elaborates and runs a source string in one call; that is
almost certainly what the example should use rather than assembling the pipeline
by hand.

Then delete all eleven existing `.cpp` files and rewrite
`src/examples/CMakeLists.txt` down to the single `godbolt_cl` block, keeping the
existing block's shape: `add_executable`, `target_link_libraries`, an
`install(TARGETS …)` with a component name matching the new package
(`schemepoc.schemepoc.examples` unless you find a better fit in
`infra/cmake/beman-install-library.cmake`), `add_test(NAME examples_godbolt_cl …)`,
and whatever `set_tests_properties` the neighbours carried.

### 2. Export the live trees

Rewrite the top-level `beman_install_library` target list. The eight
`smdscheme.*` targets go. What replaces them is `kit.foundation` and the `cl.*`
targets, plus `smd.fixpoint` which is unaffected:

```cmake
beman_install_library(
    schemepoc.schemepoc
    TARGETS
        kit.foundation
        cl.foundation
        cl.symbol
        cl.reader
        cl.printer
        cl.core
        cl.elaborator
        cl.eval
        cl.conformance
        cl.sender
        smd.fixpoint
    NAMESPACE smd::
)
```

Check that list against the actual `add_library` names in
`src/smd/cl/*/CMakeLists.txt` and `src/smd/kit/foundation/CMakeLists.txt` rather
than trusting it; A1 added one and the set may have moved. Every listed target
must be installable — `beman_install_library` needs each to have a
`FILE_SET HEADERS`, which the `cl.*` targets do.

`cl.conformance` is a judgement call: it pulls in `sbcl_oracle.hpp`, which
shells out to a subprocess. If installing it is awkward, leave it out and say
why in your handoff — an installed package that cannot run the conformance
harness is a defensible scope, an installed package that fails to configure is
not.

### 3. Re-point the install test

Delete `installtest/test.cpp`, `test_foundation.cpp`, `test_reader.cpp`,
`test_elaborator.cpp`, `test_closure.cpp`, `test_sender.cpp` — all six exercise
`smdscheme` only.

Write replacements against the live trees. Three is enough and mirrors what the
old set was doing at a smaller surface:

- `installtest/test_kit_foundation.cpp` — includes several
  `<smd/kit/foundation/*.hpp>` headers and exercises `static_vector`, `result`
  and `source_pos` in a `constexpr` context.
- `installtest/test_cl_reader.cpp` — includes `<smd/cl/reader/read.hpp>` and
  `<smd/cl/printer/prin1.hpp>`, reads a datum and renders it.
- `installtest/test_cl_eval.cpp` — includes `<smd/cl/conformance/driver.hpp>`
  or the evaluator headers directly and evaluates one form.

These are install tests, not unit tests: what they prove is that the headers are
reachable, self-contained and compile against the *installed* include tree. Keep
them small and do not use Catch2 — the existing six do not, and the installed
package does not carry it.

Update `installtest/CMakeLists.txt`'s `add_installtest` helper to link the right
target instead of `smd::smdscheme`. Note the existing comment explaining that
everything links `smd::smdscheme` "so they transitively pick up the
`-freflection` flag and the full sub-library include tree" — the `cl.*` targets
may not carry `-freflection` at all, which would be fine (reflection lives in
`smdscheme/reflection/` and is going away) but check rather than assume, and if
a compile flag was arriving transitively and now is not, that is a real finding
for your handoff.

### 4. Anchors

Land anchors around `godbolt_cl.cpp`'s demonstrated form and its
`static_assert`, so the post can transclude the thing the step is actually
about. Record in `docs/compiler_architecture.org` § "The Common Lisp Rebuild:
=smd::cl=", in place and at an anchor, the fact this step exposed: the live tree
had no installed-package coverage until now, and the exported target set is
`kit.foundation` plus the `cl.*` libraries.

## Declared file scope

```
src/examples/godbolt_cl.cpp                (new)
src/examples/advanced_reflection_ffi.cpp   (deleted)
src/examples/advanced_sender_ffi.cpp       (deleted)
src/examples/fibonacci_graph.cpp           (deleted)
src/examples/godbolt_arithmetic.cpp        (deleted)
src/examples/godbolt_ffi.cpp               (deleted)
src/examples/godbolt_lambda.cpp            (deleted)
src/examples/godbolt_lisp.cpp              (deleted)
src/examples/godbolt_lisp_ffi.cpp          (deleted)
src/examples/hello.cpp                     (deleted)
src/examples/lisp_sender_graph_demo.cpp    (deleted)
src/examples/sender_graph_demo.cpp         (deleted)
src/examples/CMakeLists.txt                (rewritten to one target)
installtest/test.cpp                       (deleted)
installtest/test_foundation.cpp            (deleted)
installtest/test_reader.cpp                (deleted)
installtest/test_elaborator.cpp            (deleted)
installtest/test_closure.cpp               (deleted)
installtest/test_sender.cpp                (deleted)
installtest/test_kit_foundation.cpp        (new)
installtest/test_cl_reader.cpp             (new)
installtest/test_cl_eval.cpp               (new)
installtest/CMakeLists.txt                 (helper + six calls become three)
CMakeLists.txt                             (beman_install_library target list)
docs/compiler_architecture.org             (in-place, at an anchor)
```

The two dead trees themselves are **not** in scope. They still build and still
run their tests after this step; A5 deletes them. If you find yourself needing
to edit anything under `src/smd/smdscheme/` or `src/smd/smdlisp/`, stop —
`src/smd/smdlisp/**` is never edited, and needing to is a signal the step order
is wrong. Write `blocked-A4.md`.

## Verify GREEN after

```sh
make test-matrix > /tmp/verify-A4-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A4-after.log
wc -c /tmp/verify-A4-after.log
make testinstall > /tmp/installtest-A4-after.log 2>&1; echo "exit=$?"
grep -nE 'fatal error|CMake Error|^FAILED|tests passed' /tmp/installtest-A4-after.log | head -20
make lint
./scripts/verify-transclusions.sh
```

The ctest count drops by ten — eleven example tests out, one in — and by six
installtest entries, which are in a separate CMake project and were never in the
matrix count anyway. Both matrix legs must read `100% tests passed`.

**`make testinstall` should now exit 0**, where the baseline exited 2. The
`pairs.hpp` failure goes away because the install tests no longer include
anything from `smdscheme`. If it is still red, read the grep output: a *new*
failure is yours to fix or to halt on, and the likeliest cause is a `cl.*` or
`kit.*` target you listed in the export set whose `FILE_SET HEADERS` is missing
a header — the same class of bug as the one you inherited, which is worth
noticing.

## Spot checks

```sh
grep -rn "smdscheme\|smdlisp" src/examples/ installtest/ CMakeLists.txt
```

Must return nothing.

```sh
ls src/examples/*.cpp                    # exactly one
ls installtest/*.cpp                     # exactly three
grep -n "TARGETS" -A14 CMakeLists.txt    # the new export set
./.build/*/*/godbolt_cl 2>/dev/null || echo "check the built path"
```

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
build: point the examples, the install test and the package at cl

Everything this project showed the outside world still pointed at the
two trees that are leaving. All eleven examples used smdscheme or
smdlisp, so the demonstration of a Common Lisp compiler that runs at
compile time demonstrated the previous one. All six installtest files
linked smd::smdscheme.

The third one is the finding worth keeping. The exported package was
eight smdscheme targets plus fixpoint -- no cl target and no kit target
was installed at all, so the tree the project actually works in had
never been install-tested. That was invisible while the old exports
still passed.

So this is not a deletion. godbolt_cl.cpp replaces the eleven with one
example that reads, elaborates and evaluates a form in a constexpr
context and pins the answer with a static_assert. The package now
exports kit.foundation and the cl libraries, and three install tests
compile against the installed include tree to prove those headers are
reachable and self-contained where they never had to be before.

The trees themselves still build here. A5 removes them.
EOF

git checkout retire-three-trees
git merge --no-ff step-a4-repoint-consumers
```

## Record measurements

After the merge, before cleanup.

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"A4","lane":null,"outcome":"green","wall_seconds":<measured>,"attempts":<n>,"verify":{"command":"make test-matrix + make testinstall","exit_code":0,"wall_seconds":<measured>,"log_bytes":<sum of both logs>,"summary_lines_read":<n>},"diff":{"files_changed":<n>,"insertions":<n>,"deletions":<n>},"out_of_scope":[],"note":""}
EOF
```

Sum both verify logs for `log_bytes` and both wall times for the verify wall
time, and say in `note` that this step's verify is two commands — the
integration review needs to know why this row is heavier than its neighbours.

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-a4-repoint-consumers
```

Mark A4 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff

Read `tmp/plan/step-A5.md`, then write `tmp/plan/handoff-A5.md` (≤ ~150 lines).
A5 deletes both trees. Tell it: the new exported target set, exactly as it
landed; whether `cl.conformance` is in it and why; whether `-freflection` was
arriving transitively through `smd::smdscheme` and what happened to it; the
baseline and after results of `make testinstall`; and the new example's target
name and ctest name, because A5's ctest count arithmetic depends on it.
