**DRAFT &mdash; pending author revision**

<div class="abstract" id="orgf96b81e">
<p>
String literals and character literals move onto the combinator layer. The worker that moved them is the second one to try.
The first ran out of budget part way through and committed what it had. Its commit message is addressed to whoever picks it up: incomplete, do not merge, and re-run the verification from scratch rather than trusting the interrupted session's.
The brief adds a line of its own. The unfinished work is evidence, not authority.
The second worker kept the shape and rewrote the middle. The whole difference is where the closing quote gets tested.
The first attempt tested it twice, once in the repetition's step and once inside one of the two alternatives. Then it flattened every failure of the choice into <code>unterminated string</code> through a <code>has_value()</code> ladder, the one idiom the house rules name and the reader had none of left.
The landed version tests the quote once, in the step. That is what lets the second alternative honestly be any character at all. Its own end-of-input failure already carries the right message at the right position, so nothing has to translate anything.
<code>satisfy</code> wanted to be both alternatives and could not be. It reports failure at the cursor; every diagnostic a reader can see out of this file reports at the opening quote instead.
The same property is what makes ordered choice commit to an escape once it has consumed one. The architecture note has the direction backwards: the position it calls "past" the choice's starting cursor is four characters behind it.
For half of what this step converts, a green test matrix was proving nothing. Two of the four diagnostics in the file had no test at all.
The new cases are the deliverable. The one pinning the length limit had to stop using the file's own failure helper in order to check a position.
The outside oracle the brief leans on never ran. SBCL is absent, its four cases report themselves skipped, and the harness counts a skip as a pass.
The step was handed six spot checks and four of them are wrong. A grep told to expect zero returns three. Two of the commands glob for a binary four directory levels short of where it is. The filters on one of those name test cases that do not exist. And a diff check forbids the very file the brief's own setup calls the hazard the step exists to close.
Four files, 199 lines added, and a record that says one attempt.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 42 - A Repetition That Owns Nothing, and a Choice Nothing Calls ←](phase-42-repetition-and-choice.md)

</nav>


# A step with a note attached to it

The brief doesn't open the way the others do. Its setup section normally says which branch to cut and what to measure first. This one says the step has already been started once, and was cut off part way through. A branch carries one work-in-progress commit. The commit message is addressed to the reader.

It is. "INCOMPLETE. Committed to preserve work when the model budget ran out mid-step, not because the step is finished. Do not merge as-is." Then what is there. Then what is not (the verification, the spot checks, the measurements, the handoff). And then the line that matters most, which is not to believe any of it. "This needs the spot checks and full verification re-run from scratch, not trusted from the interrupted session."

The brief adds its own sentence above that, and it's the better one. "The WIP is evidence, not authority. You own this step's outcome, not that worker's judgment."

So there are two workers here and one commit. The interesting question is which parts of the unfinished work survived being read by somebody told not to defer to it. Three didn't. One of the three is the middle of the string reader.


# Any character at all

```cpp
/// Reads a string literal (the opening `"` already consumed): characters
/// verbatim, `\c` taking @c c literally, until the closing `"`.
///
/// The body is @ref smd::kit::parser::many_until over a step that owns the
/// accumulator and the capacity check — the division of labour B4's @ref
/// read_delimited established, where @c many_until supplies only "keep
/// asking until told to stop or handed a failure". What used to be an
/// escape flag threaded by hand through each turn of a @c while is now an
/// alternative inside the repeated parser: either a single escape and
/// whatever follows it taken literally, or any character at all. That is
/// the simplification this conversion buys; the loop had to remember
/// between iterations what the choice now says in one place.
///
/// The closing quote is the step's stopping condition, so it is tested
/// once, ahead of the alternatives, exactly as @ref read_delimited tests
/// for its closing delimiter. Neither alternative tests for it, which is
/// why the second one really is "any character" rather than "any character
/// that is not a quote".
template <reader_context Ctx>
[[nodiscard]] constexpr auto read_string(cursor cur, Ctx &ctx,
                                         foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    // One character, or the diagnostic the whole literal fails with when
    // input runs out. smd::kit::parser::satisfy is the primitive this
    // wants to be and cannot: satisfy reports at the cursor, and every
    // diagnostic here reports at the opening quote instead.
    auto const any_char = smd::kit::parser::parser{
        [where](cursor c, smd::kit::parser::parse_context auto &)
            -> smd::kit::parser::parse_result<char> {
            if (c.empty()) {
                return foundation::parse_error{where, "unterminated string"};
            }
            return parse_state<char>{c.peek(), c.bump()};
        }};
    // A single escape, and then whatever follows it. Failing *before* the
    // escape is consumed is what lets operator| fall through to any_char;
    // failing *after* it -- a literal ending in a bare backslash -- is a
    // committed failure that propagates instead, carrying any_char's
    // "unterminated string" rather than a diagnostic of its own, which is
    // what the hand-written loop's break did.
    auto const escaped_char =
        smd::kit::parser::parser{[any_char](cursor c, reader_context auto &rc)
                                     -> smd::kit::parser::parse_result<char> {
            if (c.empty() ||
                rc.table.syntax_of(c.peek()) != syntax_type::single_escape) {
                return foundation::parse_error{c.position(), "single escape"};
            }
            return any_char(c.bump(), rc);
        }};
    auto const string_char = escaped_char | any_char;

    datum_string contents{};
    auto const step = [&contents, string_char, where](cursor c,
                                                      reader_context auto &rc)
        -> foundation::result<parse_state<std::optional<char>>> {
        if (!c.empty() &&
            rc.table.macro_of(c.peek()) == macro_kind::double_quote) {
            return parse_state<std::optional<char>>{std::nullopt, c};
        }
        // Running out of room is one more way reading a character fails,
        // so the check chains after the character rather than standing in
        // a ladder beside it. It is `>=` before the append, so a literal
        // of exactly max_string_chars characters fits and one more does
        // not, and it reports at the opening quote rather than at the
        // character that would not fit.
        return and_then(
            string_char(c, rc),
            [&](parse_state<char> const &ch)
                -> foundation::result<parse_state<std::optional<char>>> {
                if (contents.length >= max_string_chars) {
                    return foundation::parse_error{where, "string too long"};
                }
                contents.storage[static_cast<std::size_t>(contents.length)] =
                    ch.value;
                ++contents.length;
                return parse_state<std::optional<char>>{ch.value, ch.rest};
            });
    };
    return and_then(
        smd::kit::parser::many_until(step)(cur, ctx),
        [&](parse_state<std::monostate> const &finished)
            -> foundation::result<parse_state<int>> {
            return and_then(
                add_leaf_checked(ctx, datum_atom{contents}, where),
                [&](int id) -> foundation::result<parse_state<int>> {
                    return parse_state<int>{id, finished.rest.bump()};
                });
        });
}
```

The loop this replaces carried one bit of state across iterations: whether the last character was a single escape. Both attempts get rid of it the same way. The escape becomes an alternative inside the repeated parser rather than a flag the loop remembers. Where they differ is where the literal stops.

The first attempt put the stop condition in two places. Its step returned `std::nullopt` at a closing quote, and its second alternative (named `ordinary_char`) failed with `non-quote character` when it saw one. Two copies of the rule that ends a string literal, and they have to agree. The copy inside the alternative could never fire anyway, because the step has already returned by the time the choice runs.

The landed version tests the quote once, in the step, ahead of the alternatives. That is exactly what the list reader does with its closing delimiter. The second alternative is then `any_char`, and it means it.

The other thing that went was a shape, not a rule. The first attempt ran the choice, asked the result whether it had a value, and returned `unterminated string` at the opening quote for anything else:

```cpp
auto const matched = string_char(c, rc);
if (!matched.has_value()) {
    return foundation::parse_error{where, "unterminated string"};
}
```

The house rules say that threading a `result` through a structure is a `traverse` over the result applicative, not an `if (!r.has_value()) return r;` ladder. The list reader's last one went before any of the combinator work started, and the step before this one went looking for what survived and reported that no ladder does. So grep the reader for that spelling. Eleven lines at the interrupted commit, ten at the merge. The extra line is in this file, and it's the whole of the difference.

What stands in its place is the capacity check chained onto the character read instead of standing beside it. Its own comment gives the reasoning. Running out of room is one more way reading a character can fail.

The escape alternative lost its private diagnostic too. It consumes the escape and hands the cursor to `any_char`. A literal ending in a bare backslash therefore fails as end of input, with the message the hand-written loop's `break` reached, at the position it reached it. Nothing translates anything.


# A failure reported behind the cursor

`satisfy` is the primitive that wanted to be both alternatives and couldn't. It takes a predicate and a name for what was expected, and it reports failure at the cursor it was given. Every diagnostic a reader can see out of this file reports at `where` instead, which is the opening quote, or the `#` of a character literal. So both of the one-character parsers are written out longhand. The step wrote that down as a finding about the primitive, and not about these two callers. The fix waits on a second caller: give `satisfy` the position to report at.

The same property decides the choice, and that's where the written explanation comes apart. Ordered choice falls through to its second alternative only when the first fails at exactly the cursor the operator started from:

```cpp
if (ra.error().where != start) {
    return ra;
}
```

The architecture note explains the bare-backslash case by saying the escape parser's failure "is past the position `operator|` started from". The handoff to the next step writes it as a property to preserve: fail at the start cursor to fall through, fail past it to commit. But it isn't past it. The failure is reported at `where`, the opening quote, which sits behind every cursor inside the literal (four characters behind, in the case the step added). The comparison is an inequality and not an ordering. The outcome is right; the reason given for it isn't.

There's a live consequence under that, which no one has had to meet yet. Any alternative whose diagnostic is anchored at `where` commits unconditionally, including one that consumed nothing, because `where` can never equal the cursor the choice started from. The file depends on the converse in one place. `escaped_char`'s own failure, the one that says `single escape`, is deliberately reported at `c.position()` instead. That's the only reason the choice ever reaches its second alternative, and the comment over it says so.

One small thing falls out. `single escape` is a message no input can produce: the choice that reads it always discards it, and it exists only to be compared against a cursor position. The first attempt had three such strings. This has one.


# One bind, and the cursor that rides along

```cpp
/// Reads a character literal (the `#\` already consumed): the next
/// character itself, case-sensitively, unless it begins a run of
/// constituents — then a character name, case-insensitively (ANSI 2.4.8.1).
///
/// Whether the first character stands for itself or opens a character name
/// depends on the character *after* it, so the second parser is chosen by
/// the first one's result rather than fixed in advance: a @c bind. D28
/// (docs/cl-parser-scoping.md) asks that applicative composition stay the
/// default and @c bind be spent only where the grammar is really
/// context-dependent, because @c lift2 says something stronger about a
/// parser than @c and_then does and that information should not be
/// discarded where it holds. Here it does not hold, and this is what the
/// exception looks like.
template <reader_context Ctx>
[[nodiscard]] constexpr auto read_character(cursor cur, Ctx &ctx,
                                            foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    auto const first_char = smd::kit::parser::parser{
        [where](cursor c, smd::kit::parser::parse_context auto &)
            -> smd::kit::parser::parse_result<char> {
            if (c.empty()) {
                return foundation::parse_error{where,
                                               "expected character after #\\"};
            }
            return parse_state<char>{c.peek(), c.bump()};
        }};
    // The continuation. It holds the first character, so it is the first
    // thing here that can ask whether a name follows. The outer cursor
    // rides along because a name's source span is measured from the first
    // character, which by then is behind the parser's own cursor.
    auto const name_or_itself = [cur, where](char first) {
        return smd::kit::parser::parser{
            [cur, first, where](cursor after_first, reader_context auto &rc)
                -> foundation::result<parse_state<int>> {
                auto const finish =
                    [&](char value,
                        cursor rest) -> foundation::result<parse_state<int>> {
                    return and_then(
                        add_leaf_checked(rc, datum_atom{datum_character{value}},
                                         where),
                        [&](int id) -> foundation::result<parse_state<int>> {
                            return parse_state<int>{id, rest};
                        });
                };
                bool const named =
                    rc.table.syntax_of(first) == syntax_type::constituent &&
                    !after_first.empty() &&
                    rc.table.syntax_of(after_first.peek()) ==
                        syntax_type::constituent;
                if (!named) {
                    return finish(first, after_first);
                }
                cursor const name_end =
                    advance_while(after_first, [&rc](char c) {
                        return rc.table.syntax_of(c) ==
                               syntax_type::constituent;
                    });
                auto const raw = cur.remaining().substr(
                    0, static_cast<std::size_t>(name_end.position().offset -
                                                cur.position().offset));
                if (auto const value = lookup_char_name(raw)) {
                    return finish(*value, name_end);
                }
                return foundation::parse_error{where, "unknown character name"};
            }};
    };
    return bind(first_char, name_or_itself)(cur, ctx);
}
```

The character reader is the half both workers agreed on, and a `bind` because it has to be. Whether `#\a` is the character `a` or the first letter of a name depends on what follows the `a`, so the second parser is picked by the first one's result. The project's standing preference is applicative composition, on the ground that `lift2` says something stronger about a parser than `and_then` does. Here the grammar really is context-dependent, and the exception applies.

The continuation captures the cursor from outside the parser, which reads oddly until you see why. A character name's source span is measured from the first character of the name. By the time anything can ask whether there's a name at all, the parser's own cursor is past it. So the outer cursor rides along and the span is still offset arithmetic.

The step was told to use a consumed-span-returning repetition if the substrate had one, and to report it as evidence if it didn't. That answer went into the handoff. There's no repetition here to return a span from, and `advance_while` already hands back the cursor the span is measured to.

The commit message opens by calling this file "the reader's last two hand-managed loops". The architecture section it landed says the same thing twice, in its heading and in its first sentence. The character reader was never a loop. It did two-character lookahead and then a `substr` computed from two offsets. Worth converting, and not a `while`. The first sentence has since been corrected, by the count phase 41's post ran. The heading over it was missed.

Both functions now name the reader's context concept at the signature. The handoff volunteers that the list reader still doesn't: its own parameter is a bare `class Ctx`, with the concept on its inner step closure instead. The brief's phrasing had suggested otherwise, so the handoff adds a line for the next worker saying so. Nothing asked it to.


# Half of it had no test

The file had four diagnostics when the step opened. Two were pinned: an unterminated literal, and a character name that isn't in the table. The length limit and `expected character after #\` were not. The commit message counts three gaps and not two. The third is a path and not a message: a literal ending in a bare backslash, which has to come out as end of input. The interrupted worker's own message is blunt about what that adds up to, and that paragraph is the one thing in it reading like a finding instead of an apology. For half of what this step converts, a green test matrix proved nothing.

So the new cases are the deliverable, and the second worker rewrote one of them too. The first attempt's `long_string_is_reported` read one character past the limit and checked the message. It went through `fails_with`, the helper every error expectation in the file goes through, which compares the message and nothing else. Its own comment said the position was the point, and named it: the opening quote. Nothing in it looked.

`string_capacity_boundary` doesn't use the helper. It reads a literal of exactly 128 characters and checks the string comes back, then one of 129 and checks the message, the line and the column. Column one is the opening quote. The character that wouldn't fit is at column 130.

The other witness the brief leans on never ran. The conformance suite compares what this reader prints against SBCL, and SBCL isn't installed here, so all four differential cases call `SKIP` and the harness counts a skip as a pass. A green matrix was available. The step didn't take it. The metrics note says "sbcl absent so all four differential tests skip", and the handoff tells the next worker to install SBCL first. Or else to say plainly what was checked instead. A backlog item for this was written weeks ago and nothing has been done about it. It is at least being reported by the steps it hits.


# Six spot checks

Every step ends with a short list of commands to run and answers to expect. This one lists six. Two are plain greps for diagnostic strings and do what they say. The other four don't.

`grep -cE '\bwhile\b|\bfor\b'` over the file, "expect 0". It returns three. All three are ordinary English words inside doc comments. One `while`, in a sentence about what the old loop remembered. Two `for=s, one about the closing delimiter and one about what a character literal's first character stands for. The first attempt's architecture prose asserted the zero anyway. The landed prose says instead that every =while` and `for` still matching a grep over the file is a word in a doc comment. True, and the one of the four the step actually fixed.

Next, the diff check. List the files this step changed, filter for test files under the front end, and it "must return nothing". It returns `src/smd/cl/reader/read.test.cpp` (the file the same brief's setup section calls the hazard this step exists to close). Two halves of one document, disagreeing. The additions won. They're additive, and no existing expectation moved, which is the standing rule the brief also states.

Then the two that run a binary. `./.build/*/*/cl_reader_test` matches two directory levels below the build root, and the binary sits six down, under the configuration name, under the source path. The glob resolves to nothing, `2>/dev/null` swallows the complaint, and `tail` exits 0, so the check is silently empty. The conformance line under it has the same glob and the same silence.

The filters make it worse. Of the three passed to the reader test &mdash; `"*String*"`, `"*Character*"` and `"*Errors*"` &mdash; only the last matches anything. The file's nine cases are named for what they read or what they check. The reader functions don't come into it, and strings and characters live inside the one called `Atoms`.

The handoff writes up the paths and the case names for the next worker. Somebody else's spot checks get to be right.


# The numbers

Four files, 199 insertions, 54 deletions, one attempt recorded, green on both legs of the matrix. The reader's test file took 29 of the added lines and gave up one: a statement terminator that became a conjunction so the chain could grow.

The test count didn't move. 378 entries per leg before and after, where the brief predicted 380 on the strength of the new cases. Nothing new is a `TEST_CASE`. Two clauses went into a `constexpr` predicate an existing case already calls, and the new predicate got one `CHECK` inside an existing case and one `static_assert` beside the others. The harness counts cases. The handoff turns that into a rule for the next worker.

`text.hpp` went from 134 lines to 230, and its comment lines from 13 to 61. The string reader used to be 29 lines of code with four of comment. It's now 54 carrying 34, half a doc block and half inline. Whatever the conversion did, it didn't make the file shorter. The commit message claims only the narrow thing, which is that escape handling is the part that actually gets simpler, and that much holds. One bit of cross-iteration state is gone, and the two alternatives say in one place what the loop had to remember between turns.

Verification ran 43 seconds against a 136-kilobyte log. The wall clock reads 3420 seconds (a little under an hour), which is at least a number that could be true. Earlier steps in this run have reported 266 seconds and fifty-one hours. Attempts is recorded as 1. The interrupted commit is folded into this one instead of carried as its own history, so the record says one attempt for a step that had two.

Two process complaints from earlier posts didn't recur. No follow-up commit sits between this merge and the next step's first commit. The step before this one needed one, to carry a formatting rewrite the hooks had already made and no one had staged. That was the fourth time in this run. The interrupted worker's own message had pointed at the open backlog item for it. The root checklist line is ticked inside the step's own commit, too.

A divergence number was reserved for this step, as for each of the four before it. None of the five was used. So nothing has been recorded as a deliberate departure from the standard, from the plan, or from the project's own rules in five steps of moving the reader onto a new layer. Or nothing has been written down.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 42 - A Repetition That Owns Nothing, and a Choice Nothing Calls](phase-42-repetition-and-choice.md)

</nav>


# References
