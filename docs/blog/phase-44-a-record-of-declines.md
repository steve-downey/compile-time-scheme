**DRAFT &mdash; pending author revision**

<div class="abstract" id="orgd9e2515">
<p>
The quote family and token data move onto the combinator layer, and the layer gains nothing. Neither did the step before it.
The brief asks for more than the conversion. If the primitive set has stopped growing, say so, and settle a note that has been marked provisional since the layer was built.
A layer that stopped needing additions and a layer nothing dares extend look the same from outside. Same file list, same history, same green matrix.
Two steps adding nothing is thin evidence. What the architecture note rests on instead is the record of what was asked for and turned down: a collecting repetition with no client yet, a bounded one that would have owned state its caller needed, a parser for "any character that is not this one" whose negation was really a stop condition in the wrong place, and this step, which wanted nothing.
The quote reader was <code>bind</code> written out by hand, twice, over two types that happened to spell their sequencing the same way. Only the outer one moves.
Appending to a tree is not a parse step, so the inner one stays where it was. That is why the reader still has calls that look unconverted and are not.
What stopped being written down is the cursor the read resumes from. It used to be spelled out by hand inside the continuation.
The branch is recorded at the marker's position, not the wrapped datum's. Nothing in the types keeps those apart, and the brief said so.
No existing test would have caught a swap either: that position reaches the tree only as the position of a diagnostic no successful read ever raises.
There is a case now, a read with room for exactly one node, where the leaf fills the tree and the quote's branch is what overflows.
Then the step mutated the code and watched the case fail. Run again against the tagged tree, substituting the resumption cursor's position breaks that <code>static_assert</code> and no other.
Put the two nested <code>and_then</code> calls back and the case still passes. It should: nothing about the behaviour changed.
The outside oracle ran, for the first time in this phase. SBCL 2.6.0.debian, 180 assertions across three cases, one of which is a header included twice.
The corpus happens to cover both halves of what this step touched, down to a bare <code>'</code> in its error table.
The commit message the step was handed closes on a claim that was already true three weeks ago, in the phase before this one. What landed does not make it.
The architecture note says the conversion is worth more than four fewer lines. Four is the right number with the wrong sign.
Five files, 140 insertions, one attempt, twenty minutes.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 43 - Any Character At All, and a Step Started Twice ←](phase-43-any-character-at-all.md)

</nav>


# The question, not the task

Most steps in this run get a task and a paragraph saying why the task exists. This one gets a question as well, and it is the more interesting half of the brief. If the primitive set has stopped growing by now, say so, and settle the note that has been sitting marked provisional since the layer was created.

The reason for asking is in the same sentence of the brief. A layer that stopped needing additions and a layer nothing dares extend look identical from outside: same file list, same commit history, same green matrix. Nothing in the code tells them apart, and the first is a much better thing to have.

The brief asks for the claim on the strength of three consumers that needed nothing. There are two. The step that landed ordered choice and the bounded repetition added both of them, and only the two steps since have added nothing at all. So the arithmetic the instruction asks for does not close. The step makes the claim anyway, on evidence of a different kind.


# Bind written out by hand, twice

```cpp
/// Reads the datum after a quote-family marker and wraps it in a
/// one-child branch of @p kind.
///
/// The example @c docs/cl-parser-scoping.md § 1 leads with, and the
/// shortest statement of what the last five steps were for. Before the
/// layer existed this was @c and_then nested inside @c and_then -- the
/// inner datum's result feeding a branch append, the branch append's
/// result feeding a @ref parse_state -- which is @c bind written out by
/// hand, twice, over two different monads that happened to share a
/// spelling. Now the outer one is the parser Monad's own @c bind: @ref
/// read_node lifted into a parser value, and a continuation that wraps
/// whatever it produced.
///
/// Two positions have to stay apart and neither is checked by the types.
/// The branch is recorded at @p where -- the *marker's* position, passed
/// in by @ref read_sharpsign or by @ref read_node's readtable dispatch,
/// not the inner datum's, which for `'x` is one character further on. The
/// cursor the whole thing resumes from is the inner datum's rest, and that
/// one is no longer written down at all: @c bind threads it into the
/// continuation's parser, which is exactly the class of transcription
/// mistake the conversion removes rather than merely avoids.
///
/// @ref read_node is still an ordinary function template reached through
/// @c detail/read_node_fwd.hpp, wrapped here in a @c parser at each call
/// rather than made a parser value that closes over itself. The mutual
/// recursion is B7's to look at; this step only stops writing its bind out
/// longhand.
template <reader_context Ctx>
[[nodiscard]] constexpr auto read_wrapped(cursor after_marker, Ctx &ctx,
                                          datum_branch kind,
                                          foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    auto const node_p = smd::kit::parser::parser{
        [](cursor c, reader_context auto &rc) { return read_node(c, rc); }};
    auto const wrap_in_branch = [kind, where](int inner) {
        return smd::kit::parser::parser{
            [kind, where, inner](cursor rest, reader_context auto &rc)
                -> foundation::result<parse_state<int>> {
                typename Ctx::child_list children;
                children.push_back(inner);
                return bind(
                    add_branch_checked(rc, kind, children, where),
                    [rest](int id) -> foundation::result<parse_state<int>> {
                        return parse_state<int>{id, rest};
                    });
            }};
    };
    return bind(node_p, wrap_in_branch)(after_marker, ctx);
}
```

Before the layer existed this was `and_then` around `read_node`, and inside that continuation a second `and_then` around the branch append. Two nested continuations over two different types, both of which happened to spell their sequencing the same way.

Only the outer one moves. `read_node` is lifted into a `parser` value at the call site, a fresh three-line wrapper and not a parser that closes over itself. The mutual recursion belongs to a later step, and nothing here needed to touch it. `bind` over that wrapper takes the continuation.

What stopped being written down is the cursor. It used to be `inner.rest`, spelled out by hand and handed to the returned `parse_state`. Now it arrives as the continuation parser's own argument, and appears only where it is used. The radix number reader made the same observation when the layer was new. But here, for the first time, the version being replaced was a **nested** hand-written bind and not a single one.

The scoping note that argued for all of this is cited here as leading with `read_wrapped`, which is generous to the example. What its first section leads with is the reader having already re-derived `parse_state` and the layer's result type under other names. The function it names as the actual argument for changing anything is a different one, the list reader with its `if (!r.has_value()) return r;` ladder. `read_wrapped` is the example it gives for a bind written by hand, and the shortest of the three.


# The conversion that did not happen

The inner sequencing is still there, spelled `bind` now instead of `and_then`, which is the generic name for the same operation over the same type. It did not move to the parser layer, and it should not have. `add_branch_checked` pushes a child list into the datum tree and hands back a `result<int>`. Appending to a tree is not a parse step. It reads no input and it has no cursor. Wrapping it in a `parser` to make the call chain look uniform would say something false about it.

That distinction is the whole reason the reader still contains calls that look unconverted and are not. The list reader, the only other function in the file, keeps three `and_then` calls for exactly the same reason, and the brief forbids touching it.

Which leaves one of the step's own spot checks impossible to satisfy. `grep -n 'and_then' src/smd/cl/reader/detail/forms.hpp`, expect 0. The file has six matching lines at the merge. Three are the list reader's, in the function the same document says not to touch. The other three are English. Two of those are in the list reader's doc comment and predate the step; the third is in the doc comment this step wrote, explaining what nested `and_then` used to look like. Seven lines before, six after. Zero was never available.

The step reported that instead of making the grep true, and wrote the contradiction into its own measurements row as well as into the handoff. Both halves of the instruction were in the same file, a hundred lines apart.


# Two positions, and a test made to fail

The branch is recorded at the marker's position. The cursor the whole read resumes from is the inner datum's. For `'x` those differ by one character, both are a `source_pos`, and nothing in the signature keeps them apart.

The brief says so, and adds that no test would obviously say so either. The brief is right, and for a specific reason: the marker's position reaches the tree only as the position of a `datum tree full` diagnostic. On every successful read the choice is invisible. Every reader test that existed either read successfully or failed somewhere else entirely.

So the step wrote the case that makes it visible.

```cpp
sym_table syms;
auto const r = read<1, 8>(" 'x", syms);
return !r.has_value() &&
       std::string_view{r.error().message} == "datum tree full" &&
       r.error().where.line == 1 && r.error().where.column == 2;
```

Room for exactly one node. The leaf `x` goes in and fills the tree, so the quote's branch is what overflows. The error comes back at column 2, the quote. Not column 4, where the cursor has got to by then.

Then the part that matters. The step mutated `read_wrapped` to pass the resumption cursor's position where it passes the marker's, and checked that the new `static_assert` stopped compiling. Take the tagged tree, apply that one substitution, and compile the test file with nothing but `-fsyntax-only`: GCC reports one error, and it is that assertion. Every other `static_assert` in the file still holds. A pin no one has watched fail is a guess about what it covers.

One more thing falls out of having the tree in a scratch directory. Put back the two nested `and_then` calls &mdash; the whole pre-conversion function &mdash; and the new case still passes. It should. The step changed no behaviour, so the position it pins was already right and had been since the function was written. What the case adds is not correctness. Nothing else in the repository would have noticed.


# A whole token, or nothing

```cpp
/// Reads a token datum: scans the whole token, then classifies it
/// (DIV-0003, whole-token classification): a number by the decimal
/// grammar, else a keyword (leading unescaped colon), else a symbol —
/// interned either way (decision D12).
template <reader_context Ctx>
[[nodiscard]] constexpr auto read_token_datum(cursor cur, Ctx &ctx)
    -> foundation::result<parse_state<int>> {
    auto const where = cur.position();
    auto const classify_and_intern = [where](token_text const &token) {
        return smd::kit::parser::parser{
            [where, token](cursor rest, reader_context auto &rc)
                -> foundation::result<parse_state<int>> {
                auto const finish = [&](datum_atom atom) {
                    return bind(
                        add_leaf_checked(rc, std::move(atom), where),
                        [rest](int id) -> foundation::result<parse_state<int>> {
                            return parse_state<int>{id, rest};
                        });
                };
                if (!token.has_escape) {
                    auto const classified = classify_number(token.view(), 10);
                    switch (classified.cls) {
                    case number_class::fixnum:
                        return finish(
                            datum_atom{datum_fixnum{classified.value}});
                    case number_class::bignum:
                        return finish(datum_atom{
                            datum_tower{tower_kind::bignum, 10, token}});
                    case number_class::ratio:
                        return finish(datum_atom{
                            datum_tower{tower_kind::ratio, 10, token}});
                    case number_class::floating:
                        return finish(datum_atom{
                            datum_tower{tower_kind::floating, 10, token}});
                    case number_class::none:
                        break;
                    }
                }
                // A keyword interns under its colon-carrying spelling, so
                // :FOO and FOO are distinct entries until a package system
                // exists — see datum.hpp on @c datum_keyword.
                auto const view = token.view();
                bool const keyword =
                    !token.has_escape && view.size() > 1 && view.front() == ':';
                return bind(intern_checked(rc.symbols, view, where),
                            [&](symbol::symbol_id id)
                                -> foundation::result<parse_state<int>> {
                                return finish(
                                    keyword ? datum_atom{datum_keyword{id}}
                                            : datum_atom{datum_symbol{id}});
                            });
            }};
    };
    // DIV-0003, accepted-permanent, and the shape here is what enforces it
    // rather than merely agreeing with it. @ref token_p scans a *whole*
    // token — the maximal run of constituents, @c scan_token unmodified —
    // and only the continuation classifies what came back. Nothing in this
    // function may decide anything before the token ends: not a @c satisfy
    // over constituent characters, not a choice between a number parser
    // and a symbol parser, neither of which can see the token it is half
    // way through. The retired Scheme reader had a greedy-digit integer
    // parser and that is precisely the shape DIV-0003 records as wrong for
    // Common Lisp, where a symbol may start with a digit: it reads `1+` as
    // the fixnum 1 with a stray `+` left over, instead of the symbol `1+`.
    return bind(token_p, classify_and_intern)(cur, ctx);
}
```

The second function is the opposite kind of conversion. It scans a whole token and only then classifies it. The project's record of deliberate departures has carried that as permanent since July, when a port of the Scheme reader's greedy-digit integer parser read `1+` as the fixnum 1 with a stray `+` left over. Common Lisp symbols may start with a digit. Scheme's may not. No decision taken part way through a token can be right here.

A combinator rewrite reaches for the wrong shape by default, which makes this the one conversion in the series that could have broken something quietly. `satisfy` over constituent characters, or an ordered choice between a number parser and a symbol parser: neither can see the token it is half way through. The second is what the retired Scheme reader did. So the conversion keeps the shape it had, one `bind` over the whole-token scan, with everything that decides anything living in the continuation.

`token_p`, the lift of `scan_token` into the layer, is not in this file. It landed with the layer's first `bind` and has had exactly one caller since, the radix number reader. It has two now.

`scan_token` and `classify_number` themselves are untouched here, and out of scope for the whole phase. Nothing has moved them, which is what makes the behaviour safe by construction instead of by test. What this half leaves behind is the comment at the `bind` site, naming the shapes that are wrong at the one place a later rewrite would reach for one of them.


# The oracle ran

The step before this one could not use the differential. SBCL was not on the machine, the cases skip when it is absent, and the harness counts a skip as a pass. A green matrix said nothing about it. SBCL went back on the machine the day this step ran.

So the number is worth reading closely. The reader differential comes back at 180 assertions across three test cases. One of the three is a header idempotency check that asserts `true`, so the oracle is the other 179. Forty-one source strings, read by this reader and by SBCL and compared on what each prints. Five more, read by both and compared on the fact that both fail.

And the corpus covers both halves of what this step changed, by luck: it was written for a printer three weeks ago, in the phase before this one. `'x` and `#'car` are in it for the quote family. `1+` and `2buffer` are in it for whole-token classification, and the label on the first says so in as many words. A bare `'` is one of the five error strings, which is the quote reader's own failure path. Nothing disagreed, before or after the change.

The version is recorded too, which it needed to be. Earlier the corpus agreed against SBCL 2.2.9. What is installed now is 2.6.0.debian, so this is the first time these forty-six strings have been put to that implementation, and nothing in it disagrees either.

The commit message the step was handed ends on a claim. This is the first time the project's most load-bearing reader behaviour has been checked against something other than its own opinion. It isn't. The printer work did that in the phase before this one, against 2.2.9, and `1+` was one of its eleven strings then. What landed drops the sentence. The architecture note keeps the idea and narrows it to this phase, where it is true. An inherited overclaim, caught and not repeated. That is rarer in this run than it should be.


# Asked for, and declined

Back to the question. The layer gained nothing here and nothing last step, which on its own is a fact about two commits. Not much of one. What the architecture note writes down instead is the list of things that were asked for and turned down:

-   a collecting repetition owning a capacity, declined for having no client yet;
-   a bounded one, declined because the position of its overflow diagnostic comes out exact only when the closure already holding the cursor also holds the capacity check;
-   a parser for "any character that is not this one", declined because the thing being negated was the repetition's own stop condition, put in the wrong place;
-   and this step, which wanted nothing.

Three refusals with reasons attached are a different kind of record from two quiet steps. A layer no one dares touch cannot produce them.

One request is still open, and the note leaves it open rather than settling it early. `satisfy` reports failure at the cursor it was handed, and the string reader's diagnostics are anchored at the opening quote, so it could not serve them. Neither function here is the second caller. Both diagnose through helpers that already take the position to report at. The standing instruction is unchanged, which is to give `satisfy` a position parameter when a second caller turns up rather than build a parallel primitive beside it.

The handoff written for the next worker opens by saying the parser directory is byte-identical to where it stood when ordered choice landed. It isn't, and the step before this one said it first. Two commits touched it in between. One is a clang-format rewrite of four lines. The other is an out-of-band maintenance step that rewrote the Monad registration under names that had changed elsewhere, and supplied `fmap` natively because the inherited derivation holds its callable by reference and this instance does not run it before returning. Neither came from a consumer. That is the claim that matters, and the one the word gets in the way of. The set of primitives has not changed since ordered choice landed. The bytes have.


# The numbers

Five files, 140 insertions, 48 deletions, one attempt, green on both legs of the matrix. 1238 seconds, a little over twenty minutes. Two steps in this phase have reported less. The split into eight headers took 695 seconds, and the skipping step 266, over two attempts.

And neither converted file got shorter. `read_wrapped` was eighteen lines and is twenty-two. `forms.hpp` went from 119 lines to 152, its comment lines from 38 to 65; `token_datum.hpp` from 71 to 92, comments from 6 to 22. The architecture note says the conversion is worth more than four fewer lines. Four is the right number and the sign is wrong: the function gained four lines. The sentence it means to be is that the line count was never the point, which is what the rest of it goes on to say.

The test file gained sixteen lines and no new `TEST_CASE`, so the harness count does not move: 378 per leg, as it was before. The step before this one reported that trap after walking into it, and this one predicted no rise.

The context header's comment on the reader-context concept still says only the radix number reader is constrained with it. Five function templates are, after this step. It went stale three steps ago. The step before this one flagged it; this one left it alone as no one's declared scope. And the handoff's own count of those five is six. The sixth is the list reader, whose own template is still a bare `class Ctx`, with the concept only on its inner step closure. The last line of the architecture section this same step wrote says exactly that.

A divergence number was reserved for this step, as for each of the five before it. None of the six has been used, so three weeks of moving a reader onto a new layer have recorded no deliberate departure from anything at all. Either the plan is being followed exactly, or no one is writing down the places it isn't.

The measurements row is the first in this run to say what the step actually ran. Every earlier step's row records `make test-matrix + compile-headers`; this one adds the linter and the transclusion check. Every one of those step files asked for all four.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 43 - Any Character At All, and a Step Started Twice](phase-43-any-character-at-all.md)

</nav>


# References
