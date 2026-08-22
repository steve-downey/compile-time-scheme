# Step B6 — forms: the quote family and token data

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. One front end, `src/smd/cl/`, over one substrate, `src/smd/kit/`. B1
split the reader into `src/smd/cl/reader/detail/*.hpp`. B2–B5 built
`smd::kit::parser` incrementally — `pure`, `map`, `bind`, `satisfy`, `char_p`,
`skip_many`, choice, bounded repetition — each primitive landing with the
reader function that needed it. Converted so far: `read_radix_number`, the two
skippers, `read_delimited`, `read_string`, `read_character`.

**Integration branch: `cl-parser-combinators`.**

**Reserved for this step:** blog phase **43**, divergence number **DIV-0039**.

## Phase B's standing constraints

- No observable behaviour change: no new syntax, no changed diagnostic text, no
  different error position.
- DIV-0003 must not regress; `1+` reads as the symbol `1+`.
- `scan_token` and `classify_number` are out of scope for all of Phase B. A
  parser may call them; neither may be rewritten.
- Existing reader tests and the conformance corpus pass unchanged. Adding a
  case is fine; **changing an existing expectation is a halt**.

## Why

`read_wrapped` is the clearest thing in the reader. It is
`and_then(read_node(...), λ → and_then(add_branch_checked(...), λ))` — bind
written out by hand, twice, nested, to build a one-child branch. It is the
example `docs/cl-parser-scoping.md` § 1 leads with, and after five steps of
building the layer it should become one line of `bind`.

`read_token_datum` is the opposite kind of interesting. It is where DIV-0003
lives: it scans a whole token and *then* classifies it, which is the mechanism
by which `1+` reads as a symbol rather than as the fixnum 1 followed by a stray
`+`. Converting it is the one place in the series where a careless combinator
translation could classify character-at-a-time and break the behaviour the
whole plan names as most at risk. It is converted here, in daylight, with
`scan_token` and `classify_number` still untouched — which is what makes the
divergence structurally safe rather than merely tested.

## Setup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-b6-forms -b step-b6-forms cl-parser-combinators
cd ../step-b6-forms
git submodule update --init --recursive
```

## Verify GREEN baseline

```sh
make test-matrix > /tmp/verify-B6-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-B6-base.log
```

Not green ⇒ `blocked-B6.md`.

## What already exists that this step builds on

- `src/smd/kit/parser/{parser,repeat,choice}.hpp` — the primitive set as B5
  left it; your handoff carries the spellings and anything B5 found lacking.
- `src/smd/cl/reader/detail/forms.hpp` — `read_wrapped` (this step) and
  `read_delimited` (already converted by B4; **do not touch it**).
- `src/smd/cl/reader/detail/token_datum.hpp` — `read_token_datum`.
- `src/smd/cl/reader/detail/read_node_fwd.hpp` — the `read_node` declaration
  `read_wrapped` recurses through.
- `src/smd/cl/reader/detail/read_context.hpp` — `reader_context`,
  `add_leaf_checked`, `add_branch_checked`, `intern_checked`, and `token_p`,
  the lift of `scan_token` that B2 landed.
- `src/smd/cl/reader/token.hpp` — `scan_token`, `token_text` (`view()`,
  `has_escape`). `src/smd/cl/reader/number.hpp` — `classify_number(view,
  radix)` returning a `number_class` and a value. **Both out of scope.**
- `docs/divergences/DIV-0003-atom-maximal-munch.md` — read it before touching
  `read_token_datum`. It is short and it is the point of this step's second
  half.
- `docs/compiler_architecture.org` § `smd::kit::parser`, updated in place by
  B2–B5.

## The change

### 1. `read_wrapped`

Same signature, same behaviour. It reads a datum, pushes it as the single child
of a new branch of the given kind, and returns that branch with the inner
datum's remaining cursor.

The two nested `and_then`s become `bind`. There is nothing subtle here except
that the branch's position is `where` — the marker's position, passed in — not
the inner datum's, and the returned cursor is `inner.rest`. Both are easy to
swap by accident and no test would obviously say so, because for `'x` the two
positions differ by one character and most error paths never reach here.

Recursion through `read_node` goes through the forward declaration exactly as
it does now. Do not try to make it a parser value that closes over itself;
`read_node` is still an ordinary function template until B7.

### 2. `read_token_datum`

Same signature, same behaviour, and read DIV-0003 first.

The shape must stay: **`bind(token_p, λ)`** where λ receives a whole
`token_text` and classifies it. Not `satisfy` over constituent characters, not
a choice between a number parser and a symbol parser, not anything that decides
before the token ends. The retired Scheme reader had a greedy-digit integer
parser and that is exactly the shape DIV-0003 records as wrong for Common Lisp,
because CL symbols may start with a digit.

Inside λ, the existing logic is unchanged: if the token has no escape, classify
it with `classify_number(view, 10)` and finish as a fixnum or one of the three
tower kinds; otherwise, or if classification says `none`, decide keyword versus
symbol by the leading-colon rule (`!has_escape && size() > 1 && front() == ':'`)
and intern.

Note the interning subtlety already in the code and preserve it: a keyword is
interned under its spelling **with** the colon, so `:FOO` and `FOO` are
distinct entries. `datum.hpp`'s comment on `datum_keyword` explains why.

Put a comment at the `bind(token_p, …)` site naming DIV-0003 and saying that
the whole-token shape is load-bearing rather than incidental. That comment is
the cheapest defence this behaviour will ever get.

### 3. Constrain and anchor

Constrain both with `reader_context`. Anchor the converted `read_wrapped` — it
is `docs/cl-parser-scoping.md`'s headline example and the post will want it —
and the `bind(token_p, …)` site with its DIV-0003 comment.

Update `docs/compiler_architecture.org` § `smd::kit::parser` in place. If by
now the primitive set has stopped growing, say that: a layer that stopped
needing additions three consumers ago is a different and better fact than a
layer nobody dared extend, and the provisional note B2 left should be resolved
one way or the other.

## Declared file scope

```
src/smd/cl/reader/detail/forms.hpp         (read_wrapped only)
src/smd/cl/reader/detail/token_datum.hpp   (read_token_datum)
src/smd/kit/parser/*.hpp                   (only if a primitive needs extending)
src/smd/kit/parser/*.test.cpp              (law tests for any such extension)
docs/compiler_architecture.org             (in-place, at the kit anchor)
```

`src/smd/cl/reader/token.hpp` and `src/smd/cl/reader/number.hpp` are **not** in
scope and must not appear in your diff. If you believe one of them needs to
change, that is a halt: write `blocked-B6.md`. Their immutability is what makes
DIV-0003 safe, and a step that edits them has lost the property the whole
series was arranged around.

## Verify GREEN after

```sh
make test-matrix > /tmp/verify-B6-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-B6-after.log
wc -c /tmp/verify-B6-after.log
make compile-headers
make lint
./scripts/verify-transclusions.sh
```

## Spot checks

```sh
git diff --name-only cl-parser-combinators..HEAD
```

Must not contain `token.hpp`, `number.hpp`, or any `.test.cpp` under
`src/smd/cl/`.

```sh
grep -n 'DIV-0003' src/smd/cl/reader/detail/token_datum.hpp
grep -n 'scan_token\|classify_number' src/smd/cl/reader/detail/token_datum.hpp
grep -n 'and_then' src/smd/cl/reader/detail/forms.hpp        # expect 0
./.build/*/*/cl_reader_test "*Symbol*" "*Keyword*" "*Quote*" 2>/dev/null | tail -6
```

And the one that matters most, through the outside oracle A4 and A5 built:

```sh
./.build/*/*/cl_conformance_test "*ReaderDifferential*" 2>/dev/null | tail -5
```

That differential includes `1+`, read by `cl` and by SBCL and compared. It is
the only check in this repository that DIV-0003 holds against something other
than this project's own opinion, and this is the step it exists for.

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
cl: the quote family and token data read by combinator

read_wrapped was the example the scoping note led with -- and_then
nested inside and_then to build a one-child branch, which is bind
written out by hand. After five steps of building the layer it is one
bind, which is the whole argument made small.

read_token_datum is the opposite case. It is where DIV-0003 lives: a
whole token is scanned and then classified, which is why 1+ reads as the
symbol 1+ and not as the fixnum 1 with a stray + after it. A careless
translation to combinators would classify character at a time and break
exactly that, so the conversion keeps the shape -- bind over the whole
token scan -- and scan_token and classify_number are not touched here or
anywhere in this series. The comment at the bind site says so, because
that is the cheapest defence the behaviour will ever get.

The differential A4 and A5 built reads 1+ with SBCL as well as with this
reader and compares what both print. That is the first time this
project's most load-bearing reader behaviour has been checked against
something other than its own opinion.
EOF

git checkout cl-parser-combinators
git merge --no-ff step-b6-forms
```

## Record measurements

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"B6","lane":null,"outcome":"green","wall_seconds":<measured>,"attempts":<n>,"verify":{"command":"make test-matrix + compile-headers","exit_code":0,"wall_seconds":<measured>,"log_bytes":$(wc -c < /tmp/verify-B6-after.log),"summary_lines_read":<n>},"diff":{"files_changed":<n>,"insertions":<n>,"deletions":<n>},"out_of_scope":[],"note":""}
EOF
```

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-b6-forms
```

Mark B6 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff

Read `tmp/plan/step-B7.md`, then write `tmp/plan/handoff-B7.md` (≤ ~150 lines).
B7 converts `read_sharpsign` and `read_node` — the readtable dispatch and the
last unconverted functions. Tell it: whether the primitive set stopped growing
and what you resolved B2's provisional note to; how you handled the recursion
through the `read_node` forward declaration, since B7 owns the other end of it;
and what `read_wrapped` needed of the branch position, because B7's dispatch
passes `where` into it.
