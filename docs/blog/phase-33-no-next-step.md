**DRAFT &mdash; pending author revision**

<div class="abstract" id="org1e3261f">
<p>
<code>AGENTS.md</code> opens its task protocol with "work only the next unchecked step in <code>checklist.md</code>", and after R8 there was no such step.
That file is ticked through the original phases, the pivot's L0 to L24, the rebuild's R0 to R8 and the blog backfill, and then it stops, so an agent following the project's own rules finds nothing to do.
Step A0 is the repair, and it touches no C++ at all.
Four other records had come loose the same way and are fixed in the same pass: two scoping notes still calling themselves unratified proposals weeks after being merged, three backlog items whose gating conditions had passed unnoticed, and a phase sketch for a plan no one is going to execute.
Decision D32 is the one with consequences, and it overrules a ratified decision.
D22 made parity with <code>smdlisp</code> the first phase of the language plan and retiring <code>smdlisp</code> that phase's merge criterion; the new plan deletes it three steps from here, before the phase opens.
D22's oracle argument was measured before being overruled and it was substantially right, so what settles it is a narrower distinction: <code>smdlisp</code> is a source of programs and can not be a source of expectations, and a tag prints source programs for free.
DIV-0029 records the thing this repository turned out to have no mechanism for at all: how a ratified decision gets superseded.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 32 - Extracting the Kit, and a Bar That Measured the Wrong Thing ←](phase-32-extracting-the-kit.md)

</nav>


# No next step

The first line of `AGENTS.md`'s task protocol is short: work only the next unchecked step in `checklist.md`. That file is how this repository describes itself to whoever reads its rules next. Every plan the project has run wrote its steps there and ticked them off as they landed.

By late August it was ticked all the way through. The original steps, the pivot's L0 through L24, the rebuild's R0 through R8, the blog backfill's B5 through B11, every box, and then the end of the file. Two scoping notes had been ratified by merge in the meantime and a fourteen-step fan-out had been written against their decisions, and none of that is in `checklist.md`, because a plan sitting in `tmp/plan/` is not the repository describing itself. It is a plan describing a plan.

So an agent doing exactly what it was told finds nothing to do, silently, and correctly. A0 exists to fix that, and the fix is fourteen lines of Markdown.


# Going to look is what found the rest

No one schedules a step for fourteen lines. Four other records had come apart the same way, and they're all one kind of thing: a document that stopped matching the tree and had no way to notice.

Both scoping notes still opened with `*Status:* proposal, awaiting ratification. Nothing here is authoritative yet.` They merged as PR #48 and PR #50 on 2026-08-22, and the plan this step opens is built on their decisions. A line that's false about its own document is a special kind of stale, because everything downstream of it reads as provisional too. Both now say ratified, by which merge, on what date, with one clause I asked for in place of a stronger one: as provisionally as anything here, since later facts and experience can override an earlier decision. D32, below, is the first exercise of that.

Three backlog items had passed their own gating conditions without anyone noticing, which I'll come back to. `docs/cl-parser-scoping.md` § 4 still carries a P1 through P10 phase sketch no one is going to execute, because most of what made that sketch wide was keeping two dead front ends alive. It gets a second dated amendment of its own, appended under the first. And there were two decisions both numbered D22, one ratified and one about to be written.

The interesting edit in `checklist.md` was not the new section. It was this line, in the rebuild's part of the file:

> `src/smd/smdlisp/**` is frozen as a behavioural oracle from R1 onward and is never edited.

True on the day, and it stops being true three steps later at A3. Deleting it would be a lie about today; rewriting it in the past tense would be a lie about the next three steps. So it says both halves now: it holds today, and D32 retires it at A3. A rule with a scheduled end date is a different kind of thing from a rule, and an agent reading only that file should be able to tell which one it's looking at.

The R8 line got the same treatment for a duller reason. It says R8 extracted seventeen `foundation/` files. R8 did move seventeen, and the line is a misleading thing to read today, because `src/smd/cl/foundation/` now holds eighteen forwarding shims: `monad.hpp` arrived afterwards, when the Monad typeclass landed in the kit. Quietly changing seventeen to eighteen would have made the line true and the history wrong. Both numbers, one line.


# Measuring the oracle before overruling it

D22 is ratified, and it says parity with `smdlisp` is the first phase of the language plan, with retiring `smdlisp` as that phase's merge criterion. Its argument is a good one. Every step of the spine has, in `smdlisp`, a working reference implementation of the same operator with the same reader in front of it: a cheaper and more exact oracle than SBCL for the semantics that are subtlest, because it already made this project's choices. Throwing that away before using it is the waste.

Step A3 of the plan A0 opens deletes `smdlisp` before that phase begins.

That overrides a ratified decision's sequencing, and the easy thing would have been to let it arrive as a consequence of tidying up. The plan measured first instead, because no one had ever assessed `smdlisp`'s value as an **evaluator** oracle; the case for retiring it had only ever cited `reader/oracle_compare.test.cpp`, twenty-eight fixed literals whose inputs are covered elsewhere.

`conformance/corpus.hpp` holds seventy-five entries, and because `satisfies()` runs `evaluate_program()` they can only reach what elaborates: `cl`'s seven special operators and eighteen builtins, and nothing else. `smdlisp` holds roughly 120 genuinely novel source-in, value-out evaluated cases, ninety-six of them in `closure/eval_direct.test.cpp`, covering `LET`, `LET*`, `SETQ`, `LAMBDA`, `FUNCALL=/=APPLY`, `DEFMACRO` and backquote, `CATCH=/=THROW`, `TAGBODY=/=GO`, `DEFVAR=/=DEFPARAMETER`, and multiple values. The corpus has none of it. D22's premise survives its own measurement.

What `smdlisp` has is programs. What it does not have is expectations. D16 puts correctness in an external authority; D22's own second limit says that where `smdlisp` pins a `defect` it is a wrong answer by design; D26 says a lane's entries are derived from the specification and checked against SBCL, not written afterwards to describe what was built. Taking `smdlisp`'s answers into the corpus would need a third provenance kind meaning "this project's earlier implementation agreed", putting "this implementation agrees with itself" back inside the one artifact R6 built to get it out of. SBCL is at `/usr/bin/sbcl` and R6's harness already shells out to it, so every harvested entry would be re-adjudicated against SBCL anyway, at which point `smdlisp` contributed a source string and nothing else.

And a tag supplies source strings losslessly, for free, forever. After A1 freezes it, `git show iteration/smdlisp-final:src/smd/smdlisp/closure/eval_direct.test.cpp` prints all ninety-six cases. Harvesting them now would land about 120 corpus entries for operators that do not elaborate. Either a red build, or inert data. The tag is already inert data.

So D32 records the retirement plus a standing obligation on every lane of the language series, in these words: before writing a lane's conformance entries, read the corresponding `smdlisp` test file out of `iteration/smdlisp-final` for source programs, and take every expectation from SBCL or the specification. The obligation keeps D22's substance and delivers it at the moment it has a consumer, which is where D26 already put it. It costs no step.

None of which is the deciding reason, and the record says so. The deciding reason is friction, and it's mine: agents keep getting hung up on the trees they must not touch and doing strategically wrong things, sometimes without saying so, while the trees are there to be gotten hung up on. That friction shows up every session. D22's benefit shows up once.

There was a third way this could have gone, and it turns out never to have been on the table. `smdlisp.smdlisp` links `smdscheme.foundation` and `smdscheme.parser`, and six of `smdlisp`'s seven sub-library `CMakeLists.txt` files link a `smdscheme.*` target. Keeping `smdlisp` through the language series meant keeping both dead front ends, and `smdscheme`'s deletion was never in question.

D32 is marked provisional and names what would revisit it: a lane finding that reading source programs out of a tag is awkward enough in practice to be worth a real harvest step for that one file.


# Nowhere to file it

Writing D32 turned up a smaller question with no answer anywhere in the repository. Where does an override of a **ratified** decision go?

`docs/divergences/` looks like the right drawer. Deliberate deviations get recorded there, classified as `defect`, `scope-decision`, `artifact-of-D1`, `toolchain` or `process`, and the directory is append-only, which is the property you want. It's the wrong drawer, and checking is quick: no divergence record in it has ever superseded a decision. DIV-0004 and DIV-0016 both carry "supersedes" sections, and both supersede other divergence records. Every precedent for overriding a decision is decision-on-decision written into a plan document, D11 retiring D1 and D21 relaxing what D11 carried forward.

Filing the override as a divergence would have classified a deliberate change of course as a deviation from authority (a category error), and it would have hidden the override from anyone who reads the decision list and not the divergence list. So DIV-0029 records the convention instead of the deviation, and it is four short sections: a decision that overrides a ratified one is written as a new numbered decision, appended to the note that owns the decision being overridden, naming it and saying which part of it survives. Which is why D32 sits in `docs/cl-language-scoping.md` and not in the closed rebuild plan. An override belongs physically next to the thing it overrides, where no one can read one without meeting the other.

One number to keep straight: D32 is this step's decision, DIV-0032 is step A3's reserved divergence number, and they have nothing to do with each other. Both step files say so in as many words, because an agent that conflates them writes the governance record into the wrong directory.


# Three gates that had already fired

Backlog items here are refined in place. From outside, a live item and an abandoned one look the same.

BL-0002 wanted the node arena sized from a compile-time measurement, and its status said "open, unblocked by D14, revisit during step R1". Its own body said to revisit it during R1 and close it if D14 made it moot. R1 landed in June. No one revisited it, and the honest move was to answer the question instead of deferring it a second time. `grep -rln "MaxNodes\|Capacity"` across `src/smd/cl` outside `foundation/` returns nothing, the `eval::value` variant is capacity-free, and `core_ast`'s own header documents its per-node leaf and tag types as constants, not template parameters, for D14's reason. `datum_tree<MaxNodes, MaxList>` being parameterised is not a counter-example; that's storage, and storage is where D14 puts capacity. Closed. Its measurement tables were taken on `smdscheme` and stay in the file as evidence for what the old coupling cost. They used to be making a different claim.

BL-0003 wants a `defun`'s implicit block elided by a `cata` rather than an evaluator special case, and it wanted R6's corpus first, because eliding a block changes which programs fit in a given frame budget. R6 landed seventy-five entries. The relevant ones are `block.1`, `block.4`, `block.8`, `return-from.1`, `return-from.unaimed`, `defun.1`, `defun.wrong-arity` and `defun.recursive`, and not one of them goes anywhere near `limits::frames` (256). `defun.recursive` is the deepest and it counts a short list. So the corpus the item was waiting for arrived and did not change the answer. Its revisit condition stands word for word, restated only to record that R6 was checked against it instead of skipped again. Not a productive-looking edit, and the whole difference between an item no one has looked at and one somebody looked at last month.

BL-0004 was disagreeing with itself. Its origin line and its "What" section both said seventeen forwarding shims, and a paragraph further down explained that there are eighteen since the Monad typeclass landed. `grep -l 'Forwarding shim' src/smd/cl/foundation/*.hpp | wc -l` prints 18. It says eighteen throughout now, with `monad.hpp` marked as the one shim that never had a `cl`-side definition to forward from: the typeclass was written in the kit directly, because `result` and its other typeclasses were already there, so the shim exists purely so `cl` code spells its includes the way the rest of `cl` does. `monad.hpp` is therefore the cheapest of the eighteen to retire, and the least informative about whether the kit boundary has settled. Its "Frozen-tree impact: none" paragraph was about to stop meaning anything as well, since both of its clauses are about trees A3 deletes.


# What a paragraph costs

Nothing in A0 touches a line of C++, and its merge criterion is still `make test-matrix`, both legs. 1123 tests per leg before, 1123 after; the step brief says flatly that a documentation change which moves that count has done something else too. Four hundred-odd seconds of cold build, twice, to establish that some prose did not alter the behaviour of a compiler. The plan calls A0 its documentation-only floor and asks for a line in `metrics.jsonl` saying whether the matrix dominated the step, which of course it did. A rule that fired only on code would be cheaper. It would also have to be able to tell the difference.

There is a smaller circularity here. § 4 of the parser note had reserved "phases 33–42 and DIV-0029 through DIV-0033" for a ten-step plan. The plan actually written is fourteen steps, so the reservation is now phases 33 to 46 and DIV-0029 through DIV-0042, one pair per step, claimed up front so concurrent lanes can not collide on the next free number. This post is phase 33 because of a correction made by the step it describes.

None of this was caught by a test. `make test-matrix` can not fail on a paragraph, and `make lint` only checks that the paragraph is spelled correctly. The only check on a document claiming to describe the tree is somebody reading it with the tree open, and between R1 and here, no one ran it. Three of the five things A0 repaired were self-reporting: the notes said they were unratified, the backlog items named the step that would revisit them, BL-0004 said seventeen in one paragraph and eighteen in the next. All of it sitting in the open, in files an agent reads at the start of every session. What that implies about the documents no one thought to check is the part I have not checked.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 32 - Extracting the Kit, and a Bar That Measured the Wrong Thing](phase-32-extracting-the-kit.md)

</nav>


# References
