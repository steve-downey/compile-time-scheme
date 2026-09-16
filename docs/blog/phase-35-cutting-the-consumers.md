**DRAFT &mdash; pending author revision**

<div class="abstract" id="orgbbeb0a3">
<p>
Step A3 deletes two front ends.
Step A2 is what makes that a subtraction instead of a build break.
The build still names both trees out loud: eleven example programs link a <code>smdscheme.*</code> or <code>smdlisp.*</code> target by name, the top-level export list names eight of them, and one test file under <code>src/smd/cl/</code> includes both dead trees at once.
Twelve lines in, 1302 out, across twenty-three files.
The number to distrust is <code>make testinstall</code>, which moved from exit 2 to exit 0 in this step and was not fixed.
It exits 0 because <code>find_package</code> now resolves a package claiming no <code>smd::smdscheme</code> target, nothing is left under <code>installtest/</code> that tries to link one, and zero installed tests run.
The red was a header missing from an installed <code>FILE_SET</code>, and it showed up every time anyone ran the check.
The green is a project that configures and then finds nothing to do.
The step's own spot check is a <code>grep</code> for two strings that has to come back empty.
So the comments left in place of the deleted build wiring can not say which trees they are about.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 34 - Freezing Two Iterations, and Two Links Nothing Checks ←](phase-34-freezing-the-iterations.md)

</nav>


# The deletion runs third, so something has to run second

The plan originally sequenced the deletion fifth. `tmp/plan/README.md` moved it to third, on the owner's argument. Agents keep getting hung up on trees they must not touch, and keep doing strategically wrong things while the trees are there to get hung up on. Phase 34 covered the half of that reordering which is about prose. This is the other half.

The deletion can't run first because the build names things. `src/examples/CMakeLists.txt` carries eleven `add_executable` blocks, and all of them link `smdscheme.smdscheme`, `smdscheme.sender`, `smdlisp.smdlisp` or `smdlisp.sender` by name. The top-level `beman_install_library` call names eight `smdscheme.*` targets. `installtest/` has a helper whose body hardcodes `smd::smdscheme`, and six calls to it, each backed by a `.cpp` that includes only `smdscheme` headers. Run `git rm -r` on the two trees with any of that still in place and CMake stops at configure time, naming targets that no longer exist. A3 would then be a step that deletes thirty thousand lines and repairs the build in the same diff. Two jobs, one commit.

So A2 cuts everything pointing at the trees and leaves the trees alone. They still build after this step, and they still run their own tests. What they no longer have is a single consumer outside themselves.


# The oracle's last file

The interesting deletion is 225 lines: `src/smd/cl/reader/oracle_compare.test.cpp`.

Decision D22's behavioural oracle amounted, in code, to this file. The pivot's reader was never edited, so for the syntax it implemented (fixnums, symbols, keywords, lists, the quote family, comments) the rebuild's reader had to agree with it, case for case. Twenty-eight `CHECK` calls across six substantive `TEST_CASE` blocks, every input a fixed string literal. D32 overruled D22 two steps ago, back in phase 33, so the oracle's authority was already gone. This file is the last thing that still compiled against it.

One surprise, recorded in the step file: an earlier pass at this plan assumed the file reached into `smdlisp` only. It reaches into both.

```cpp
#include <smd/smdlisp/reader/read_datum.hpp>
#include <smd/smdscheme/parser/cursor.hpp>
```

The pivot's `read_datum` takes a cursor that the Scheme-light tree defined and the pivot never redefined. Calling the pivot's reader means building the first iteration's type to hand to the second one. There is a third reach, and it has no include of its own at all. The arena the oracle reads into is `smd::smdscheme::foundation::tree_arena`, which arrives transitively through the pivot's own header. So a grep over `#include` lines alone would have missed one of them, and a grep for `smdlisp` alone would have reported this step already done. Three reaches, two includes, one name.

Then the part that isn't mechanical. Deleting a test is a claim that its coverage lives somewhere else. The step file named two cases it expected to find elsewhere, and asked for the greps rather than trusting itself: the bare fixnum `0`, pinned in `number.test.cpp` and not in `read.test.cpp`; and the comment-inside-a-list case, covered by `read.test.cpp`'s `skips_comments_and_whitespace`. Both checked out. A2 re-derived the other four itself. Of the six, four are case-for-case matches against named functions in `read.test.cpp`.

Two are not. `Lists`'s `"( 1 2 )"` spacing variant has no literal match anywhere. It was ruled a strict subset of `reads_lists()`'s `"( a ( b c ) d )"`, which exercises the same intertoken spacing around parens and between elements. `ErrorsAgree`'s `both_fail("(")` was ruled not a distinct code path from `fails_with("(1 2", "expected ')'")`. Both of those hit the same `cur.empty()` check at the top of `read_delimited`'s loop, one on the first iteration and one later. Nothing was ported.

Both rulings are arguments from the implementation, and either could have gone the other way for a line apiece. A test that pins an input is pinned to the language. A test that pins a code path is pinned to whatever shape the code has this week. `read_delimited` is on this plan's Phase B list by name, to be moved onto a combinator layer. `"("` and `"(1 2"` are the same code path because that function has one loop in it today.


# A spot check that forbids its own documentation

A2's spot check is blunt:

```sh
grep -rn "smdlisp\|smdscheme" src/examples/ installtest/ CMakeLists.txt
```

Must return nothing. Not "no live reference"; no occurrence at all, which is the strictest reading available and the reason it's a good check. It can't tell a `target_link_libraries` line from a comment, so it doesn't let you argue.

The same step file also asks for a comment in each emptied file, saying which examples left and where their replacement is tracked. Those two instructions contradict each other, and A2 noticed. The comments went in reworded to carry the information without the strings. `installtest/CMakeLists.txt` now refers to "the now-dead-ended reader tree". `src/examples/CMakeLists.txt` says the eleven programs that used to live there were "one per dead-ended front-end tree". Neither names a tree, which is the point. The second one is also wrong on its own arithmetic: eleven programs across two trees is not one per tree. A comment written to explain a hole got its count wrong on the way out, and nothing in this repository reads comments.

Anyone who wants to know what actually left has to go to `git`, which is where it is.

The `add_installtest` helper went the same way, for two reasons of which only one is written down. The stated one is that its body hardcodes `target_link_libraries(${name} PRIVATE smd::smdscheme)`. A live function holding a dead target name is the kind of stale reference A3's sweep would then have to reason about. The unstated one is that the string is in the function body, so the helper could not have survived the grep either way. The step file's own scope table records that an earlier draft of that line said the helper was kept, and it resolves the contradiction by declaring the other section authoritative. A plan file with two answers in it, arbitrating between them in a third place. `find_package` stays, so `installtest/` still configures. It is a test project with no tests in it.


# What this project installs

Before:

```cmake
beman_install_library(
    schemepoc.schemepoc
    TARGETS
        smdscheme.smdscheme
        smdscheme.sender
        smdscheme.reflection
        smdscheme.closure
        smdscheme.elaborator
        smdscheme.reader
        smdscheme.parser
        smdscheme.foundation
        smd.fixpoint
    NAMESPACE smd::
)
```

After:

```cmake
beman_install_library(schemepoc.schemepoc TARGETS smd.fixpoint NAMESPACE smd::)
```

Gersemi collapsed the block to one line on its own, which the handoff notes so the next step doesn't read it as a hand edit.

`smd.fixpoint` is the standalone recursion-scheme playground. It is not the compiler, and it never was. So after this step the one installable thing in the repository is the part of it that isn't the project. The live front end and the shared substrate aren't in that list either, and A2 didn't take them out: no version of the list ever named them. The real finding was sitting under eight dead entries the whole time. Putting them in is `docs/backlog/BL-0005-…`, scheduled after the parser-combinator phase. It carries an open question about whether `cl.conformance` belongs in the list at all, since it shells out to a subprocess for its SBCL oracle.


# Eighteen tests, not seventeen

`ctest` goes from 1123 entries to 1105 on both matrix legs, both still at 100%. Eighteen entries left: eleven example tests, and seven `TEST_CASE` blocks out of the deleted oracle file.

The step file estimated "roughly 17", counting the six substantive cases. The seventh is `OracleCompareTest - HeaderIsIdempotent`, the boilerplate every test file in this repository carries: the header included twice at the top, and a `TEST_CASE` whose whole body is `REQUIRE(true)`. The assertion checks nothing. The doubled include is the test, and it finished long before Catch2 was involved. `read.test.cpp` does the same thing to the same header, so nothing was actually lost with it. It still counts, and an estimate built from counting substantive cases is going to miss it every time.

A2 wrote the correction into its handoff so A3 could reconcile against a number instead of an estimate. Two steps running now have handed the next one a corrected count.

Nothing here is transcluded, because A2 added no component and placed no anchors. Twelve of its lines are new: two comments, one rewritten link line, one tick in a checklist, and the one-line `beman_install_library` call above.


# The green that means less

The baseline, unchanged on `main` for as long as anyone can date it:

```
.install/include/smd/smdscheme/closure/cps_code.hpp:6:10:
    fatal error: smd/smdscheme/closure/pairs.hpp: No such file or directory
```

`pairs.hpp` was never added to `smdscheme.closure`'s installed `FILE_SET`, so the installed package has been unusable for as long as no one ran the check. `make testinstall` exits 2.

After A2 it exits 0.

Nothing was fixed. `find_package` now resolves a package that claims no `smd::smdscheme` target, and the six install tests and the helper that linked them are gone. So the project configures, finds nothing to build, runs zero tests and reports success. A3 then deletes the header the error was about, and the error has nowhere left to come from.

Everyone involved saw this coming. That's the part I want to write down. The step file says the exit code may change and that it is not a gate. `tmp/plan/README.md` says `make testinstall` is measured for the trend and gates nothing. The backlog item says it plainly. A red signal became green by having nothing left to check, which is worse than the failure it replaced, because the failure was visible and this is not. Four separate documents saying the same thing, because the exit code is the only part of this a later reader will see without opening any of them.

There was another way to run it, and it is not a silly one. Do BL-0005's work here &mdash; one `cl` example, an export list carrying `kit.foundation` and the `cl.*` targets, three install tests against the installed include tree &mdash; and `make testinstall` never has a window in which it is vacuously green. What ruled it out is Phase B. It is going to rewrite the reader any new install test would be written against, and bundling speculative positive work into the largest and riskiest step of the plan gets this plan no gate it needs. I think that's right. It is still a decision to accept a misleading green number, for however long the backlog item sits, for a smaller diff in the step after this one.

`make testinstall` isn't part of `make test-matrix`; they are separate CMake projects, so no acceptance gate anywhere is going to read that 0. The only thing that reads it is a person.

A2's row in `tmp/plan/metrics.jsonl` records the exit code and, in the same field, refuses to call it an improvement. Not a gate. Recorded for the trend and not the verdict. Zero installed tests expected, and not green in any meaningful sense. Two sentences. The green number is one character, and it's the part that gets read.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 34 - Freezing Two Iterations, and Two Links Nothing Checks](phase-34-freezing-the-iterations.md)

</nav>


# References
