**DRAFT &mdash; pending author revision**

<div class="abstract" id="org8e359c5">
<p>
What is left of the reader is its spine: two switches on the readtable, one on the macro character and one on whatever follows a <code>#</code>.
The brief expects the readtable to be the obstacle and explains why it is not. Because a datum is an arena index, every arm of both switches already returns the same type. So a table selecting among parsers is a table selecting among same-typed parsers, which is the shape the design asked for long before there was a layer to ask it of.
The switches stay switches. A table selects one arm and commits; ordered choice searches, and a search changes which diagnostic surfaces when an arm fails.
Then the build stopped.
Not a failing test. A compiler limit: <code>'constexpr' evaluation depth exceeds maximum of 512</code>.
The series has named that flag twice before, both times about the evaluator, and the project's C++ rules say evaluation does not recurse in C++ at all. The reader is not evaluation and never was, so every reified parser between the node reader and its own recursive call is a frame.
The step bisected rather than estimated. 424 frames before it, and a reported 570 with the dispatch converted the obvious way, which is a parser that holds a callable and forwards to it through its own <code>operator()</code>. Reproducing that middle number gives 562, and the difference does not matter, because the cap is 512.
Changing the parser type to <b>be</b> its callable (the function object as a private base, its <code>operator()</code> re-exported) brings the same case under 380.
That removes a frame from every parser invocation in the tree, including all the ones the five steps before this one had already placed.
Run the same bisection back across those five and a more interesting number appears. The case needed 167 frames when the phase opened, and still needed 167 after five of the seven steps that have landed.
The whole rise came from one step, the one immediately before this, which reported twenty minutes and a green matrix and measured none of it.
So the architecture note's closing claim is wrong in the direction that matters. 363 is below where this step found the number. It is nowhere near below where the series started.
The test that caught it was written for the printer, eight phases earlier, to overflow a character limit. That limit is also 512, and has nothing to do with this one.
The primitive predicted to need extending did not. The digit run of <code>#nnR</code> cannot fail. Wrapping it in the optional combinator turns a cursor-positioned failure into the repetition's stop signal, and no diagnostic here ever comes from it.
Ordered choice gets a caller in the reader, in the comma arm, where <code>,@</code> against <code>,</code> is a lookahead and not a table selection. It is the second such caller; the string reader was the first.
The argument-order guard survives the reshape, and the step checked it instead of assuming it. The error line comes back byte for byte identical against the pre-change header. What changed is which candidate the notes name.
Eight files, 315 insertions, two attempts, fifty-two minutes. And a doc comment that says four where the switch it documents says five.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 44 - A Record of Declines, and a Test Made to Fail ←](phase-44-a-record-of-declines.md)

</nav>


# The obstacle that wasn't

Six steps have been through the reader, five of them converting it a function at a time. What they left is the spine. `read_node` skips intertoken space and switches on `macro_of`. `read_sharpsign` reads an optional numeric argument and switches on `sharp_of`. Between them they are every decision the reader makes about what a character means.

The brief spends its "why" section on an obstacle and then removes it. A readtable dispatch looks like the hard case for combinators. The retired Scheme layer's alternatives were heterogeneous in their result type and could not be combined without extra apparatus. The reader's are not. A datum is an arena index, so every arm of both switches returns `result<parse_state<int>>` already, and same-typed alternatives are the easy case. The table is therefore **already** selecting among same-typed parsers, and nothing had to be built for that to be true.

Which is the whole of what the decision record asked for when it said a later `set-macro-character` should be a table lookup and not a redesign. It adds a row. It does not touch the dispatch.

```cpp
/// Reads one datum node: skips intertoken space, then dispatches on the
/// current character through the readtable (decision D19: this lookup —
/// not character tests — is the reader's spine).
///
/// The reader's other dispatch, and the same shape as @ref read_sharpsign:
/// a selection by table lookup, committed to, with each arm naming a
/// parser. Every arm has the same result type because a datum is an arena
/// index, so what the readtable selects among is already a set of
/// same-typed parsers -- which is what a later @c set-macro-character
/// needs and is the whole of D19's ask.
///
/// It is not @c operator|, and the distinction is the point. A chain of
/// alternatives searches, and on a double failure reports the last
/// alternative's error; a table selects one arm and lets that arm's
/// diagnostic stand. Those differ exactly when an arm fails, which is
/// every diagnostic below, so D31 settles it.
template <reader_context Ctx>
[[nodiscard]] constexpr auto read_node(cursor cur, Ctx &ctx)
    -> foundation::result<parse_state<int>> {
    cur = skip_intertoken_space(cur, ctx.table);
    if (cur.empty()) {
        return foundation::parse_error{cur.position(),
                                       "unexpected end of input"};
    }
    auto const where = cur.position();
    // The quote family and the two bracketing forms record their branch at
    // @p where -- the marker's own position, one character behind the
    // cursor each parser is then handed. That difference is observable in
    // exactly one place (a "datum tree full" from the branch append) and
    // is pinned by read.test.cpp's wrapped_branch_error_sits_at_the_marker.
    auto const wrapped_p = [where](datum_branch kind) {
        return smd::kit::parser::parser{
            [where, kind](cursor c, reader_context auto &rc) {
                return read_wrapped(c, rc, kind, where);
            }};
    };
    auto const delimited_p = [where](datum_branch kind) {
        return smd::kit::parser::parser{
            [where, kind](cursor c, reader_context auto &rc) {
                return read_delimited(c, rc, kind, where);
            }};
    };
    auto const string_p =
        smd::kit::parser::parser{[where](cursor c, reader_context auto &rc) {
            return read_string(c, rc, where);
        }};
    auto const token_datum_p =
        smd::kit::parser::parser{[](cursor c, reader_context auto &rc) {
            return read_token_datum(c, rc);
        }};
    auto const sharpsign_p =
        smd::kit::parser::parser{[](cursor c, reader_context auto &rc) {
            return read_sharpsign(c, rc);
        }};
    switch (ctx.table.macro_of(cur.peek())) {
    case macro_kind::none:
        return token_datum_p(cur, ctx);
    case macro_kind::left_paren:
        return delimited_p(datum_branch::list)(cur.bump(), ctx);
    case macro_kind::right_paren:
        return foundation::parse_error{where, "unexpected ')'"};
    case macro_kind::single_quote:
        return wrapped_p(datum_branch::quote)(cur.bump(), ctx);
    case macro_kind::backquote:
        return wrapped_p(datum_branch::backquote)(cur.bump(), ctx);
    case macro_kind::comma: {
        // `,@` against `,`, and the one place in this function where
        // operator| is the right tool: this is a lookahead between two
        // spellings of one macro character, not a readtable selection, so
        // there is no table entry to consult and nothing to commit to
        // until the next character is read. The splice alternative fails
        // at the cursor operator| started from whenever the `@` is absent,
        // which is what lets the plain unquote stand; once the `@` is
        // consumed the alternative has committed, and everything that can
        // fail after it reports at where or beyond, never back at the
        // starting cursor, so nothing real can fall through by accident.
        auto const splice_p =
            bind(smd::kit::parser::char_p('@'), [wrapped_p](char) {
                return wrapped_p(datum_branch::unquote_splice);
            });
        return (splice_p | wrapped_p(datum_branch::unquote))(cur.bump(), ctx);
    }
    case macro_kind::double_quote:
        return string_p(cur.bump(), ctx);
    case macro_kind::semicolon: // consumed as intertoken space
        break;
    case macro_kind::sharpsign:
        return sharpsign_p(cur, ctx);
    }
    return foundation::parse_error{where, "expected datum"};
}
```


# Selecting is not searching

The switches stay switches, and the reason is the one this phase keeps returning to. `operator|` tries its alternatives in order and, on a double failure, reports the last one's error. A readtable looks one arm up and commits to it, so that arm's own diagnostic stands. The two differ exactly when an arm fails, which covers every diagnostic in either function, and the scoping note governing this phase forbids changing which diagnostic surfaces or where.

So the conversion is narrower than "replace the switch". Each arm names a parser instead of open-coding a call. Three of the five are built closed over `where`, because they report at a position handed to them and not at the cursor they run from. The other two, the token reader and the sharpsign reader, capture nothing at all. They are lifts and nothing more.

The one arm that really is a choice is the comma. `,@` against `,` is a lookahead between two spellings of a single macro character. There is no table entry to consult, and nothing to commit to until the next character has been read. The splice alternative is `char_p('@')` bound to the wrapped reader, and the plain unquote sits behind it.

What makes that safe is the rule ordered choice was given three steps ago. It falls through only when the first alternative fails **at the cursor it started from**; a failure anywhere else is a commitment and propagates. The splice alternative fails at the starting cursor exactly when the `@` is absent. Once the `@` is consumed, everything that can fail afterwards reports either at the comma, one character behind the start, or somewhere further along. Never at the start itself. The step verified that, and wrote the reasoning into the comment above the arm.

Ordered choice landed three steps ago with law tests and no caller at all. The string reader gave it one the step after, spelling its escape handling `escaped_char | any_char`, and that is where the fall-through rule got worked out. So the comma arm is the second caller in the reader, and the first inside a dispatch. The third file is the umbrella, and it is the largest single piece of the diff after the sharpsign reader. `read_datum` reads a node and then sets it as the tree's root. `read` reads a datum and then requires the input to be over. Two steps each, the second taking the first's output, and both were written out by hand with `and_then`. Both are `bind` over the parser layer now, which is what makes "no reader function spells a bind by hand" true rather than nearly true. The last `bind` in `read` is still `result`'s own. Dropping a cursor that a caller holding the whole string has no use for is not a parse step either.


# A digit run that cannot fail

```cpp
/// Reads a sharpsign dispatch form (positioned at the `#`): the optional
/// infix numeric argument, then the sub-handler @ref readtable::sharp_of
/// selects.
///
/// The argument decides which parsers the character after it may name --
/// four of the seven arms refuse one outright, one requires it to be a
/// radix, one refuses it with a message of its own -- so the second parser
/// here is chosen by the first one's value, which is a @c bind and not a
/// @c lift2 (D28, docs/cl-parser-scoping.md).
///
/// The dispatch itself stays a @c switch. A readtable selects one branch
/// by lookup and commits to it; @ref smd::kit::parser::operator| searches,
/// and a search would change which diagnostic surfaces when the selected
/// branch fails, which D31 does not permit. What changed is that every arm
/// now names a parser instead of open-coding a call -- and because a datum
/// is an arena index, all seven of them have the same result type, so the
/// table already selects among same-typed parsers. That is the shape D19
/// asked for when it said a later @c set-macro-character should be a table
/// lookup rather than a redesign.
template <reader_context Ctx>
[[nodiscard]] constexpr auto read_sharpsign(cursor cur, Ctx &ctx)
    -> foundation::result<parse_state<int>> {
    auto const where = cur.position();

    // The optional infix numeric argument: a digit run folded into an int,
    // -1 when absent. Absence is a value rather than a failure, so
    // `optional` is what turns satisfy's own cursor-positioned failure --
    // which no caller here ever sees -- into the std::nullopt `many_until`
    // reads as "stop", and the fold rides along in `map`. The accumulator
    // is a local the way @ref read_string's is, for the same reason: this
    // parser is built, run, and discarded inside one call, so nothing it
    // captures can outlive it.
    int argument = -1;
    auto const digit_p = smd::kit::parser::satisfy(
        [](char c) { return c >= '0' && c <= '9'; }, "expected digit");
    auto const accumulate = [&argument](std::optional<char> digit) {
        if (digit.has_value()) {
            // Cap far above the largest valid radix; enough to reject.
            argument = std::min(
                (argument < 0 ? 0 : argument) * 10 + (*digit - '0'), 999);
        }
        return digit;
    };
    auto const argument_p = smd::kit::parser::map(
        smd::kit::parser::many_until(smd::kit::parser::map(
            smd::kit::parser::optional(digit_p), accumulate)),
        [&argument](std::monostate) { return argument; });

    auto const dispatch = [where](int infix) {
        return parser{[where, infix](cursor after, reader_context auto &rc)
                          -> foundation::result<parse_state<int>> {
            if (after.empty()) {
                return foundation::parse_error{
                    where, "unexpected end of input after '#'"};
            }
            // Each sub-handler as a parser value. Every one of them
            // reports at @c where -- the `#`'s own position, not the
            // cursor they are handed -- which is why they are built here,
            // closed over it, rather than named at namespace scope.
            auto const wrapped_p = [where](datum_branch kind) {
                return parser{
                    [where, kind](cursor c, reader_context auto &inner) {
                        return read_wrapped(c, inner, kind, where);
                    }};
            };
            auto const delimited_p = [where](datum_branch kind) {
                return parser{
                    [where, kind](cursor c, reader_context auto &inner) {
                        return read_delimited(c, inner, kind, where);
                    }};
            };
            auto const character_p =
                parser{[where](cursor c, reader_context auto &inner) {
                    return read_character(c, inner, where);
                }};
            auto const radix_p = [where](int radix) {
                return parser{
                    [where, radix](cursor c, reader_context auto &inner) {
                        return read_radix_number(c, inner, radix, where);
                    }};
            };
            // A precondition on four of the seven arms, producing one of
            // the five pinned diagnostics. It guards a parser rather than
            // a continuation now, but it is otherwise the check it was.
            auto const reject_argument =
                [&](auto p) -> foundation::result<parse_state<int>> {
                if (infix >= 0) {
                    return foundation::parse_error{
                        where, "unexpected numeric argument after '#'"};
                }
                return p(after.bump(), rc);
            };
            switch (rc.table.sharp_of(after.peek())) {
            case sharpsign_kind::function_quote:
                return reject_argument(wrapped_p(datum_branch::function));
            case sharpsign_kind::vector_open:
                if (infix >= 0) {
                    return foundation::parse_error{
                        where, "sized #n(...) vectors not yet supported"};
                }
                return delimited_p(datum_branch::vector)(after.bump(), rc);
            case sharpsign_kind::character_literal:
                return reject_argument(character_p);
            case sharpsign_kind::radix_binary:
                return reject_argument(radix_p(2));
            case sharpsign_kind::radix_octal:
                return reject_argument(radix_p(8));
            case sharpsign_kind::radix_hex:
                return reject_argument(radix_p(16));
            case sharpsign_kind::radix_n:
                if (infix < 2 || infix > 36) {
                    return foundation::parse_error{
                        where, "radix must be between 2 and 36"};
                }
                return radix_p(infix)(after.bump(), rc);
            case sharpsign_kind::block_comment: // consumed as intertoken space
            case sharpsign_kind::none:
                break;
            }
            return foundation::parse_error{where, "unsupported '#' syntax"};
        }};
    };
    return bind(argument_p, dispatch)(cur.bump(), ctx);
}
```

The handoff written for this step said, in as many words, that this was where `satisfy` would finally have to change. The argument is good. `satisfy` reports failure at the cursor it was handed, always, and all five of this function's diagnostics report at the `#`, behind wherever the cursor has got to by then. The infix numeric argument is the one place a character-level primitive belongs. So the prediction was that this step would be the second caller. It would extend `satisfy` with a position parameter in place, which has been the standing instruction since the limitation was first written down two steps ago.

It did not, and the reason is better than a decline. The digit run cannot fail. Absence of a digit is a **value** there, not an error: the argument is optional, and `-1` means absent. So the composition wraps `satisfy` in the optional combinator, which converts a failure at the starting cursor into an unengaged `std::optional`. The repetition reads that as "stop". The fold rides along in `map`. `satisfy`'s cursor-positioned failure exists, and no caller ever sees it.

Two steps running have now been named in advance as the caller that would force it, and neither turned out to be one. The request stays open and stays unjustified. That is a more useful thing to write down than a primitive no one calls.

The other predicted pressure point was the readtable query itself. A parser predicate is context-free, so `macro_of` and `sharp_of` could not be `satisfy` predicates without capturing the context, which the scoping note forbids. That did not come up either: both are read inside a parser that already has the context threaded to it, so neither wanted a context-taking overload.

While reading that function closely, one small thing does not add up. Its own doc comment says the argument guard is "a precondition on four of the seven arms". Count the arms: the guard appears on `function_quote`, `character_literal` and the three fixed-radix cases. That is five. The other two of the seven handle the argument themselves; `vector_open` refuses it with a message of its own, and `radix_n` requires it to be between 2 and 36. Five plus one plus one is the seven the same sentence claims. Four leaves an arm unaccounted for. The step file said four, the code comment repeats it, and the architecture note repeats it again.


# Then the build stopped

None of the above is what this step is about.

Partway through, on a translation unit the step had not touched and had no reason to think about, GCC stopped:

```text
error: 'constexpr' evaluation depth exceeds maximum of 512
       (use '-fconstexpr-depth=' to increase the maximum)
```

Not a failing assertion. Not a wrong diagnostic. A compiler limit, on a flag the project sets nowhere.

Not one it had never heard of, either. The series has named `-fconstexpr-depth` twice already, once about continuation-passing style and once about Mendler recursion, both times to make the same point: the constant evaluator counts every nested call whatever its return position, so the object program's depth is the compiler's depth. The project's C++ rules turn that into an instruction. Evaluation does not recurse in C++ at all. Which is why the evaluator is a small-step machine with an explicit frame stack, and why its depth and step limits are capacities the machine diagnoses itself, instead of a compiler flag and a build that never finishes.

The rule says evaluation. The reader is not evaluation. Its recursion is ordinary C++ recursion and always was: `read_node` calls `read_wrapped` which calls `read_node`, and inside a `static_assert` every one of those is a frame. Nothing was ever wrong with that. Nothing had ever cost enough to notice.

Reifying a parser adds frames to that chain. Not many. A handful per level of nesting in the source being read, which is a fine number right up until it is multiplied by the nesting of something a person might plausibly write.

The translation unit that noticed is `src/smd/cl/printer/prin1.test.cpp`, which reads sixty-four nested quotes at `static_assert`. It exists for an entirely different reason. The printer grows its rendered text by eight characters per level of `(QUOTE ...)`, so `1 + 8*63` fits in its 512-character buffer and `1 + 8*64` does not. The case pins that the overflow is diagnosed and not silently truncated. Written eight phases back, with the printer, to check a character count. Nothing about it was aimed at evaluation depth, and the two 512s in this paragraph have nothing to do with each other.


# Where the frames already were

The step measured instead of estimating. `-fconstexpr-depth=N` on a `-fsyntax-only` compile of that one file, bisected for the smallest `N` that still compiles, gives a number and not an impression.

-   **424** on the tree this step started from.
-   **570** with the dispatch converted and `parser<F>` still holding its callable and forwarding to it. Over the cap. The build stopped, which is how any of this got found.
-   **under 380** after the change described in the next section.

They reproduce, mostly. Bisecting the pre-step tree gives 424 exactly. The merged tree gives **363**, which is the "under 380". The middle number does not come back: putting the pre-step `parser<F>` on the merged dispatch and bisecting again gives **562**, not 570. Eight frames, and the likely explanation is in the measurements row, which records two attempts. 570 is the number the first conversion produced, and the first conversion is not what merged. Either way it is over 512 and the build stops. The wall does not care about eight frames.

Worth more than any of those is what happens when the same bisection is run backwards across the phase's earlier tags. No one had done it. Until this step there was no reason to:

| tree                                     | minimal `-fconstexpr-depth` |
|---------------------------------------- |--------------------------- |
| before the phase opened                  | 167                         |
| the split into eight headers             | 167                         |
| the first `bind`                         | 167                         |
| intertoken space                         | 167                         |
| the delimited list, and ordered choice   | 167                         |
| strings and character literals           | 167                         |
| the quote family and token data          | 424                         |
| this step's dispatch, forwarding wrapper | 562                         |
| this step, as merged                     | 363                         |

The split moved the number by nothing, and so did four of the five conversions after it, because none of those four sits on the path a nested quote takes. The fifth moved it by 257 frames on its own. It converted `read_wrapped`, exactly the function the recursion cycles through. It reported five files, 140 insertions, one attempt, twenty minutes and a green matrix, and it was right about every one of those, while leaving 88 frames of margin under a cap no one in the series had ever looked at. The next step wanted 138.

The step's own commit message records the turn. It was handed one to use, four paragraphs long, all of them about the readtable and why the switches stay switches, under the subject "the readtable dispatch, and the last two hand-written parsers". It kept the four, added four more, and changed the subject to name the cost instead of the parsers. None of the four it added was predicted by the plan, because nothing in the plan had measured this.

So the closing sentence of the architecture note this step wrote is wrong, and wrong in the flattering direction. It says the reshape "brings the same case to under 380: below where the series started". 363 is below where **this step** found the number, which is the true and useful claim. The series started at

1.  The phase has slightly more than doubled the constant-evaluation stack

this case needs, and there is no wording of that fact which makes it a saving.


# A parser that is its callable

The fix is three lines of header and it is not a micro-optimization.

```cpp
// before
template <class F>
class parser {
  public:
    constexpr explicit parser(F f);
    template <parse_context Ctx>
    constexpr auto operator()(cursor cur, Ctx &ctx) const;
  private:
    F f_;
};

// after
template <class F>
class parser : private F {
  public:
    constexpr explicit parser(F f);
    using F::operator();
};
```

A parser stops holding a callable and starts **being** one. The function object becomes a private base and its `operator()` is re-exported. Invoking a parser is now one constant-evaluation call where it used to be two: the wrapper's, and then the lambda's inside it.

The saving is not local to the dispatch. It applies to every reified parser anywhere on the chain, including all of the ones the previous five steps placed. So the merged tree lands below the tree this step started from. Clearing 512 was the smaller half of it. Measured on the pre-step tree, the reshape alone takes 424 to 294; on the merged tree it takes 562 to 363, because the merged tree runs more parsers per level.

It also settles whether a wrapper this thin was ever doing anything. It was doing one thing, stating `parse_context` on the context parameter. The honest reading is that it could never state it usefully, because the callable underneath has to name its own context parameter anyway. Restating a constraint costs a call. The constraint does not need the call.


# A guard, checked twice

The claim that the guard survives is the kind that usually gets asserted and not tested, and this phase has produced several of those. This one was tested twice, in two different ways.

First a grep. Does any parser in the kit or in the reader name a bare unconstrained context parameter, such that dropping the wrapper's own constraint would let a `cursor` through? Every context parameter in the kit and in the reader spells `parse_context auto &`, or the reader's own refinement of it, or a concrete context type. No site relies on the wrapper.

Second the diagnostic itself. Take a parser over a callable that names `parse_context auto &`. Call it with a `cursor` where the context belongs, and compile against the old header and the new one. Both reject it, and the error line is byte for byte the same:

```text
error: no match for call to '(const smd::kit::parser::parser<
       <lambda(smd::kit::parser::cursor, auto:39&)> >)
       (smd::kit::parser::cursor&, smd::kit::parser::cursor&)'
```

The notes under it are not the same, and the difference is the change itself. Before, GCC names one candidate and it is the wrapper's own `operator()`. After, the one candidate is the lambda. Both then walk to the same line of the same header, `concept parse_context = !std::same_as<..., cursor>`, and report the same expression evaluated to `false`. Same reason, same place, different candidate. What the step reported is that the diagnostic is unchanged. What is unchanged is the error and the reason; what moved is which function the compiler blames. Nothing rests on it. The narrower claim is the one to keep.

The test file gains one case and four `static_assert` lines pinning both directions with `std::invocable`. That is the only new test case in the step, and the only change to any test anywhere in it.


# The numbers

Eight files, 315 insertions, 109 deletions, two attempts, green on both legs of the matrix. 3150 seconds, fifty-two and a half minutes. Second longest of the seven steps landed so far, behind the string reader's fifty-seven. Also behind the delimited-list step's wall clock, which reads fifty-one hours and measures nothing, as its own note says.

Three of the eight files were outside the declared scope and all three are consequences of the same two decisions. The forward declaration of `read_node` had to gain the `reader_context` constraint the definition gained, or the two declare different templates. `parser.hpp` is the reshape. `parser.test.cpp` is the case pinning it. It moves the harness count from 378 entries per leg to 379, after two consecutive steps that added cases without moving it.

Seven functions in the reader are now constrained with `reader_context`, plus that forward declaration. The one still spelled `template <class Ctx>` is the delimited-list reader, which names the concept only on its inner step closure. No step in this phase has owned it. The doc comment on the concept still says only the radix number reader is constrained with it. That has been wrong since the third step of the phase, and three consecutive workers have now flagged it without it being in any of their scopes.

And `and_then`, the spelling the reader used before the layer existed, is gone from the umbrella header. It is not gone from the reader. Seven call sites remain, three in the delimited-list reader and four across the string and character readers, and every one is `result`'s own bind wrapped around a tree append. Appending to a tree is not a parse step. Counting mentions instead of calls gives sixteen, because the doc prose and a `using` declaration match the grep too. The last three steps have each had to make that distinction in their own words.

One small regression in the record. The step before this one was the first in the whole run whose measurements row said what it actually ran, listing the linter and the transclusion check alongside the matrix and the header compile. The row here is back to `make test-matrix + compile-headers`. Its own handoff says the linter was run, and that it was not green the first time, because clang-format reflowed four files the step had already edited.

A divergence number was reserved for this step, as one was for each of the six before it. None of the seven has been used. The nearest any of them has come is here: a step told to touch the layer only if a primitive needed extending, changing the shape of the type the whole layer is built on. It went down as an out-of-scope file in the measurements row.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 44 - A Record of Declines, and a Test Made to Fail](phase-44-a-record-of-declines.md)

</nav>


# References
