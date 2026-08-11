**DRAFT &mdash; pending author revision**

<div class="abstract" id="org8b49e90">
<p>
R6 builds the corpus decision D16 promised: a place where a program's spelling, its expected outcome, and the provenance of that expectation live together.
Seventy-five cases, split between thirty-five adapted from Paul Dietz's <code>ansi-test</code> (MIT, pinned commit) and forty hand-derived from the ANSI text, because <code>ansi-test</code> leans on strings, characters and the numeric tower, and none of the three are in scope yet.
Every case states its outcome by channel (value, diagnosed error, or unwind), the same three R5 spent a whole phase splitting apart.
Only one entry pins anything, and D16 says that's the most any entry is allowed to pin.
Building the seventy-five found six new divergences, and the SBCL oracle closes a question this series has been carrying since phase 28: whether the evaluator agrees with anything other than itself.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 29 - Answering Phase 7, and One Invariant That Was Two ←](phase-29-answering-phase-7.md)

</nav>


# Four calls, written down once

`machine.test.cpp` already had the chain install-read-elaborate-run written out, twice: once for a single form, once for two forms sharing an arena, both times through `foundation::and_then` by hand. A corpus is not one form or two, it's whatever a case's author wrote, so the chain had to stop being copied and become a component.

`read_and_elaborate_all` is the sequence R5's brief left as an unfold available but unwritten: `reader::read_datum` already returns the cursor positioned after the datum it read, so reading the rest is calling it again against that cursor until nothing but whitespace and comments is left. Every root it returns shares one `program` tree, because a closure remembers the index of the `core_lambda` node its body lives at, and a function defined by one form and called by a later one needs both forms' nodes to still be there.

`evaluate_program` wraps more than one root in a fresh `core_progn` before handing the whole thing to the machine, and a read failure, an elaborate failure and a run-time failure all funnel into the one `eval::outcome` the two test helpers already agreed on. A corpus case that expects an error should not have to know which stage produced it. The one piece that isn't a straight formalization is `program_report::witness_defined`: a case can name a symbol and ask, after the run, whether it acquired a function definition, which is the same probe `machine.test.cpp` used by hand to watch `unwind-protect`'s cleanup actually run, now written once instead of reached for.


# Two provenances, and the smaller one isn't the one I expected

D16 wants a corpus where a program, its expected outcome, and where that expectation came from all live in one place. `ansi_test_adapted` takes that literally: every entry names, in an `origin` field, the exact `ansi-test` file and `deftest` it came from, and no `deftest` body was rewritten to fit. A body using `let`, `flet`, `values`, `catch` or `tagbody` was simply dropped.

Thirty-five cases made it through that filter, out of a tree of `ansi-test` files that is not small. D10 put strings, characters and the numeric tower out of scope at the pivot, before any of this rebuild existed, and most of `ansi-test` turns out to be about those same three things. `cons.24` (`(cdr '(a . b)) => b`) didn't make it either, for a reason that isn't D10 at all: the reader has no syntax for a dotted-pair consing dot, so `(a . b)` reads as the three-symbol list `(a |.| b)` instead of a pair, and adapting the case would have silently tested the wrong thing. That's DIV-0022, and it's below.

So the adapted set is thirty-five and the hand-derived set is forty, and I went in assuming the ratio would run the other way. It doesn't. The honest reading is that this project's own operator surface is narrower than `ansi-test`'s assumptions; `ansi-test` itself is not thin. `quote` has no dedicated `ansi-test` file at all. Multi-argument arithmetic, `atom`, `consp` and `symbolp`, and all three branches of `unwind-protect` are spec-derived because every adaptable case for them binds something with `let` first. The recursive-`defun` acceptance witness from phase 28 is in there too, as `defun.recursive`, because DIV-0009 closing was itself a claim I wanted pinned as a permanent regression: a `static_assert` can quietly stop being run, and a corpus entry can not.

One more adaptation, worth stating plainly because it's the kind of thing that could be mistaken for cheating: this driver doesn't expose the symbol table, so a `program_report` can't compare a returned symbol against its name. A `deftest` whose expected value is a bare symbol gets wrapped in `eq` or `null` against a quoted literal drawn from the same source string: the same symbol, interned once, in the same run. That changes how the check is written in C++. The CL form under test is untouched, and every such rewrite is recorded next to the entry it applies to.


# The one entry allowed to pin anything

D16's rule is short and has no hedge in it: a test may pin a `scope-decision`, a test must never pin a `defect`. Out of seventy-five entries, exactly one pins anything at all.

`nil.8` asserts `(eq nil 'nil)`, which is DIV-0002: `eq` and `eql` still coincide in this tree, for a different reason than the pivot's. There it was string comparison on a name; here identity is real, and every number that exists is an immediate, so nothing yet separates the two operations. DIV-0002 is classified `scope-decision` in `docs/divergences/README.md`, so the corpus is allowed to assert the coincidence as correct. Nothing else in the corpus comes close: no entry spells `(block nil ...)`, none reaches for a lambda-list keyword, none calls anything through `funcall`, because those are the gaps that would turn a corpus case into an anti-specification. Assert the bug, and fixing it costs a test deletion instead of buying one.

Writing seventy-five cases against those boundaries is what turned up six divergences nobody had filed yet. DIV-0022 is the dotted-pair gap above, deferred since the pivot's own D6 and inherited unreopened by this rebuild's reader step; it only bit here, when a corpus needed the syntax and didn't have it. DIV-0023 is a genuine defect. ANSI has no restriction on `nil` as a block name, and `(block nil ...)` is what `loop`'s implicit block needs, but the atom pass here turns every `NIL` token into a constant before the role pass runs, so `(block nil ...)` is diagnosed the same as `(return-from nil ...)`. DIV-0024 and DIV-0025 are both D19's deliberate scope, stated as the same rule twice: don't add a channel with no producer. Lambda-list keywords are diagnosed; none is silently bound to a parameter literally named `&optional`. `lambda` in expression position, `#'f`, `funcall` and `apply` are all still unreachable, because nothing yet produces a function value for them to consume. DIV-0026 writes up a GCC trunk defect R3 had already worked around in `reader/number.hpp` (a `memchr`-lowered `find` misreports its offset on a view advanced by `remove_prefix`), and DIV-0027 states, now that a corpus is here to feel it, that every elaborator and evaluator diagnostic still carries no source position at all.

None of the six are news to the evaluator; R4 and R5's own step briefs had already noticed most of them in passing. What the corpus did was force each one into a doc with a classification, because "may I pin this" turns out to be a question that only gets asked once something is trying to.


# Someone else's answer

Everything above runs against this project's own evaluator, which is a way of checking that the machine agrees with the specification I read and the tests I wrote. Phase 28 said that's the weakest evidence in the project, and it stayed that way through phase 29. `sbcl_oracle.hpp` is where it stops being the only evidence.

This is a runtime probe, because the orchestrator's decision was that the differential check has to degrade to a skip when `sbcl` isn't on `PATH`. It must never simply fail to configure. An environment without SBCL stays green. A missing oracle is a fact about the environment, not a defect here.

The check is deliberately informal: one form in, one `prin1` line out, compared as text against what a fixnum or `T` / `NIL` expectation would print. It's not a general SBCL-condition-to-`outcome` translation, and the test that drives it only runs the comparison for the corpus's fixnum- and boolean-valued cases: the ones scalar enough for two printers to agree on without either side inventing a shared error vocabulary first. A form SBCL itself signals an error on prints as the literal string `SBCL-ERROR`, which is not a value any well-typed corpus entry can produce, so an error there reads as a plain mismatch rather than a crash.

I'd half-expected shelling out to be the awkward part, and it wasn't; wrapping a two-form corpus entry (a `defun` and then a call) into one `--eval` argument was. The differential test wraps the source in `(progn ...)` for the same reason `evaluate_program` wraps several roots in a `core_progn`: SBCL and this driver both need one form, and a sequence of top-level forms is one form once you say "and then." Two independent pieces of code reaching for the same wrapper is a small thing, but it's the kind of small agreement that makes me trust the comparison more than I would have from either alone.

`sbcl_oracle.hpp` is deliberately not `constexpr`. Every other file in `conformance/` has a compile-time twin (`corpus.test.cpp` runs the whole corpus through `cl/eval` inside two separate `static_assert` expressions, one per array), and this is the one exception, on purpose: a subprocess has no meaning under constant evaluation, so there's nothing to make a twin of. The corpus proves the pipeline agrees with itself at compile time; the oracle test is the only place in this whole area that has to run.


# What's still owed

The corpus checks value, error and unwind shape, never a diagnostic's text against SBCL's condition system, and DIV-0027 means it couldn't check position even if it wanted to. Forty of the seventy-five cases have no `ansi-test` counterpart to compare provenance against at all. That's a fact about the coverage: a hand-derived case cites the ANSI section it encodes, and that's still a citation, just a different kind. And the differential check only ever asked SBCL about the scalar fraction; nothing here compares list structure, and nothing asks it about a program that errors or unwinds, because there's no shared vocabulary yet for what SBCL's condition system and this project's `outcome` agree to mean by "the same failure."

That's the next gap, and it's someone else's problem for now. R7 is next.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 29 - Answering Phase 7, and One Invariant That Was Two](phase-29-answering-phase-7.md)

</nav>


# References
