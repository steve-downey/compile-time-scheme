**DRAFT &mdash; pending author revision**

<div class="abstract" id="org761d179">
<p>
This step moves the reader's whitespace and comment skipping onto the combinator layer.
<code>src/smd/kit/parser/</code> gains <code>satisfy</code>, <code>char_p</code> and <code>skip_many</code>, a repetition that discards every value and treats exhaustion as success.
<code>skip_intertoken_space</code> becomes one <code>skip_many</code> over a parser that handles whichever of the three skippable syntaxes applies.
The brief asked for three laws and, separately, for a decision about whether the combinator guards against a parser that consumes nothing or documents that away as a precondition.
The two instructions are not independent, and the law is the one that settles it.
<code>skip_many(skip_many(p))</code> never comes back, and GCC reports that as an operation count exceeding 33554432, which is what an infinite loop looks like when the loop runs in the constant evaluator.
<code>skip_many</code> is itself a parser that succeeds without consuming once its inner parser is spent, so an outer <code>skip_many</code> is handed a success at the same cursor forever.
The guard is one condition: stop on a success that made no progress, the same way it stops on a failure.
The guard is a property of the primitive now, and the question goes forward to whoever writes the next repetition.
The comment skipper did not become a combinator at all. A recursive function with a depth parameter, and it never touches <code>skip_many</code>.
Two new cases pin what an unterminated <code>#|</code> does, flat and nested. Nothing had checked that before, so the conversion had nothing to be measured against until the step wrote the measure.
Both cases went into a file the step's own spot check says must not change.
And the premise ("the reader's last two raw loops") is short by three. <code>forms.hpp</code>, <code>text.hpp</code> and <code>token.hpp</code> each still hold one, each under a comment making the same claim this step set out to stop making.
266 seconds, two attempts, against a declared floor of 695.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 40 - One bind, and the Namespace It Could Not Be Declared In ←](phase-40-one-bind.md)

</nav>


# Two loops with a note excusing themselves

`docs/cpp-rules.md` grants one exception to its ban on raw loops, and the exception is narrow: "A raw `for~/~while` loop is permitted only inside the substrate's own generic algorithms; anywhere else it is a defect, not a style nit."

The reader's skipping code had two loops, and each carried a comment claiming that exception by name. `skip_block_comment` opened with "Substrate generic algorithm: two-character lookahead with a nesting depth makes this the skipping layer's one stateful loop." `skip_intertoken_space` with "Substrate generic algorithm: fixpoint iteration of the three skippable syntaxes, each step built on `advance_while`."

Neither function is in the substrate. Both are in `src/smd/cl/reader/detail/skip.hpp`, which is the front end. So the comments claim an exemption their own file can't have. Deleting them would not have been the fix.

The loops don't go away, and were never going to. What moves is which side of the line they sit on. `skip_many`, the new repetition, holds a `while (true)` in `src/smd/kit/parser/repeat.hpp`, and the comment above it reads "Substrate generic algorithm: the repetition loop every 'run until failure, succeed on exhaustion' combinator over this layer is built from." Same sentence opener, different address. Now it's true.


# A repetition, and the law that would not finish

```cpp
/// Runs @p p repeatedly, discarding every value, until @p p first fails or
/// first succeeds without consuming input, and succeeds with the cursor
/// where @p p stopped making progress.
///
/// Exhaustion is success, not failure -- the opposite of a collecting
/// repetition, whose contract is that running out of input before its own
/// stopping condition is met is a failure. This is deliberate: it is the
/// property @ref smd::cl::reader::detail::skip_intertoken_space depends on
/// so that an unterminated `#|` block comment still reports the caller's
/// end-of-input diagnostic rather than a diagnostic pointing at the comment
/// (decision D31, docs/cl-parser-scoping.md).
///
/// A zero-consumption success from @p p stops the repetition (as if @p p
/// had failed there) rather than looping forever: @c skip_many is itself
/// exactly such a parser -- it always succeeds, and consumes nothing once
/// its own inner parser starts failing -- so without this guard
/// @c skip_many(skip_many(p)) would spin the moment the inner repetition
/// reached its own fixpoint. Guarding here, once, is what lets a caller
/// nest repetitions without re-deriving this by hand.
///
/// @tparam P Parser type.
/// @param p The parser to repeat.
template <class P>
[[nodiscard]] constexpr auto skip_many(P p) {
    return parser{[p = std::move(p)](cursor cur, parse_context auto &ctx)
                      -> parse_result<std::monostate> {
        // Substrate generic algorithm: the repetition loop every
        // "run until failure, succeed on exhaustion" combinator over this
        // layer is built from.
        while (true) {
            auto const r = p(cur, ctx);
            if (!r.has_value() || r.value().rest == cur) {
                return parse_state<std::monostate>{{}, cur};
            }
            cur = r.value().rest;
        }
    }};
}
```

The brief asked for three laws, static-asserted: `skip_many(p)` never fails; `skip_many(p)` where `p` fails is `pure` of nothing at that same position; `skip_many(skip_many(p))` is `skip_many(p)`. A separate paragraph asked for a bounded-progress test, and left the design open: "decide whether the combinator guards against that or documents it as a precondition, and say which in your handoff."

Those two paragraphs are not independent, and the third law is what decides between them. The first draft did take the precondition route, with a doc comment saying `skip_many` must not be called with a parser that consumes nothing. Then the law test ran.

It didn't fail. It didn't finish. GCC says:

```
error: 'constexpr' evaluation operation count exceeds limit of 33554432
(use '-fconstexpr-ops-limit=' to increase the limit)
```

The compiler's suggestion is to let it run longer.

Write that unguarded version back, compile it against this step's own headers, and it reproduces in three and a half seconds. Fast, for thirty-three million operations of nothing happening. The cause is short. `skip_many(p)` always succeeds, and once `p` starts failing it succeeds while consuming nothing, which is exactly the shape the precondition excluded. Wrap one in another and the outer loop is handed a success at the same cursor, forever. The primitive's own documented precondition rules out the primitive itself.

So the guard is one condition on a branch that was already there: `if (!r.has_value() || r.value().rest == cur)`. A success that made no progress stops the repetition the same way a failure does. The idempotency law then holds, and the file checks it against `pure` as well, an explicit zero-consumption parser and the smallest witness of the shape.

The metrics row for this step says `"attempts": 2`. The commit message calls the fix "a one-line fix once the law test surfaced it," and the ordering in that sentence is the whole point. A precondition in a doc comment would have shipped.

The handoff forwards the question and keeps the answer, which is the right shape. It says the next repetition will have the opposite failure contract: running out of input is a failure there. So what carries over is whether its element step can ever succeed at zero width. The guard belongs to this one.


# One call site, and a message no one can read

```cpp
/// Advances past all leading intertoken space: interleaved runs of
/// whitespace, `;` line comments, and `#|...|#` block comments, to a
/// fixpoint, every decision read from @p table.
///
/// @p table is threaded as the combinator layer's context here: the public
/// `read<>` entry point calls this with a bare @ref readtable and no reader
/// context in hand, and `readtable const` models @c parse_context directly
/// rather than being wrapped -- the first evidence in this reader that two
/// different context types coexist, each named in the signature of the
/// parser that needs it (see docs/compiler_architecture.org, B3).
[[nodiscard]] constexpr auto skip_intertoken_space(cursor cur,
                                                   readtable const &table)
    -> cursor {
    return skip_detail::skip_many(skip_detail::intertoken_step)(cur, table)
        .value()
        .rest;
}
```

Three lines, and the middle one is a bare `.value()` on a parse result. That call is total because of the first law in `repeat.test.cpp`: `skip_many` never fails. A law written to check a combinator turns out to license an unchecked accessor at the one place the combinator is used.

The parser it repeats, `intertoken_step`, sits just above the anchor in `skip_detail`. It is a single `parser{...}` with an empty-input guard and three branches in it: whitespace, `;` to end of line, `#|`. The layer has no choice operator yet, and the brief was explicit that inventing a local one would be forking a shared abstraction. If the one-parser version turned out worse, the instruction was to stop and write an amendment moving `operator|` earlier. No amendment was written, so it didn't.

The anchored region is the wrapper. The three-way decision that actually replaced the `while (true)` is outside it, so the architecture document shows the call and describes the loop.

`intertoken_step` also introduces a diagnostic string that nothing can print. It returns `parse_error{cur.position(), "no intertoken space"}` from two places, and `skip_many` discards every failure it is handed. The message exists so the step parser can fail; there's no path from it to a user.

The readtable threads through as the context, which is the evidence the brief wanted. The public `read<>` has a table and no reader context in hand. So instead of wrapping one, `readtable const` models `parse_context` as it stands, and needed nothing added to either concept to do it. Two context types in one reader, each named where it is used. Three, counting the `no_context` the block-comment delimiters run against. That one needs no table at all.


# The comment skipper is not a combinator

`skip_block_comment` was the other loop, and it did not become a `skip_many`. It became `skip_block_comment_depth`, an ordinary recursive function taking the still-open count as a parameter. Skip body characters with `advance_while`, try `#|`, try `|#`, step one character and recurse.

The two delimiters are the interesting part. They are `bind(char_p('#'), [](char) { return char_p('|'); })` and its mirror. The comment above them says they go through the Monad instance the step before this one registered, and not through a sequencing helper written on the spot. But a `bind` whose continuation ignores its argument and returns a fixed parser is the derived `then`, which `monad<Impl>` supplies and `parser_monad_map` inherits. GCC 16 accepts `parser_monad_map{}.then(char_p('#'), char_p('|'))` against this step's own headers, and it agrees with the hand-written `bind` on both a match and a mismatch. What it doesn't have is a customization-point object. `monad.hpp` declares one for `bind` and one for `join`, and none for `then` or `apply`, so reaching `then` means naming the instance map. The spelling that landed is the one that is reachable.

Which means the instance is still one caller away from the operation its own base class warned it about. The class comment says a deferred instance ("a parser that stores what to do next") has to supply `join`, `then` and `apply` itself instead of inheriting them. Two steps in, nothing calls any of the three on a parser.

A `while` loop has no depth limit. A recursion evaluated at compile time does, and each `#` or `|` in a block comment is now a frame on the constant evaluator's stack where it used to be an iteration. Ordinary comment text costs one frame, because `advance_while` still runs the interior. Nothing in the test suite comes near a limit, and nothing here says what a limit would be. It is a different resource than the one the loop used, and the code no longer says which.

The architecture note claims the guard "costs nothing at either of this step's two call sites," then names one of them in the parenthesis that follows. There is one. Outside its own definition and its own tests, `skip_many` appears in this tag's tree as one `using` declaration, one mention in a doc comment, and one call.


# A behaviour nothing was checking

The brief's stated risk was not the hang. It was that the obvious combinator is a better parser. An unterminated `#|` currently runs to end of input and lets the caller report. A recursive matcher fails at the comment, points at it, and says something more useful. The scoping note ratified before any of this started forbids exactly that improvement: this series changes no observed behaviour, and a better message is still a changed message. The note had flagged this exact trap already, while the steps were being written. One of two it listed.

Handling it was easy. The base case returns the cursor unconditionally at end of input regardless of depth, with a comment saying why, so the next reader doesn't fix it. Then the step went looking for the test that pinned the behaviour it was preserving.

`read.test.cpp` had `"#| block |# 42"` and `"#| nested #| deeper |# still |# 42"`, both terminated, and `" ; only a comment"` failing with `unexpected end of input`. Nothing for an unterminated `#|`, flat or nested. The one behaviour the whole step was organized around not breaking had no test at all, so a regression in it would have come back green.

Two cases went in, `fails_with("#| unterminated", "unexpected end of input")` and a nested variant, appended to the existing chain with no existing expectation touched. They went into `src/smd/cl/reader/read.test.cpp`, which the step's own spot check rules out:

```sh
git diff --name-only cl-parser-combinators..HEAD | grep 'cl/.*\.test\.cpp$'
```

"Must return nothing." It returns that file. The handoff records the authorization mid-step and the metrics row records the touch, so the trail is there. The check exists so that the witnesses can't be adjusted to fit the conversion. And the witness this conversion most needed did not exist, so the step broke the check to write one.


# Two of five

The brief opens with "`src/smd/cl/reader/detail/skip.hpp` holds the reader's two remaining raw loops." The commit message says "skip.hpp held the reader's last two while loops." The architecture note says "held the reader's last two raw loops."

At the commit before this one, `src/smd/cl/reader/` held five, in four files. `skip.hpp` had two. The others:

-   "Substrate generic algorithm: the reader's element unfold." (`detail/forms.hpp`)
-   "Substrate generic algorithm: the string accumulator; the escape state makes each step depend on the previous character." (`detail/text.hpp`)
-   "Substrate generic algorithm: the reader's token accumulator&hellip; this is the token layer's one primitive loop, built directly on cursor." (`token.hpp`)

All three are still there. All three carry the comment this step's argument is against, in files this step's argument covers, and one of them calls itself the layer's **one** primitive loop. Two of the three belong to later steps in the plan. The third is `scan_token`, which is out of scope for every step in this stretch by standing rule, so its comment is going to outlive the argument either way.

None of this makes the conversion wrong. It makes the sentence justifying it a count of the loops the step was assigned, where it reads as a count of the loops in the reader. Different numbers, identical wording.


# The numbers

266 seconds, two attempts, green on both legs of the matrix, verification 89 seconds against a 121-kilobyte log. Nine files, 429 insertions, 38 deletions. Twelve new Catch2 cases and eight new `static_assert~s across two test files. ~repeat.test.cpp`, which is new, holds seven of the cases and five of the assertions.

The header split that opened this stretch came in at 695 seconds, and the plan calls that the floor for a step in this series. This one did a fair amount of thinking: a hang diagnosed, a primitive's contract changed, a behaviour found to be untested. Under two fifths of the floor, across two attempts. Whatever the wall clock measures across these runs, it isn't work. A floor is the one thing it can't be.

Verification is a third of it. 89 seconds, up from 53 and 55 on the two steps before, on a build that gained a header and a test file.

A divergence number was reserved for this step before it ran, for whatever deliberate departure it might need. Nothing went in.

The reader has three loops left, and a comment on each explaining that they're somewhere else.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 40 - One bind, and the Namespace It Could Not Be Declared In](phase-40-one-bind.md)

</nav>


# References
