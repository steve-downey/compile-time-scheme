**DRAFT &mdash; pending author revision**

<div class="abstract" id="org542e566">
<p>
Step B1 opens Phase B by splitting <code>src/smd/cl/reader/read.hpp</code> into eight headers under <code>detail/</code> plus an umbrella, and changing nothing else.
No test file is touched and <code>ctest</code> holds at 311 entries on both matrix legs.
607 lines become 785 across nine files, and the difference is eight prologs, eight include guards, and eight include lists worked out from what each file uses.
<code>detail/read_node_fwd.hpp</code> is the piece that makes the split work.
The sentence explaining it names three callers where the code has two, in the step brief, the commit message, two code comments and the architecture document.
On the first <code>make lint</code> run, <code>clang-format</code> sorted the umbrella's includes alphabetically and left a hand-written "dependency order" comment standing over a list that no longer had one.
The comment was rewritten to describe the forward declaration instead, because order stopped mattering the moment every header became self-contained.
The architecture paragraph written in the same commit still claims the order, and it sits directly above a transclusion of the list that disproves it.
The step's own <code>wc -l</code> spot check expects <code>~60</code> against <code>~583</code> and prints 94.
That file was 607, and 583 is roughly what <code>read.hpp</code> held until the Monad typeclass widened it, on the day the plan was written.
At 695 seconds the step is the plan's declared floor for Phase B, and it is slower than A5 and twice A3.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 38 - Forty-Six Strings, and a Spot Check That Could Not Run ←](phase-38-forty-six-strings.md)

</nav>


# A branch, and somewhere for seven more steps to stand

Phases 33 through 38 were Phase A, which was mostly subtraction. Two front ends frozen to tags and then deleted, and their build consumers cut. A printer and an SBCL differential went in to replace the oracle that left with them. Phase B runs the other way, on a branch of its own called `cl-parser-combinators`. Over eight steps the reader gets rewritten onto a combinator layer. B1 is the first of the eight and writes no combinator at all.

`src/smd/cl/reader/read.hpp` was 607 lines. Eleven `template <class Ctx>` declarations covering ten functions, because `read_node` appears twice, once forward and once defined. Two public entry points, a ten-entry character-name table, and two `using`-declarations.

The argument for splitting before touching any of it is about diffs. Every later step edits some part of that file. Without a split, all seven would declare the same single file as their scope, and every diff would be read against a moving target. And the claim Phase B is making is D31's: nothing observable changes. Diffs are how that gets checked.

The eight go under `src/smd/cl/reader/detail/`, where the directory and the `smd::cl::reader::detail` namespace agree. `docs/cpp-rules.md` says a `detail/` header big enough to need its own tests is a component and should be promoted. None of these is. So none of them gets a test file, and the reader's test count is untouched by construction.

`read_context.hpp` 99 lines, `skip.hpp` 65, `read_node_fwd.hpp` 24, `forms.hpp` 94, `text.hpp` 134, `sharpsign.hpp` 138, `token_datum.hpp` 71, `node.hpp` 66. `read.hpp` keeps 94: the include list, and `read_datum` and `read` with their documentation. 785 lines where there were 607.

Two of the eight are odd in small ways. `detail/read_context.hpp` opens `namespace smd::cl::reader` before it opens `detail`. `default_max_nodes` and `default_max_list` live at the outer scope and are `read.hpp`'s default template arguments, so moving them inward would change their qualified name. The step brief flags that, and the code respects it. And `skip.hpp` holds no template at all, which the umbrella's description of all eight as "the Ctx-templated reader machinery" misses. `skip_block_comment` takes a cursor, `skip_intertoken_space` a cursor and a readtable, and the one header `read_node` reaches first is the one with no `Ctx` in it.


# The declaration that makes it a split rather than a pile

`detail/read_node_fwd.hpp` is twenty-four lines, five of which are the comment saying why it exists.

```cpp
/// Forward declaration only: @ref read_wrapped, @ref read_delimited and
/// @ref read_sharpsign all call @c read_node, and @c read_node calls all of
/// them. Everything here is a template, so a declaration suffices at each
/// call site; the definition (@c detail/node.hpp) need only be visible at
/// the point of instantiation, which the @c read.hpp umbrella guarantees.
template <class Ctx>
[[nodiscard]] constexpr auto read_node(cursor cur, Ctx &ctx)
    -> foundation::result<parse_state<int>>;
```

The mechanism is right. Everything on both sides of the cycle is a template, so a declaration is enough at the call site, and the definitions need only be visible at the point of instantiation, which the umbrella guarantees by including all eight.

The census is wrong. `read_wrapped` and `read_delimited` call `read_node`. `read_sharpsign` doesn't: it calls `read_wrapped`, `read_delimited`, `read_character` and `read_radix_number`, and reaches `read_node` only through the first two. The cycle is two functions wide.

`sharpsign.hpp`'s own include list says so without being asked. It pulls in `detail/forms.hpp` and `detail/text.hpp`; it does not pull in `detail/read_node_fwd.hpp`, because it has no call that needs the declaration. All of the new headers worked out their includes from what each one actually uses, instead of inheriting the union. So the header with no use for the forward declaration is one of the three that declaration's comment names as callers.

The claim about three callers is in five places: the step brief, the commit message, the umbrella's comment, the declaration's own docstring, and the new section in `docs/compiler_architecture.org`. Nothing follows from it being wrong, since `forms.hpp` needs the header either way. What follows is smaller than the prose suggests. `forms.hpp` is the only header that includes the forward declaration in order to call through it; `node.hpp` includes it to define what it declares, and the umbrella includes all eight regardless.


# The comment about an order

The first `make lint` run sorted the umbrella's eight `detail/` includes alphabetically and exited 2. The exit code is documented behaviour here: the run that rewrites fails, and the next run over the same files passes.

Above the list was a comment saying the list was in dependency order. `clang-format` sorts includes and doesn't move comments to match, so the comment was left standing over a list it no longer described.

Restoring the order, in order to keep the comment true, would have taken a blank line between groups or a line of configuration. What landed instead is a rewritten comment.

```cpp
// The Ctx-templated reader machinery, split (step B1) into these eight
// headers under detail/. clang-format sorts this list alphabetically
// rather than by dependency, which is fine: each header is self-contained
// and pulls in whatever it needs, so repeated inclusion through several
// paths is a no-op under the usual include guards. The one dependency
// worth naming is read_node_fwd.hpp, the forward declaration that lets
// read_wrapped, read_delimited and read_sharpsign (in forms.hpp,
// sharpsign.hpp) call read_node before node.hpp defines it -- every
// function on both sides of that cycle is a template, so a declaration
// suffices at the call site and the definition need only be visible at
// the point of instantiation, which this umbrella guarantees by including
// all eight.
#include <smd/cl/reader/detail/forms.hpp>
#include <smd/cl/reader/detail/node.hpp>
#include <smd/cl/reader/detail/read_context.hpp>
#include <smd/cl/reader/detail/read_node_fwd.hpp>
#include <smd/cl/reader/detail/sharpsign.hpp>
#include <smd/cl/reader/detail/skip.hpp>
#include <smd/cl/reader/detail/text.hpp>
#include <smd/cl/reader/detail/token_datum.hpp>
```

Order had stopped mattering. Every one of the new headers is self-contained and sits under its own guard, so arriving twice by two paths is a no-op, and the umbrella's job is to make every definition visible somewhere, with no sequence implied. The comment was documenting a constraint that the split had just removed, which is why the formatter could break the comment without breaking anything else.

That fix went into the file. The prose written beside it in the same commit was not. `docs/compiler_architecture.org` gains a B1 section saying that `read.hpp` "shrinks to an umbrella that pulls the eight in dependency order". A paragraph later it has the umbrella guaranteeing instantiation "by including `read_node_fwd.hpp` before every caller and `node.hpp` last, after everything `read_node` dispatches to". `node.hpp` is second in the list. `forms.hpp` is first, and `forms.hpp` is a caller.

The architecture document is the living one, so it resolves against the worktree instead of a tag. At the merge, the paragraph claiming the order rendered directly above the include block that sorts alphabetically, on the same page.


# What zero behavioural diff is made of

Three checks stand behind the claim.

`git diff --name-only cl-parser-combinators..HEAD | grep '\.test\.cpp$'` returns nothing. That's the acceptance criterion made mechanical: had the split changed behaviour, some test would have had to move, and the brief's instruction for that case is to stop instead of adjusting the test.

`ctest` holds at 311 entries per leg, both legs 100% passing, and the brief requires exactly the baseline, not one more and not one fewer. A split that added a bootstrap test file per header would have failed that as surely as one that lost a case.

`make compile-headers`, with `CMAKE_VERIFY_INTERFACE_HEADER_SETS` on, builds each of the eight as its own translation unit. This is the one that checks the umbrella's new comment rather than the reader's behaviour. A header that compiles only because something else included its dependencies first passes every test in the suite and fails here.

There is no check in any of the three for whether the code that moved is the code that arrived. Nothing compares the 691 lines in eight files against the 607 they came out of. The tests agreeing afterwards is real evidence, and the diff is short enough to read, and that's all of it.


# Two numbers in a one-line check

The spot-check block ends with this:

```sh
wc -l src/smd/cl/reader/read.hpp                       # ~60, not ~583
```

It prints 94. The file it replaced was 607.

583 was nearly true once. `read.hpp` held 582 lines from step R5 in early August. Then `and_then` became a spelling of the Monad instance's `bind`, and the file went to 607. That widening landed on 2026-08-18, the same day the combinator plan was committed. So the brief is a line out and a fortnight behind. About a file it had read closely enough to enumerate in source order and split into eight correctly.

The other number has no such history. `~60` is what someone expected an umbrella to weigh, and this umbrella carries two entry points, one of them with a three-line `@tparam` block.

Neither number can make the check fail. `wc -l` prints what it prints, a comment beside it says roughly what to expect, and the comparison is done by a person or by no one.


# The warning that arrived with nothing to catch

The handoff into B1 carried the sizing baseline: 311 entries per leg, 159 seconds for a cold matrix in a fresh worktree, and a note about where the combinator layer lives now, which is at a tag. `src/smd/smdscheme/parser/` is not in the worktree. It left with the rest of `smdscheme` when A3 deleted the tree, and it is read with `git show iteration/smdscheme-final:<path>` or not at all.

The last item concerned the plan and not the code. Six step-file inaccuracies had been found and reconciled across A1 through A4. A5 found a seventh, the spot check that globs two directories down for a binary that is six down. The advice passed forward was to run `find .build -name` against any `*/*` pattern before trusting it.

B1's spot checks never touch the build tree. `wc -l`, `ls`, a `grep -c` on a UUID, a `grep -rn` counting `scan_token` and `classify_number` call sites, and the `git diff --name-only` above. Not one of them is a path into `.build`. So the warning had nothing here to apply to.

The inaccuracy in this block is the other kind, and it fails in the opposite direction. A glob that matches nothing prints nothing and exits 0, which reads the same as a quiet pass. A `wc -l` printing 94 under a comment expecting 60 is loud, and gets read by whoever is watching the terminal. That makes eight.


# The floor, and what is under it

`tmp/plan/metrics.jsonl` records the step as green, one attempt, 695 seconds, with verification at 55 seconds warm against the 162-second cold baseline measured in the same step.

The brief says what the number is for. "This is the plan's measured floor: a pure code motion with no logic in it. What it costs is what any Phase B step costs before it does any thinking, and the integration review compares every other row against it."

It is not the smallest row on the board. A5, immediately before it, built a 41-case corpus and a five-case error table against a real SBCL and came in at 687 seconds. A3 removed 173 files and 31,155 lines in 354.

Deleting is cheap. This step wrote eight prologs, eight guards, and eight include lists. Then the `FILE_SET` entries, an architecture section with two transclusions, and one comment it had to write twice. 735 insertions against 537 deletions across the merge, for an outcome whose entire claim is that nothing happened.

Nothing the reader does changed, and `ctest` can confirm that much. The only thing in the reader that did change is a comment, and it changed because a formatter disagreed with it.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 38 - Forty-Six Strings, and a Spot Check That Could Not Run](phase-38-forty-six-strings.md)

</nav>


# References
