**DRAFT &mdash; pending author revision**

<div class="abstract" id="org3ecaf00">
<p>
The last of the eight steps changed no source. It measured, and it wrote.
Three things were owed and none of them was code.
What the combinator layer costs at compile time; what became of a divergence record whose revisit condition had just fired; and one section saying what the reader is now.
The cost was supposed to be operation count. It is not.
The reader's own translation unit went from 149541 evaluated constant-expression operations to 1046872, seven times as many.
Three percent of what GCC allows before it stops.
What the series actually spends is constant-evaluation stack, and this step measured its slope instead of one point.
A probe outside the repository reads N nested quotes through the reader at <code>static_assert</code>.
Bisecting <code>-fconstexpr-depth</code> at six values of N, in both trees, gives two straight lines.
2.0 frames per level of source nesting before the series, 5.0 after.
Extrapolate to GCC's default cap of 512, then bisect to confirm the extrapolation.
Source nested 236 levels deep would have compiled before. 93 levels compiles now.
The tree's own deepest constant evaluation is sixty-four nested quotes, written nine phases back to overflow the printer's text buffer, and it went from 167 frames to 363 with 149 to spare.
Every bisected number in that section reproduces exactly.
The two lines don't share an intercept, though, and the note says they do.
It says so in the one place where the difference decides an answer.
The series does not end below where it started. It ends at a little over twice the stack.
Nor is the largest jump the step that noticed it.
Through five of the seven steps that touched code the probe does not move at all; the sixth takes the nesting ceiling from 236 levels to 78, and the seventh puts fifteen back.
The first cold-build comparison was measuring <code>ccache</code>, which the project configures as the compiler launcher, so a build from an empty directory is a warm build.
Both sides were run again with it off.
The claim the whole series rests on is that nothing observable changed, and the witness is a test file that still passes.
Rather than read the diff, the step extracted every input-and-expected-message pair from that file at both ends and compared the two as sets.
Seventeen before, twenty-one after, none removed, none altered.
Each of the four additions pins a diagnostic nothing had ever tried.
And a divergence record dissolved instead of closing.
It said the parser combinator layer had no client in this front end, and its revisit condition named what would change that.
These eight steps are that thing. The condition fired. The record still did not close.
Its framing counted clients to three, and the closing note reads that three as the repository's three front ends, two of them since deleted.
That is not what was counted.
Two of the three were the two existing copies of the layer, and one of those copies has never been in this repository at all.
The conclusion survives the correction. The premise does not.
Four files, 96 insertions, one attempt, twenty-five minutes, and most of the twenty-five is measurement.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 45 - The Readtable Dispatch, and a Wall at 512 ←](phase-45-a-wall-at-512.md)

</nav>


# The limit that was supposed to bind

The decision governing this step says the compile-time price gets measured and recorded. Not gated.

The reasoning was written down at the time. The restrictions of `constexpr` are valuable here as a correctness discipline and as a demonstration, rather than as a product feature. So a large constant factor is an acceptable price for structure that is correct by construction. Cost does not veto. Only a qualitative failure counts against, and one is available: a limit that cannot be raised.

The limit the step was pointed at is `-fconstexpr-ops-limit`, which GCC defaults to 33554432 (taken from the compiler's own diagnostic rather than from the manual). Bisect for the smallest limit at which a translation unit still compiles, in the tree the series started from and in the tree it ended at:

| minimal `-fconstexpr-ops-limit` | before  | after   | ratio |
|------------------------------- |------- |------- |----- |
| `read.test.cpp`                 | 149541  | 1046872 | 7.0   |
| `prin1.test.cpp`                | 3323259 | 3429007 | 1.03  |

Seven times the evaluated operations. A real constant factor, then, and still three percent of the default budget. The printer's file is the control, and it moves by three percent, because almost everything it evaluates is printing.

Except that the two columns are not the same file. The reader's test file gained four expectations and two predicates over the series, and an ops limit is a property of a translation unit rather than of the reader inside it. Hold the file fixed, compile it against both readers, and the number changes shape:

| `read.test.cpp`        | before reader | after reader | ratio |
|---------------------- |------------- |------------ |----- |
| as the series found it | 149541        | 260368       | 1.74  |
| as the series left it  | 208953        | 1046872      | 5.01  |

1.74 is what the conversion costs on identical input. The rest is one added case. It is the one worth looking at. `string_capacity_boundary` reads a 129-character string literal at `static_assert`, to pin the reader's 128-character limit on both sides of the boundary. Comment out that single `static_assert` and the same file needs 325965 operations after against 171779 before. Put it back and it carries 720907 operations under the new reader against 37174 under the old. A factor of nineteen, for one string.

Which says what the layer actually charges for. The old string reader was a raw loop. The new one is a repetition over an ordered choice, reifying a parser per character, so the conversion costs operations per **character of input** where the old reader spent them per datum. Almost nothing in this tree reads a long literal. The one thing that does is a test the series itself added, to pin a boundary nothing had pinned.

So the flag the step was told to look at was never close to binding. Raising it would have been free anyway, although the number it produced has been quoted since as though it were the conversion's price.


# The limit that did

The reader's recursion is ordinary C++ recursion. That sounds like a truism and isn't. The evaluator was built as a small-step machine with an explicit frame stack, on purpose, so that evaluating a deeply nested Lisp form would not recurse in C++ at all. The project's rules say so in as many words. The reader was never part of that. `read_node` calls `read_wrapped` calls `read_node`, and inside a `static_assert` every one of those is a frame the constant evaluator counts.

Reifying a parser adds frames to that chain. The step before this one found out when a translation unit it had not touched stopped compiling. What it came away with was a single before-and-after on one file. One point does not measure a slope.

So this step wrote a probe, outside the repository, that reads N nested quotes and nothing else:

```cpp
using sym_table = smd::cl::symbol::symbol_table<int, int, int, 64, 512>;

constexpr auto spelling() {
    std::array<char, probe_n + 1> buf{};
    for (int i = 0; i < probe_n; ++i) { buf[i] = '\''; }
    buf[probe_n] = 'x';
    return buf;
}
constexpr auto buf = spelling();

constexpr auto reads() -> bool {
    sym_table syms;
    return smd::cl::reader::read<4096, 8>(
               std::string_view{buf.data(), buf.size()}, syms).has_value();
}

static_assert(reads());
```

Bisect `-fconstexpr-depth` for the smallest value at which that compiles, at several N, in both trees:

| nesting depth N | before | after |
|--------------- |------ |----- |
| 1               | 41     | 48    |
| 2               | 43     | 53    |
| 4               | 47     | 63    |
| 16              | 71     | 123   |
| 64              | 167    | 363   |
| 100             | 239    | 543   |

Both columns are linear, and not approximately. Before the series the cost is `39 + 2N`; after it, `43 + 5N`. Every one of the twelve numbers is what those two lines give. Two frames per level of source nesting became five.

A straight line predicts a ceiling. GCC stops at depth 512, and the project sets that flag nowhere. So the deepest source the reader takes with no flags at all is whatever N the line allows. Before: 236. After: 93. Both were confirmed the slow way, by bisecting N with the flag left alone. Both exact.


# Thirty-nine, forty-three

The architecture note reports the same two slopes, and then says they sit "over a fixed overhead of about 39 frames either way". They do not. The overheads are 39 and 43.

Four frames, and this is the one place they change an answer. A shared overhead of 39 predicts a ceiling of 94 for the converted reader. 94 does not compile. The note's own bisected 93 is what `43 + 5N` gives.

Nothing downstream of it moves. The ceiling in the table was bisected, not derived, so the number was right before the model explaining it was. But a section whose whole argument is that the slope is the finding should not misstate the line. The correction is a sentence.


# Two and a half times, and where it came from

Per level of source nesting the series costs two and a half times the constant-evaluation stack it used to. The default-flags nesting ceiling is 39% of what it was. Those are the honest headline, and neither is the interesting part.

Running the probe backwards over the phase's own tags is. The step before this one did that for the printer's test file and found the whole rise in one step. A probe gives a slope, and a slope gives a ceiling, which says something sharper:

| tree                                   | frames/level | ceiling |
|-------------------------------------- |------------ |------- |
| before the series                      | 2.0          | 236     |
| through strings and character literals | 2.0          | 236     |
| the quote family and token data        | 6.0          | 78      |
| the readtable dispatch, as merged      | 5.0          | 93      |

Five of the seven steps that touched code cost nothing. Not nearly nothing. The split into eight headers, the first `bind`, intertoken space, the delimited list, the string reader: through all five the probe returns the pre-series numbers unchanged. None of those functions sits on the path a nested quote takes.

The sixth converted `read_wrapped`, which is the function that path cycles through. It took the ceiling from 236 levels to 78 in one step, reporting twenty minutes and a green matrix on the way past.

The seventh added two dispatchers, which should have made things worse, and gave fifteen levels back instead. The same step reshaped `parser<F>` to **be** its callable rather than hold one, and that saving applies to every parser invocation the six steps before it had placed.

None of which anyone noticed until a build stopped. The margin at the sixth step was 88 frames, under a cap no one in the series had ever looked at.

The tree's own worst case is the printer's sixty-four nested quotes. They were written nine phases back to overflow a 512-character text buffer (a different 512, and unrelated to this one), and have nothing whatever to do with a frame cap. That case went from 167 frames to 363. No other test file in the tree needs more, so 363 is the high-water mark, and the margin is 149.


# A measurement that caught itself

The first whole-matrix comparison was cold in the sense of an empty build directory. Which is not the sense that matters. The project's Makefile configures `ccache` as the compiler launcher, and `ccache` does not live in the build directory. Both sides were measured again with it disabled.

| measurement                    | before | after |
|------------------------------ |------ |----- |
| matrix wall time, cold         | 65 s   | 91 s  |
| C++ translation units compiled | 140    | 152   |
| ctest entries per leg          | 326    | 379   |

Forty percent, and it is the number in the section that means least.

The sentence explaining the twelve extra translation units credited six new headers and six new test files. A plausible composition, arrived at without counting. A follow-up commit enumerated the compiler invocations in both logs instead. All twelve are the kit's six test files, once per configuration, and the matrix runs two configurations. The six new headers are checked by a separate target that does not appear in the count at all. The 53 extra ctest entries belong to the kit as well, none of them in the front end.

The per-translation-unit number answers the question that was asked. The reader's own test file, compiled alone, goes from 1.62 s to 2.01 s at `-O0`, and from 1.86 s to 2.34 s at `-O3`. A quarter more, for the file where all the change landed.


# Seventeen and twenty-one

The promise the series ran under is that it changed no observable behaviour. The evidence offered for that all along has been that the reader's tests pass unchanged.

A claim like that is usually made by looking at a diff and finding it small, which proves nothing about a diff that isn't empty. This one isn't: one test file, 56 lines added.

So the step extracted every `fails_with(input, message)` pair from that file at both ends of the series and compared the two as sets. Seventeen before, twenty-one after. Nothing removed, nothing altered, four added. The single deleted line is a `;` that became an `&&` so the chain could go on, with the same input and the same message on either side of it. Two new `constexpr` predicates are the rest of the 56 lines: the string capacity boundary on both sides of it, and the position `read_wrapped` records for its branch.

The four additions are an unterminated `#|`, flat and nested; a `#\` with the input ending before the character; and a string literal whose last character is a backslash. Every one is an input nothing had ever handed the reader. Block comments were tested, and only for succeeding. `unterminated string` had a test, and only by running off the end in the ordinary way, never through the escape. For those paths a green matrix had been proving nothing. Three of the eight steps left the test file stronger than they found it, while changing nothing that was already in it.

Set comparison is cheap, and rereadable by someone who doesn't trust the person reporting it. It should be how a claim of this shape gets made.


# Two places that stayed worse on purpose

The same section of the note records where the series chose the worse combinator. The scoping decision it ran under says a conversion does not get to improve behaviour on the way past.

An unterminated `#|` still consumes to end of input whatever the nesting depth, and the message the reader raises comes from the caller running out of input, not from the comment skipper. The obvious recursive combinator would point at the unterminated comment, which is a better diagnostic and therefore a forbidden one. So `skip_many` is built to preserve the worse answer. Exhaustion of its inner parser is success. Not failure.

The other is the repetition that reads a delimited list, which owns no container and no capacity. The natural move, hoisting a collecting repetition into a kit, is to hand the combinator the capacity and the message. Considered, and refused. The overflow diagnostic has to sit at the post-skip position of the element that would not fit, and that position stays exact only while the closure holding the cursor also owns the check. So the combinator supplies control flow and the caller keeps the accumulator. What makes that repetition a diagnosing one instead of a truncating one is its caller, which is also why it isn't named for a bound.


# A record that stopped being about this repository

The kit extraction, at the end of the rebuild, took nine `foundation` files and left the parser combinator layer alone. The record of why says the Common Lisp reader was not a client of that layer in any sense. No `parser<F>` wrapper, no `alt`, a hand-written cursor with the parse state folded into it. Extracting the layer would have produced a kit module with no callers anywhere in this repository.

The revisit condition it wrote down was specific. Either the reader gets rewritten onto a `parser<T>`-shaped combinator layer, making the front end a genuine **third** client, or a fourth front end needs the same abstraction.

These eight steps are the first of those, and were aimed at it. The condition fired.

It did not close. What the step recorded is a dissolution, and that is a different thing. Closing a divergence says the situation it describes has stopped being true. Nothing in this one has stopped being true. Every sentence in it is still an accurate account of the code it was written about.

What stopped was the proposition underneath it: that a kit module owes a count of clients before it belongs in the kit. The layer has one client, and one client is what makes it worth having. The lesson the record was itself the evidence for is that extracting a module for its own sake produces something nothing calls. The answer to that turns out not to be more clients, but one real one. A two-independent-copies standard was right for a move, and nine files that already existed twice were a move. This layer is a design.

One decision in the record still stands, and this step moved it from provisional to settled. `map` is not registered as a `functor` instance for the parser type, for the reason it never was. Nothing outside the Monad instance needs a generic `fmap` over a parser value, and seven converting steps produced no caller that does. Registering it anyway would be the over-eagerness the record was written against, an instance whose only justification is that the typeclass exists. The native `fmap` stays reachable through the monad object, which is how every caller in the tree reaches it.


# What the three was

The closing note explains the number. The condition counted to three "because at the time this record was written the repository had three front ends", two of which the earlier phase deleted.

That is not what it counted. The record names its own two copies in the paragraph above the condition: the retired Scheme front end's `parser/`, and the sibling Forth repository's. At the commit where the record was written there was exactly one `parser/` directory in this repository, the Scheme one. The Common Lisp pivot was the third front end here, and it never had a `parser/` directory at any revision. A Lisp reader over a cursor, and nothing else. It was frozen and deleted with no combinator layer to its name.

So the tree retirement removed one of the two copies from trunk. It did not remove the other, which was never here. That one is in a repository this project can read and cannot modify (the sibling Forth tree), and it still holds `cursor`, `parser`, `alt` and `parser_ops`. Those are the four names the record listed.

The record is confusing on the point, in its own defence. "A genuine third client of `parser/`" counts copies of the layer. "A fourth front end in this repository" counts front ends here. Two different rulers, two consecutive lines, one paragraph. The closing note picked up the second and used it to explain a number produced by the first.

The dissolution survives all of that without a scratch. Inside this repository the layer has one client, the fourth-front-end trigger has no prospect of firing, and there is nothing left to revisit. The conclusion was right and the premise was wrong, which is the least satisfying way to be right. It matters because a divergence record is the thing a later reader believes.


# Eight steps

Four files, 96 insertions, three deletions, one attempt, 1501 seconds. The verification took eleven of those, against the 198 the step before it recorded, because no source changed and there was nothing to rebuild.

Across the seven steps that touched code: 31 files, 3322 insertions, 666 deletions, twenty-two files added and nine modified. This step's own four take it to 33 and 3417.

Nine combinators in six headers, and the set closed after the fourth step. Three converting steps in a row added nothing to it, which is the evidence a note marked provisional had been waiting for since the layer was built. One request was made of the layer and never justified: a position parameter on `satisfy`, so that a caller can anchor a diagnostic away from the cursor. Two separate steps were named in advance as the caller that would force it. Neither was.

Each of the eight steps had a divergence number reserved for it. None of the eight used one. The only divergence record the phase touched is the one it dissolved.

The doc comment on the reader's context concept still says that only `read_radix_number` is constrained with it. Seven functions are, plus a forward declaration that had to gain the constraint or it would have declared a different template. That comment has been wrong since the third step, and outside every step's declared scope since the third step. Four consecutive workers have now written it down without being allowed to fix it.

The divergence record's last line is that there is nothing left here to close, revisit, or re-verify. Very nearly.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 45 - The Readtable Dispatch, and a Wall at 512](phase-45-a-wall-at-512.md)

</nav>


# References
