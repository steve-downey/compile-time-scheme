**DRAFT &mdash; pending author revision**

<div class="abstract" id="orgef16313">
<p>
The reader's delimited-list reader loses its last raw loop, and the combinator that takes it over holds no container, no capacity and no message.
The brief asked for a <code>many_bounded&lt;Capacity&gt;</code> carrying a message and a failure position. What landed is <code>many_until</code>, which supplies only control flow. The design turns on one position: "too many elements", at the post-skip position of the element that would not fit. It lands right only if the closure already holding that cursor also owns the capacity check.
Hoisting collection into the substrate moves it.
<code>read_delimited</code>'s step closure therefore keeps the accumulator, the check and the message, and hands back a <code>std::optional</code> whose payload nothing reads: the repetition asks only whether it is engaged.
Ordered choice lands too, with law tests and no caller.
The retired iteration exercised <code>operator|</code> ten ways and not one of them was the rule the operator is actually for: a left alternative that consumes and then fails wins outright.
That rule is now a <code>static_assert</code>, and so is the one that says a double failure reports the second alternative's error rather than a synthesized "expected one of &hellip;".
The <code>optional</code> beside them sits outside the anchor, and cites the wrong file of the frozen tag it differs from; the function it names is in the file next door, under a comment that draws a distinction its own body never drew.
No reader test looks at the overflow position. <code>list_capacity_error</code> checks the message, and the position is pinned in the substrate's own tests against a step closure written to look like <code>read_delimited</code>.
The brief's closing instruction was to grep the reader for one shape and report what survives. The architecture note says none. The command returns ten lines.
Seven files, one attempt, and a wall clock reading fifty-one hours that its own note says measures nothing.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 41 - Skipping Space, and a Law That Would Not Finish ←](phase-41-skipping-space.md)

</nav>


# A combinator named for what it doesn't hold

The brief names what it wants. A repetition that "must **diagnose at capacity**, not truncate", and then a shape for it: "something like `many_bounded<Capacity>` that carries a `char const*` message and a failure position, or a repetition parameterised on what to do at capacity."

What landed carries none of those.

```cpp
/// Runs @p step repeatedly, threading @p ctx, until it yields
/// @c std::nullopt -- which ends the repetition successfully at that
/// step's own rest cursor -- or until it fails, whose error this
/// repetition propagates unchanged. A @c std::optional value @p step
/// yields is otherwise discarded: this repetition supplies control flow
/// only, not accumulation, the same division of labour @ref skip_many
/// has with its own inner parser, which does not know what its caller
/// does with a successful match either.
///
/// This is the diagnosing collecting repetition the retired iteration's
/// `many<Capacity>` (`iteration/smdscheme-final`,
/// `src/smd/smdscheme/parser/alt.hpp`) is not: that combinator loops
/// until its inner parser fails and then always succeeds, so both
/// "collected Capacity elements and there was more" and "ran out of
/// input before the caller's own stopping condition was met" are silent
/// truncation, never failure -- a caller relying on either to be
/// diagnosed never finds out from that combinator, only from something
/// else afterward, at the wrong position, with the wrong message. Here,
/// @p step alone decides both what "stop" means (yielding
/// @c std::nullopt) and what a capacity overflow means (failing, with
/// whatever message and position it chooses, typically by checking its
/// own accumulator before accepting a value) -- this repetition does not
/// invent either policy, it only replaces the hand-written @c while(true)
/// loop that would otherwise carry it. It is named @c many_until, not
/// @c many_bounded or any other name suggesting it owns a capacity
/// itself, so that its caller-supplied stopping condition is not mistaken
/// for a truncating bound.
///
/// Guards a zero-consumption "keep going" value the same way @ref
/// skip_many guards a zero-consumption success -- by stopping there
/// rather than looping forever -- even though every known caller's own
/// @p step always advances on a "keep going" value (reading one datum, or
/// consuming a closing delimiter, always consumes at least one
/// character): a future @p step that does not hold that property should
/// not be able to hang a constexpr evaluation to find out.
///
/// @tparam P A parser over @c std::optional<T> for some @c T.
template <class P>
[[nodiscard]] constexpr auto many_until(P step) {
    return parser{[step = std::move(step)](cursor cur, parse_context auto &ctx)
                      -> parse_result<std::monostate> {
        // Substrate generic algorithm: the repetition loop this
        // combinator is built from, the collecting counterpart of
        // skip_many's discarding one above.
        while (true) {
            auto const r = step(cur, ctx);
            if (!r.has_value()) {
                return parse_result<std::monostate>{r.error()};
            }
            if (!r.value().value.has_value() || r.value().rest == cur) {
                return parse_state<std::monostate>{{}, r.value().rest};
            }
            cur = r.value().rest;
        }
    }};
}
```

`many_until` is control flow and nothing else. It asks its step for another turn until the step says stop or hands back a failure, and everything else belongs to the caller. The argument for that is one position. `read_delimited` diagnoses `"too many elements"` at the position of the element that would not fit. That position is **after** the intertoken skip, since skipping is the first thing each turn does. The stop condition, a closing delimiter, has to be tried ahead of "read one more element", on the same already-skipped cursor. Both land right when the closure holding the skipped cursor also holds the capacity check. Move collection up into the substrate and the skip has to go somewhere. Every somewhere is a different position.

The trap is real, and it has a name and a body in the frozen tree. The retired iteration's `many<Capacity>` is at `iteration/smdscheme-final`, in `src/smd/smdscheme/parser/alt.hpp`, and its loop is `while (result.size() < Capacity)` followed unconditionally by a success. Its doc comment is honest about it: "collecting at most `Capacity` results", "Always succeeds (zero repetitions is valid)". The honesty is the whole problem. A caller that wants an over-long list diagnosed gets neither the failure nor the place, and finds out later, somewhere else, with a different message.

The substrate now holds two repetitions that answer the same question opposite ways. The one the previous step landed never fails, because an unterminated `#|` must keep consuming and let the caller report end of input. This one fails, because an unclosed list must be reported where it goes wrong. Neither will absorb the other, and the commit message says as much. The doc comment argues something narrower, at thirty-seven lines over nineteen lines of code; eighteen of those lines are about the combinator that wasn't written.

The step's own spot check still expects that one. It greps for `too many elements` in `forms.hpp` and `repeat.hpp` together. The string is only in `forms.hpp`; `repeat.hpp` never sees it, which is the design. Grep over two files exits 0 on a hit in either, so the check passes while asking for the thing the step declined to build.


# The list reader, and a value nothing reads

```cpp
/// Reads `)`-delimited elements (the opening delimiter already consumed)
/// into a branch of @p kind: lists and vectors.
///
/// One iteration of the element unfold, reused verbatim from before this
/// step (93e4bc7 already turned it into the @c and_then chain below): read
/// one datum, and record it unless @c children is already full, in which
/// case that is one more way reading an element fails. What this step
/// changes is what drives that chain: a hand-written unbounded repeat
/// construct used to call it, carrying a "substrate generic algorithm"
/// comment that was never true of a loop living in the reader rather than
/// the substrate (@ref smd::kit::parser -- @c src/smd/kit/parser/repeat.hpp
/// -- is the substrate; this file is not). @ref smd::kit::parser::many_until
/// now drives it: this function's own step closure decides, each turn,
/// whether to stop (the closing delimiter, signalled by
/// @c std::nullopt), keep going (one element, folded into @c children by
/// the unchanged @c and_then chain), or fail (running out of input, or
/// the chain above failing) -- and @c many_until only supplies the "keep
/// asking until told to stop or handed a failure" control flow around
/// that, the same relationship it has with @ref skip_many's own inner
/// parser.
///
/// The three diagnostics are unchanged, at the same positions: `"expected
/// ')'"` when input runs out before the closing delimiter (checked here,
/// ahead of @c read_node, so it is never read_node's own "unexpected end
/// of input" -- a different message at the same position, and so a
/// behaviour change D31 forbids); `"too many elements"`, at the position
/// of the element that would not fit, from the unchanged capacity check;
/// and `"datum tree full"` from @ref add_branch_checked, unchanged.
template <class Ctx>
[[nodiscard]] constexpr auto read_delimited(cursor cur, Ctx &ctx,
                                            datum_branch kind,
                                            foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    typename Ctx::child_list children;
    auto const step = [&children](cursor c, reader_context auto &rc)
        -> foundation::result<parse_state<std::optional<int>>> {
        c = skip_intertoken_space(c, rc.table);
        if (c.empty()) {
            return foundation::parse_error{c.position(), "expected ')'"};
        }
        if (rc.table.macro_of(c.peek()) == macro_kind::right_paren) {
            return parse_state<std::optional<int>>{std::nullopt, c.bump()};
        }
        // Read one element and record it: two steps, the second taking the
        // first's output, which is a bind. Running out of room is one more
        // way reading an element fails, so the check belongs inside the
        // chain rather than in a second ladder after it.
        return and_then(
            read_node(c, rc),
            [&](parse_state<int> const &element)
                -> foundation::result<parse_state<std::optional<int>>> {
                if (children.size() >= children.capacity()) {
                    return foundation::parse_error{c.position(),
                                                   "too many elements"};
                }
                children.push_back(element.value);
                return parse_state<std::optional<int>>{element.value,
                                                       element.rest};
            });
    };
    return and_then(
        smd::kit::parser::many_until(step)(cur, ctx),
        [&](parse_state<std::monostate> const &finished)
            -> foundation::result<parse_state<int>> {
            return and_then(
                add_branch_checked(ctx, kind, children, where),
                [&](int id) -> foundation::result<parse_state<int>> {
                    return parse_state<int>{id, finished.rest};
                });
        });
}
```

What went is the `while (true)` and the comment over it claiming the substrate's raw-loop exemption from a file that isn't the substrate. What stayed is everything else. The `and_then` chain an earlier commit had already made of the element read, the `static_vector` of children, the capacity check, and all three diagnostics at all three positions.

The step closure signals "stop" by yielding `std::nullopt`. It signals "keep going" by yielding the child id it has just pushed into `children`. And nothing reads that id. `many_until` tests `!r.value().value.has_value()` and goes no further into the optional; the value is discarded, as the doc comment says outright. So the step's return type is `std::optional<int>` where a bool would do, and the `int` is there for a caller that might want it later. Today it's a flag with a passenger.

One thing the brief asked for didn't quite land where it was aimed. It said to constrain the function with `reader_context`, following the pattern the step that built the layer established. That pattern is visible one file over: `read_radix_number` is declared `template <reader_context Ctx>`. `read_delimited` is still `template <class Ctx>`. The concept went onto the step closure's parameter instead, where it does reject a wrong context, deeper in, and out of sight of the signature.

The handoff to the next step counts it differently. It says four functions now constrain with the concept, and names them: `read_radix_number`, `read_delimited`, and "the two skippers indirectly via `readtable const`". `read_radix_number` does. The skippers name no concept at all: `skip_block_comment` takes a `cursor`, `skip_intertoken_space` a `cursor` and a `readtable const&`. And `readtable const` is the one context that **doesn't** satisfy `reader_context`, which was the previous step's finding, recorded in the handoff this step read. One of four.


# Ordered choice, tested and unused

```cpp
/// Ordered choice: runs @p pa; if it fails without consuming any input,
/// runs @p pb from the same starting cursor; otherwise @p pa's result --
/// success or failure -- stands.
///
/// "Without consuming" is judged by comparing @p pa's failure position to
/// the cursor @c operator| itself started from, not by any property @p pa
/// reports about itself: a parser that partially matches and then fails
/// deeper in has already committed, and that failure propagates rather
/// than being discarded in favour of @p pb. This is the retired
/// iteration's own rule (`iteration/smdscheme-final`,
/// `src/smd/smdscheme/parser/parser.hpp`), carried forward unchanged
/// because B7's readtable dispatch is built on it continuing to hold.
///
/// On a double failure -- @p pa fails without consuming and @p pb also
/// fails -- @p pb's error, whatever it is, is what @c operator| returns.
/// This is deliberately not a synthesized "expected one of ..." message:
/// hand-rolled parsers in this codebase produce targeted diagnostics
/// (`"expected ')'"`), and a chain of alternatives that manufactures its
/// own combined message would be a *better* parser than the one it
/// replaces, which decision D31 forbids. A caller that wants a specific
/// message to survive a double failure puts that alternative last.
///
/// @tparam PA First alternative.
/// @tparam PB Second alternative; its type must produce the same
///         @c parse_result as @p PA.
template <class PA, class PB>
[[nodiscard]] constexpr auto operator|(parser<PA> pa, parser<PB> pb) {
    return parser{[pa = std::move(pa),
                   pb = std::move(pb)](cursor cur, parse_context auto &ctx) {
        auto const start = cur.position();
        auto const ra = pa(cur, ctx);
        if (ra.has_value()) {
            return ra;
        }
        if (ra.error().where != start) {
            return ra;
        }
        return pb(cur, ctx);
    }};
}

/// Named alias for @ref operator|, for a call-site spelling that reads as
/// a combinator rather than an operator.
///
/// @tparam PA First alternative.
/// @tparam PB Second alternative.
template <class PA, class PB>
[[nodiscard]] constexpr auto alt(parser<PA> pa, parser<PB> pb) {
    return pa | pb;
}
```

`operator|` runs the first alternative. If it fails at the exact position the operator itself started from, the second runs from that same cursor. Otherwise the first alternative's result stands, failures included. A parser that matched half a token and then gave up has committed, and its error propagates instead of being thrown away for a second try.

The rule is carried forward from the retired iteration unchanged, and the code is nearly line for line what's at that tag in `src/smd/smdscheme/parser/parser.hpp`. One thing did widen. The old one compared `ra.error().where.offset` against the starting offset; this one compares the whole `source_pos`, offset and line and column, through its defaulted `==`. No code in the tree can tell the difference, since line and column are derived from the offset of the same input.

What's new is that the rule is checked. The retired tree exercises the operator ten ways: four `static_assert=s written against =operator|` itself, three more through `alt`, and three Catch2 cases besides. Between them they cover three situations. The first branch wins on a match, the second branch is taken when the first fails without consuming, and both failing fails. Not one of them is a left alternative that consumes and then fails. That is the case the operator exists to get right &mdash; stated in its doc comment, implemented in its body, never tested. Here it is `x_then_fail | is_a` against `"xa"`, and the answer has to be `"deep failure"` even though `'a'` is sitting right there for the taking.

The other law is about the message. On a double failure the second alternative's error survives, whatever it is, and there is no synthesized "expected one of &hellip;". The test doesn't check that the chain's error merely looks right; it checks `r.error() == is_b(cursor{"z"}, ctx).error()`, the second alternative's own error, unretouched. The reason is the constraint the whole series runs under: it changes no observed behaviour, and a chain that manufactures a combined diagnostic is a **better** parser than the hand-rolled one it replaces. Better is still different. A caller who wants a particular message to survive puts that alternative last.

And nothing calls any of it. That is how the plan scheduled it (the first consumer is the readtable dispatch, several steps out), and the step confirmed the scheduling rather than fighting it. There was a place it might have gone. The brief's own description of the converted loop reads like a use of the operator: repeat "skip intertoken space, then either the closing delimiter &hellip; or one `read_node`". Either/or is what `|` is for. Folding it in means one of two things. Duplicate the intertoken skip on both sides of the choice, and `"expected ')'"` moves at end of input. Hoist the skip out of both, and the overflow position moves, for the same reason the repetition owns no container. Both positions are pinned. So the step wrote a plain `if` and said why in the doc.

Whether that was right isn't settled by anything here. The project keeps a divergence record, written weeks before this plan, about a combinator layer with no client in this repository at all. Its argument is that extracting for its own sake produces a module with no callers. The plan repeats the lesson elsewhere in its own words: define everything in step one, first use it in step nine, and maximize the guesses validated too late. Then it scheduled a primitive three steps ahead of its consumer.

The third function in the file is `optional`, and it sits below the closing anchor, so the architecture note transcludes `operator|` and `alt` and not this one. Its doc comment says the retired `optional` "always succeeds even when `p` fails after consuming input," and cites `iteration/smdscheme-final`, `src/smd/smdscheme/parser/parser.hpp`. The retired `optional` is in `alt.hpp`. `parser.hpp` at that tag has no `optional` in it anywhere. The `operator|` comment further up the same file cites `parser.hpp` and is right about it; the `many_until` comment in the other file cites `alt.hpp` and is right about that.

But follow the citation to the right file and there is a second thing to read. The retired `optional` is documented as "Always succeeds: yields `std::nullopt` if `p` fails without consuming input, otherwise yields the value." Two clauses, and the body makes only the first one true. It compares no positions at all. It asks `has_value()` and falls back, so it yields `std::nullopt` on every failure, consuming or not. The comment drew a distinction its code didn't. This one draws it, and gives up always succeeding to do so.


# The position the argument rests on

Everything in the first section comes down to one place in one error. `"too many elements"` must land at the post-intertoken-skip position of the element that would not fit, and the repetition must not own the container, because owning it moves that position.

`read.test.cpp`'s `list_capacity_error` reads `"(1 2 3)"` into a tree whose child list holds two, and checks that the parse failed and that the message is `"too many elements"`. The message, and nothing else. `error_positions_track_lines` does check a position, for a different input entirely: a `)` on the second line at column three.

The position does get checked, however, in the substrate's own new tests: `ManyUntilTest - DiagnosesAtCapacityPlusOneRatherThanTruncating` asserts `r.error().where.offset == 2` after four ~'a'~s into a capacity of two. The step it runs is a closure written in the test file to have `read_delimited`'s shape &mdash; a `static_vector` captured by reference, a size check before the push, its own message and position. It's a faithful model, and it is not `read_delimited`.

The architecture note names three test functions as the acceptance witness, "pinning all three by name and, where the test checks it, by position." Two of the three diagnostics are in there. `"datum tree full"` isn't: it's pinned by `capacity_errors_not_asserts`, a fourth function the sentence doesn't mention. And the hedge in the middle is doing more work than it looks like. The one position this step's central design decision was arranged to protect is the one no reader test looks at.


# A grep that returns ten lines

The brief ends with an instruction, and it's a good one. Run `grep -rn 'has_value()) {' src/smd/cl/reader/`, and say in the handoff whether any remain and in which function, so that the step owning each one knows it's there.

The architecture note answers: the grep "after this step's conversion finds none remaining in the reader."

It returns ten lines. Two in `src/smd/cl/reader/detail/skip.hpp`, eight in `read.test.cpp`.

The substance underneath is fine. Both `skip.hpp` hits are the same shape: `if (auto const open = block_comment_open(cur, ctx); open.has_value()) {`, and its `close` twin three lines down. An init-statement asking a positive question and taking a branch, not the short-circuit-and-return ladder the rules forbid. The eight in the test file are tests deciding whether a parse succeeded before reading out of it. No error ladder survives in the reader, which is what the sentence meant. What it reports is the output of a command, and the output of that command is ten lines.

The handoff doesn't mention any of them. The instruction was to list what survives and where. The answer was "none", so there was no list to write, and the next step is told about the choice combinators, the zero-consumption guard and the shape of the accumulator instead. All useful. None of it the thing that was asked for.


# The numbers

Seven files, 536 insertions, 32 deletions, green on both legs of the matrix, one attempt. Sixteen new Catch2 cases and five new `static_assert=s across two test files; =choice.test.cpp` is new and holds eleven of the cases and four of the assertions. `forms.hpp` went from 94 lines to 119, and twenty-six of the twenty-eight comment lines over `read_delimited` are new.

Verification ran 25 seconds against a 127-kilobyte log, where the step before it took 89 seconds against 121 kilobytes on a smaller build. An incremental rebuild is the only way that direction makes sense, although the record doesn't say.

The wall clock says 186622 seconds. Fifty-one hours and change. Its own note in the metrics row says the measurement spans several apparent sandbox clock-date rollovers mid-session, and is not a trustworthy work-duration signal. That is as much as can be said for it. Last time the number was 266 seconds against a declared floor of 695, and the honest reading then was that the wall clock isn't measuring work. This one isn't measuring anything.

A commit was needed after the merge: four lines of `choice.hpp` reflowed, formatting only, verified by hashing the file with all whitespace stripped before and after. That's the fourth time in this run that a step merged with a rewrite the linting hooks had already performed and no one had staged. The briefing warned this worker about it, as it warned the two before, which is by now the evidence that a warning isn't the fix.

A divergence number was reserved for this step before it ran. Nothing went in, for the second step running.

Two raw loops left in the reader: the string accumulator in `text.hpp` and the token accumulator in `token.hpp`. Neither file was this step's to open. Both still carry the comment.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 41 - Skipping Space, and a Law That Would Not Finish](phase-41-skipping-space.md)

</nav>


# References
