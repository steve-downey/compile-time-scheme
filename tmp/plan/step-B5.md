# Step B5 — text: string literals and character literals

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. One front end, `src/smd/cl/`, over one substrate, `src/smd/kit/`. B1
split the reader into `src/smd/cl/reader/detail/*.hpp`. B2 created
`smd::kit::parser` with a threaded context and `bind`; B3 added `satisfy`,
`char_p` and `skip_many`; B4 added choice and a capacity-diagnosing repetition
and retired D15's ladder from `read_delimited`.

**Integration branch: `cl-parser-combinators`.**

**Reserved for this step:** blog phase **42**, divergence number **DIV-0038**.

## Phase B's standing constraints

- No observable behaviour change: no new syntax, no changed diagnostic text, no
  different error position.
- DIV-0003 must not regress; `1+` reads as the symbol `1+`.
- `scan_token` and `classify_number` are out of scope for all of Phase B.
- Existing reader tests and the conformance corpus pass unchanged. Adding a
  case is fine; **changing an existing expectation is a halt**.

## Why

`src/smd/cl/reader/detail/text.hpp` holds the two character-level readers, and
both are raw `while` loops with hand-managed state — the last of that shape in
the reader after B3 and B4. `read_string` accumulates into a fixed buffer with
an escape flag threaded by hand through each iteration; `read_character`
decides between a single character and a character name by two-character
lookahead and then slices the source by offset arithmetic.

They are also the two places where the reader has its own capacity diagnostic
(`"string too long"`) and its own table lookup (`lookup_char_name`), which
makes them a good test of whether B4's bounded repetition generalises past the
one caller it was written for. If it does not, that is an amendment worth
having early.

## Setup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-b5-text -b step-b5-text cl-parser-combinators
cd ../step-b5-text
git submodule update --init --recursive
```

## Verify GREEN baseline

```sh
make test-matrix > /tmp/verify-B5-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-B5-base.log
```

Not green ⇒ `blocked-B5.md`.

## What already exists that this step builds on

- `src/smd/kit/parser/{parser,repeat,choice}.hpp` — the primitive set as B4 left
  it. Your handoff carries the exact spellings, what `operator|` does to the
  error when both alternatives fail, and how the bounded repetition takes its
  capacity message.
- `src/smd/cl/reader/detail/text.hpp` — `read_string`, `named_char`,
  `named_chars` (ten entries), `lookup_char_name`, `read_character`.
- `src/smd/cl/reader/detail/read_context.hpp` — `reader_context`,
  `add_leaf_checked`, `token_p`.
- `src/smd/cl/reader/readtable.hpp` — `macro_of(c) == macro_kind::double_quote`,
  `syntax_of(c) == syntax_type::single_escape`, `syntax_type::constituent`.
- `src/smd/cl/reader/datum.hpp` — `datum_string` (fixed `storage` array plus
  `length`, `max_string_chars` = 128), `datum_character`.
- `src/smd/kit/parser/cursor.hpp` — `advance_while`.
- `docs/compiler_architecture.org` § `smd::kit::parser`, updated in place by
  B2–B4. Read that section, not the whole document.

## The change

### 1. `read_string`

Same signature, same behaviour, same two diagnostics at the same positions:
`"string too long"` and `"unterminated string"`, both at `where` — note that
both report at the **opening quote's** position, not at the current cursor, and
that is easy to lose in a rewrite.

The body is: repeat, bounded at `max_string_chars` and diagnosing, of "either
a single-escape followed by any character taken literally, or any character
that is not the closing quote"; then the closing quote; then
`add_leaf_checked`. Escape handling stops being a flag threaded through a loop
and becomes an alternative inside the repeated parser, which is the actual
simplification this step buys.

Two details the current code has that a rewrite drops silently:

- a single escape at end of input falls out of the loop and reports
  `"unterminated string"`, not a separate diagnostic;
- the length check is `>=` against `max_string_chars` **before** appending, so
  a string of exactly `max_string_chars` characters is accepted and one more
  is not. Check the off-by-one against `read.test.cpp` rather than reasoning
  about it.

### 2. `read_character`

Same signature, same diagnostics: `"expected character after #\\"` and
`"unknown character name"`, both at `where`.

The decision it makes is genuinely context-dependent — whether to read one
character or a name depends on whether the character *after* the first is a
constituent — so it is a `bind`, not a choice between two parsers. Write it
that way and say so at the site; `docs/cl-parser-scoping.md`'s D28 asks that
Applicative composition stay the default and bind be used only where the
grammar is really context-dependent, "because `lift2` says something stronger
about a parser than `and_then` does and that information should not be
discarded where it holds."

`lookup_char_name` and `named_chars` stay exactly as they are: a table and a
case-insensitive lookup over it, already written with `std::ranges`. They are
not parsers and converting them would be churn.

The name slice currently comes from offset arithmetic on
`cur.remaining().substr(...)`. If the repetition combinator can hand you the
consumed span directly, use it; if it cannot, keep the arithmetic rather than
extending the kit for one caller, and note in your handoff that a
consumed-span-returning repetition would have helped — that is evidence for a
later amendment, not a reason for one now.

### 3. Constrain and anchor

Constrain both functions with `reader_context`. Anchor the converted
`read_string`'s repetition and `read_character`'s `bind`. Update
`docs/compiler_architecture.org` § `smd::kit::parser` **in place** with
anything the kit gained or anything you found it lacking.

## Declared file scope

```
src/smd/cl/reader/detail/text.hpp      (read_string, read_character)
src/smd/kit/parser/*.hpp               (only if a primitive needs extending;
                                        see the amendment rule below)
src/smd/kit/parser/*.test.cpp          (law tests for any such extension)
docs/compiler_architecture.org         (in-place, at the kit anchor)
```

If a kit primitive turns out to be inadequate, **extend it in place** — never
add a parallel one. If extending it would change behaviour for `read_delimited`
or the skippers, that is an amendment: write `amendment-B5.md` and stop.

## Verify GREEN after

```sh
make test-matrix > /tmp/verify-B5-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-B5-after.log
wc -c /tmp/verify-B5-after.log
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
grep -cE '\bwhile\b|\bfor\b' src/smd/cl/reader/detail/text.hpp        # expect 0
grep -n 'string too long\|unterminated string' src/smd/cl/reader/detail/text.hpp
grep -n 'unknown character name\|expected character after' src/smd/cl/reader/detail/text.hpp
./.build/*/*/cl_reader_test "*String*" "*Character*" "*Errors*" 2>/dev/null | tail -6
```

The conformance differential is the other witness here, and it is the one that
catches a rendering-visible change: A4 and A5 compare `cl`'s printed strings
and characters against SBCL.

```sh
./.build/*/*/cl_conformance_test "*ReaderDifferential*" 2>/dev/null | tail -5
```

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
cl: strings and characters read by combinator

text.hpp held the reader's last two hand-managed loops. read_string
threaded an escape flag through each iteration of a while; read_character
did two-character lookahead and then sliced the source by offset
arithmetic.

Escape handling is the part that actually gets simpler. It stops being a
flag carried across iterations and becomes an alternative inside the
repeated parser: either a single escape and whatever follows it taken
literally, or any character that is not the closing quote.

read_character stays a bind rather than becoming a choice, because its
decision is genuinely context-dependent -- whether to read one character
or a name depends on the character after the first. D28 asks that
applicative composition stay the default and bind be reserved for where
the grammar really needs it, since lift2 says something stronger about a
parser than and_then does.

Both diagnostics still report at the opening delimiter rather than at
the cursor, which is the detail a rewrite loses quietly, and both are
still the same strings. named_chars and lookup_char_name are untouched:
a table and a lookup over it are not parsers.
EOF

git checkout cl-parser-combinators
git merge --no-ff step-b5-text
```

## Record measurements

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"B5","lane":null,"outcome":"green","wall_seconds":<measured>,"attempts":<n>,"verify":{"command":"make test-matrix + compile-headers","exit_code":0,"wall_seconds":<measured>,"log_bytes":$(wc -c < /tmp/verify-B5-after.log),"summary_lines_read":<n>},"diff":{"files_changed":<n>,"insertions":<n>,"deletions":<n>},"out_of_scope":[],"note":""}
EOF
```

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-b5-text
```

Mark B5 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff

Read `tmp/plan/step-B6.md`, then write `tmp/plan/handoff-B6.md` (≤ ~150 lines).
B6 converts `read_wrapped` and `read_token_datum`. Tell it: whether any kit
primitive needed extending and how; whether a consumed-span-returning
repetition would have helped; how you expressed "any character that is not X"
given `satisfy`; and anything about the reader-context concept that turned out
to be under- or over-specified now that four functions are constrained by it.
