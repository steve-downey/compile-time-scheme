# Step B3 — intertoken space and comments onto the layer

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. One front end, `src/smd/cl/`, over one substrate, `src/smd/kit/`. B1
split the reader into `src/smd/cl/reader/detail/*.hpp` plus an umbrella. B2
created `smd::kit::parser` with `pure`, `map` and `bind` over a threaded
context, and converted `read_radix_number` onto it.

**Integration branch: `cl-parser-combinators`.**

**Reserved for this step:** blog phase **41**, divergence number **DIV-0037**.

## Phase B's standing constraints

- No observable behaviour change: no new syntax, no changed diagnostic text, no
  different error position.
- DIV-0003 must not regress; `1+` reads as the symbol `1+`.
- `scan_token` and `classify_number` are out of scope for all of Phase B. A
  parser may call them; neither may be rewritten.
- Existing reader tests and the conformance corpus pass unchanged. Adding a
  case is fine; **changing an existing expectation is a halt**.

## Why this step, and why now rather than in a lane

`src/smd/cl/reader/detail/skip.hpp` holds the reader's two remaining raw
loops. `skip_block_comment` is a `while` with a nesting depth and
two-character lookahead; `skip_intertoken_space` is a `while (true)` iterating
whitespace, `;` line comments and `#|…|#` block comments to a fixpoint. Both
carry a comment calling themselves "substrate generic algorithm", which is the
exception `docs/cpp-rules.md` grants — "a raw `for`/`while` loop is permitted
only inside the substrate's own generic algorithms" — claimed by code that is
in the reader rather than in the substrate. The exception does not reach here,
and the honest fix is to build them from a repetition combinator that does live
in the substrate.

It happens before the conversions that follow because `read_delimited` calls
`skip_intertoken_space` on every iteration of its element loop. Converting the
callee while a peer step rewrites the caller is not a conflict a file split can
prevent, so this lands first and the callers meet a settled signature.

## One trap, and it is the whole risk of this step

**`skip_block_comment` currently lets an unterminated `#|` consume to end of
input, and leaves the caller to report.** Its loop condition is
`!cur.empty() && depth > 0`, so running out of input exits the loop and returns
a cursor at end of input; the caller's end-of-input handling then produces
`"unexpected end of input"` at the caller's position.

The natural recursive combinator does not do that. It fails *at the comment*,
which is a better diagnostic and is exactly what D31 forbids: this series does
not get to improve behaviour in passing. `read.test.cpp` pins the current
message, and a better message is still a changed message.

So the converted `skip_block_comment` must still return a cursor at end of
input for an unterminated comment, not a failure. Build it from a repetition
combinator whose exhaustion is success, not from one whose exhaustion is
failure, and put a comment at the site saying that the weaker behaviour is
deliberate and named by D31 — otherwise a later reader will "fix" it.

## Setup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-b3-skip-combinators -b step-b3-skip-combinators cl-parser-combinators
cd ../step-b3-skip-combinators
git submodule update --init --recursive
```

## Verify GREEN baseline

```sh
make test-matrix > /tmp/verify-B3-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-B3-base.log
```

Not green ⇒ `blocked-B3.md`.

## What already exists that this step builds on

- `src/smd/kit/parser/parser.hpp` — `parser<F>`, `parse_result<T>`,
  `parser_like`, `pure`, `map`, `bind`. Your handoff carries the exact
  spellings and the parser-equality helper B2 wrote for its law tests; reuse
  that helper rather than writing a second one.
- `src/smd/kit/parser/parse_context.hpp` — the `parse_context` concept and
  `no_context`.
- `src/smd/cl/reader/detail/skip.hpp` — the two functions being converted,
  signatures `skip_block_comment(cursor) -> cursor` and
  `skip_intertoken_space(cursor, readtable const&) -> cursor`.
- `src/smd/cl/reader/readtable.hpp` — `is_whitespace`, `macro_of`
  (`macro_kind::semicolon`, `macro_kind::sharpsign`), `sharp_of`
  (`sharpsign_kind::block_comment`), `syntax_of`.
- `src/smd/kit/parser/cursor.hpp` — `advance_while(cursor, Pred) -> cursor`,
  which both functions already use and which is the substrate's real
  character-stepping primitive.
- `docs/compiler_architecture.org` § `smd::kit::parser` — B2's section,
  including its **provisional** note that the primitive set is `pure`/`map`/
  `bind` only because that is what its consumer needed. This step is the
  consumer that justifies extending it.

## The change

### 1. Three primitives in the kit

`src/smd/kit/parser/parser.hpp` gains `satisfy(pred, expected)` and
`char_p(c)`. Take their shape from
`git show iteration/smdscheme-final:src/smd/smdscheme/parser/parser.hpp`, and
add the context parameter — the retired versions were `(cursor)`-only.

`src/smd/kit/parser/repeat.hpp`, new, gains a **skipping** repetition:
run a parser repeatedly, discard every value, stop at the first failure, and
**succeed** with the cursor where it stopped. Name it for what it does
(`skip_many` reads well) and give it a doc comment saying explicitly that
exhaustion of input is success, because that is the property the block-comment
trap depends on and it is the opposite of what the collecting repetition B4
adds will do.

Do **not** add the collecting `many<Capacity>` here. It has no consumer until
B4, and B4 needs it to behave differently from the retired version anyway.

Law tests as before, `static_assert`ed: `skip_many(p)` never fails;
`skip_many(p)` at a position where `p` fails is `pure` of nothing at that same
position; `skip_many(skip_many(p))` is `skip_many(p)`. Add a bounded-progress
test — a parser that consumes nothing must not make `skip_many` loop forever;
decide whether the combinator guards against that or documents it as a
precondition, and say which in your handoff, because B4 hits the same question.

### 2. `readtable const` as the context

The two skippers take a `readtable const&`, not a reader context — the public
`read<>` entry point calls `skip_intertoken_space(state.rest, table)` with a
bare table and no context in hand. Rather than inventing a wrapper, thread the
readtable **as** the context: `readtable const` models `parse_context`, and the
parsers here are `parse_result<T>(cursor, readtable const&)`.

This is worth doing rather than working around, and it is the first evidence
that B2's threaded-context design is right: two different context types in the
same reader, each named in the signatures of the parsers that need it.

### 3. Convert the two functions, keeping their signatures

`skip_block_comment(cursor) -> cursor` and
`skip_intertoken_space(cursor, readtable const&) -> cursor` keep exactly those
signatures. Every caller — `read_delimited`, `read_node`, and the public
`read<>` — is unchanged by this step. What changes is that the bodies are built
from `satisfy`, `char_p` and `skip_many` instead of raw `while` loops, and that
the "substrate generic algorithm" comments come out, because the claim is no
longer being made.

`skip_intertoken_space`'s fixpoint over three alternating syntaxes is the
harder of the two and it needs choice, which the layer does not have until B4.
Express it with `skip_many` over a single parser that handles whichever of the
three applies at the current position — one parser with an internal decision,
rather than three parsers combined with a choice operator you do not have yet.
If that turns out to be genuinely worse than having choice, **that is an
amendment, not a workaround**: write `amendment-B3.md` proposing that
`operator|` move from B4 into B2's primitive set, and stop. Do not define a
local choice helper — never fork a shared abstraction.

Nesting in `skip_block_comment` stays a depth counter. Recursion is the
idiomatic combinator answer and it is the one that fails at the comment; the
counter is what preserves the consume-to-end-of-input behaviour D31 requires.

### 4. Anchors and the architecture doc

Anchor `skip_many` and the converted `skip_intertoken_space`. Update B2's
`smd::kit::parser` section in `docs/compiler_architecture.org` **in place**:
the primitive set is now `pure`/`map`/`bind`/`satisfy`/`char_p`/`skip_many`,
still provisional, still growing only when a consumer needs it. Record the
readtable-as-context finding there too — it is a durable fact and B4 through B7
will want it. Record the D31 block-comment constraint at the anchor over the
converted function, not only in a comment.

## Declared file scope

```
src/smd/kit/parser/parser.hpp              (satisfy, char_p)
src/smd/kit/parser/parser.test.cpp         (their law tests)
src/smd/kit/parser/repeat.hpp              (new; skip_many)
src/smd/kit/parser/repeat.test.cpp         (new)
src/smd/kit/parser/CMakeLists.txt          (FILE_SET + test source)
src/smd/cl/reader/detail/skip.hpp          (both functions converted)
docs/compiler_architecture.org             (in-place, at B2's anchor)
```

## Verify GREEN after

```sh
make test-matrix > /tmp/verify-B3-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-B3-after.log
wc -c /tmp/verify-B3-after.log
make compile-headers
make lint
./scripts/verify-transclusions.sh
```

## Spot checks

```sh
git diff --name-only cl-parser-combinators..HEAD | grep 'cl/.*\.test\.cpp$'
```

Must return nothing.

```sh
grep -cE '\bwhile\b|\bfor\b' src/smd/cl/reader/detail/skip.hpp    # expect 0
grep -n 'Substrate generic algorithm' src/smd/cl/reader/detail/skip.hpp   # expect 0
grep -n 'D31' src/smd/cl/reader/detail/skip.hpp                   # the trap, named
grep -n 'skips_comments_and_whitespace' -A8 src/smd/cl/reader/read.test.cpp
```

And confirm the unterminated-comment behaviour by hand, because it is this
step's one real risk:

```sh
./.build/*/*/cl_reader_test "*Errors*" 2>/dev/null | tail -5
```

`read.test.cpp` pins `"   ; only a comment"` failing with
`"unexpected end of input"`. If an unterminated `#|` now reports anything else,
or reports at a different position, you have taken the improvement D31 forbids.

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
cl: the skipping layer is combinators, not two raw loops

skip.hpp held the reader's last two while loops, each with a comment
calling itself a substrate generic algorithm. That exception is real but
it belongs to the substrate, and these are in the reader. Building them
from a repetition combinator that does live in the substrate is the
honest version of the claim they were making.

It lands before the conversions that follow because read_delimited calls
skip_intertoken_space every time round its element loop, and converting
a callee while something else rewrites the caller is not a conflict a
file split can prevent.

The interesting constraint is that the combinator must be worse than the
obvious one. An unterminated #| currently runs to end of input and lets
the caller report; the recursive combinator fails at the comment
instead, which is a better diagnostic and is therefore forbidden -- D31
says this series changes no observed behaviour, and a better message is
a changed message. skip_many succeeds on exhaustion for exactly that
reason, and the reason is written at the call site so nobody fixes it.

The readtable threads as the context. The public read<> has a table and
no reader context, so rather than wrapping one, readtable const models
parse_context directly. Two context types in one reader is the first
evidence that threading rather than capturing was the right call.
EOF

git checkout cl-parser-combinators
git merge --no-ff step-b3-skip-combinators
```

## Record measurements

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"B3","lane":null,"outcome":"green","wall_seconds":<measured>,"attempts":<n>,"verify":{"command":"make test-matrix + compile-headers","exit_code":0,"wall_seconds":<measured>,"log_bytes":$(wc -c < /tmp/verify-B3-after.log),"summary_lines_read":<n>},"diff":{"files_changed":<n>,"insertions":<n>,"deletions":<n>},"out_of_scope":[],"note":""}
EOF
```

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-b3-skip-combinators
```

Mark B3 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff

Read `tmp/plan/step-B4.md`, then write `tmp/plan/handoff-B4.md` (≤ ~150 lines).
B4 adds choice and bounded collection and converts `read_delimited`. Tell it:
whether you needed choice and had to work around not having it, and how;
`skip_many`'s answer to the zero-consumption question, because the collecting
repetition faces it too; the exact signature the skippers kept; and whether
`readtable const` satisfying `parse_context` needed anything of the concept
that B2 did not put there.
