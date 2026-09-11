# Handoff to B7

## The primitive set stopped growing, and B2's provisional note is resolved

`src/smd/kit/parser/` is byte-identical to its state at the end of B4 — B5
extended nothing and B6 extended nothing. Two independent consumers in a row is
the second data point B2's note was waiting for, so it is resolved in
`docs/compiler_architecture.org` § B2 under *Update, B6*: the set is `pure`,
`map`, `bind`, `satisfy`, `char_p`, `skip_many`, `many_until`, `operator|` /
`alt`, `optional`, and it stopped needing additions because its clients stopped
asking rather than because nobody dared ask.

What separates those two readings from outside is a record of what was asked for
and declined, and there is one: B3 declined `many<Capacity>`, B4 declined
`many_bounded`, B5 declined a negated-character parser, B6 declined nothing
because it wanted nothing. Half the note stands and is still marked as standing:
`map` is still not a registered `functor<parser<F>>`, for the reason it never
was. B8 owns DIV-0028's disposition.

**You are the last chance for this claim to be wrong.** If your dispatch needs a
primitive, that is a real finding and a fair one — extend in place, add its law
tests, and say so. Do not let the claim above make you build around a gap.

## `satisfy`'s position limitation is still open, and B6 was not its second caller

B5 recorded it: `satisfy(pred, expected)` fails at `cur.position()`, always, so
it cannot serve a caller whose diagnostic is anchored at a `where` it was handed.
B6 did not hit it — `read_wrapped` and `read_token_datum` diagnose through
`add_branch_checked`, `add_leaf_checked` and `intern_checked`, all of which
already take the position to report at, so neither needed a character-level
parser with a relocated failure.

**You may well be the second caller.** All five of `read_sharpsign`'s
diagnostics report at `where`, the `#`'s own position, and the infix digit-run
parser your step file asks you to build sits exactly where `satisfy` would be the
obvious primitive. If you reach for it and the position is wrong, the fix is a
position parameter on `satisfy` — extend it in place, do not write a parallel
primitive beside it. `AGENT-PROMPT.md` is explicit that forking a shared
abstraction is worse than halting.

`satisfy`'s predicate is also context-free, so a readtable query
(`table.macro_of`, `table.sharp_of`) cannot be a `satisfy` predicate without
capturing `Ctx&`, which D29 forbids. Same instruction: a predicate overload
taking the context, in place, if you turn out to need one.

## How B6 recursed through `read_node`, since you own the other end

`read_wrapped` lifts `read_node` into a `parser` value at the call site:

```cpp
auto const node_p = smd::kit::parser::parser{
    [](cursor c, reader_context auto &rc) { return read_node(c, rc); }};
```

That is a fresh wrapper per call, and deliberately so. `read_node` is still an
ordinary function template reached through `detail/read_node_fwd.hpp`; nothing
here made it a parser value that closes over itself, and nothing here needed to.
The lambda names `reader_context auto &rc` rather than the outer `Ctx`, so the
wrapper is generic over whatever context `read_node` is instantiated with.

If your conversion of `read_node` wants that lift to be a named parser value
shared by all three recursion sites, that is yours to place — `read_node_fwd.hpp`
is the natural home and it is in your scope. Do not assume `read_wrapped`'s copy
is load-bearing; it is three lines and duplicating or deleting it is fine.

## What `read_wrapped` needs of the branch position, because your dispatch passes it

`read_wrapped(after_marker, ctx, kind, where)` records the branch at `where` —
the **marker's** position — and resumes from the inner datum's rest, which
`bind` now threads rather than any code spelling out. Both `read_sharpsign`'s
`function_quote` arm and `read_node`'s quote-family arms pass `where` today, and
both must keep doing so.

The difference is observable in exactly one place: `where` reaches the tree only
as the position of an `add_branch_checked` `"datum tree full"` diagnostic, so on
every successful read the choice is invisible and no test said anything about it.
B6 added one. `read.test.cpp` gained `wrapped_branch_error_sits_at_the_marker` —
`read<1, 8>(" 'x", syms)`, where the leaf `x` fills the tree and the quote's
branch overflows, pinning column 2 rather than column 4. It was mutation-checked:
substituting the resumption cursor's position fails the `static_assert`, so it is
not vacuous. It will fail if your dispatch starts passing a different position.

## Out-of-scope file B6 touched

`src/smd/cl/reader/read.test.cpp`, additively: one new `constexpr` function
(`wrapped_branch_error_sits_at_the_marker`), one new `static_assert`, and one new
`CHECK` inside the existing `ReadTest - Errors` case. No existing expectation
changed and no new `TEST_CASE`, so the ctest entry count is unchanged at **378**
per leg — the same counting gotcha B5 reported.

**Your step file's spot check is wrong as written.** It says
`git diff --name-only cl-parser-combinators..HEAD | grep '\.test\.cpp$' | grep -v
kit/parser` "must return nothing: no `cl` test changed anywhere in Phase B."
That is false of Phase B as a whole — B5 and B6 both added cases — and it
contradicts `AGENT-PROMPT.md`'s standing rule that adding a case is fine while
changing an expectation is a halt. B5 and B6 both resolved the contradiction the
same way, in favour of the standing rule. Your own diff is measured from
`cl-parser-combinators`, which already contains both merges, so the check will in
fact return nothing for you; it is the *justification* that is wrong, not the
command. If your own conversion wants a pinning case, add one.

Two more transcription defects, both in step files rather than in code, both
already reported by B5 and both still true:

- If your copy of the step file still globs `./.build/*/*/cl_reader_test`, it
  resolves nothing. The real paths are
  `.build/build-gcc-16/src/smd/cl/reader/{Asan,Debug}/cl_reader_test` and
  `.build/build-gcc-16/src/smd/cl/conformance/{Asan,Debug}/cl_conformance_test`.
  The copy in the main checkout has already been corrected; the one on the
  Phase B branch has not.
- B6's own "expect 0" grep for `and_then` in `forms.hpp` contradicts the same
  step file's "do not touch `read_delimited`". `read_delimited` legitimately
  uses `and_then` three times — that is `result`'s own bind around
  `add_branch_checked`, not a parser-layer holdout — and B6 left all three. Your
  step file asks for `grep -rn 'and_then' src/smd/cl/reader/` to find nothing
  after your change; expect those three in `forms.hpp` plus whatever
  `read_delimited`'s step closure keeps, and report what you actually see rather
  than making the grep true by editing a function you do not own.

## The SBCL differential is live and you should insist on it

`sbcl` is installed: **SBCL 2.6.0.debian**. B5's handoff said it was missing;
that is no longer true. `cl_conformance_test "*ReaderDifferential*"` runs green
at **180 assertions in 3 test cases**, and `"*Sbcl*"` at **300 in 5**, identical
before and after B6's change. A4 and A5 recorded their agreement against 2.2.9,
so B6 is the first step to exercise the corpus against 2.6, and nothing in it
disagrees.

Check the assertion count, not the exit status. The differential cases skip when
`sbcl` is absent and ctest counts a skip as a pass, so a green matrix on its own
is not evidence the oracle ran. If you see far fewer than 180, it skipped.

## Anchors B6 landed, if you need to point at them

- `086ca303-e32b-40bb-88fd-09a2cffaad61` — converted `read_wrapped`,
  `src/smd/cl/reader/detail/forms.hpp`.
- `a7927622-983d-47f3-bf8e-33985ae5db64` — `read_token_datum`,
  `src/smd/cl/reader/detail/token_datum.hpp`, including the `bind(token_p, …)`
  site and its DIV-0003 comment.

Both are transcluded from `docs/compiler_architecture.org` § B6. Per D21 they are
B6's and you owe them nothing; the only obligation is that
`scripts/verify-transclusions.sh` is green at your merge. It exits 0 today with
one pre-existing WARN on `docs/blog/phase-12-cps.org`, which is not yours.

## One thing `read_context.hpp` still says that is false

Its doc comment on the `reader_context` concept still reads "Only
`read_radix_number` is constrained with this concept in this step". Six functions
are now. It went stale at B3, B5 flagged it, and B6 did not fix it either —
nobody's declared scope, and B8 is the integration step and the natural owner.
After your step it will be wrong about `read_sharpsign` and `read_node` too.
