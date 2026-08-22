# Step B7 — sharpsign dispatch and `read_node`

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. One front end, `src/smd/cl/`, over one substrate, `src/smd/kit/`. B1
split the reader into `src/smd/cl/reader/detail/*.hpp`; B2–B6 built
`smd::kit::parser` a primitive at a time and converted everything except the
two dispatchers.

**Integration branch: `cl-parser-combinators`.**

**Reserved for this step:** blog phase **44**, divergence number **DIV-0040**.

## Phase B's standing constraints

- No observable behaviour change: no new syntax, no changed diagnostic text, no
  different error position.
- DIV-0003 must not regress; `1+` reads as the symbol `1+`.
- `scan_token` and `classify_number` are out of scope for all of Phase B.
- Existing reader tests and the conformance corpus pass unchanged. Adding a
  case is fine; **changing an existing expectation is a halt**.

## Why

What is left is the reader's spine. `read_node` skips intertoken space and then
`switch (ctx.table.macro_of(cur.peek()))`; `read_sharpsign` reads an optional
infix numeric argument and then `switch (ctx.table.sharp_of(after.peek()))`.
Decision D19 is explicit that this lookup — "not character tests" — is the
reader's spine, and D19 also asks that the reader be shaped so
`set-macro-character` can later be a table lookup selecting among parsers.

One thing that looked like an obstacle to the whole change and is not: **the
readtable is the easy part.** Because a datum is an arena index, every branch of
both switches has the same return type, `result<parse_state<int>>`.
Same-typed alternatives are *easier* to combine than the heterogeneous `T` the
retired Scheme layer dealt with. A table selecting among same-typed parsers is
the structure D19 asked for anyway, and this step is where it becomes available
rather than merely imaginable.

That does not mean the switches should become a chain of `operator|`.
`operator|` tries alternatives in order; a readtable dispatch selects one by
lookup and then commits. Turning a selection into a search would change which
diagnostic surfaces when a branch fails, and D31 forbids that. Keep the
selection a selection; what changes is that each branch names a parser instead
of open-coding a call.

## Setup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-b7-dispatch -b step-b7-dispatch cl-parser-combinators
cd ../step-b7-dispatch
git submodule update --init --recursive
```

## Verify GREEN baseline

```sh
make test-matrix > /tmp/verify-B7-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-B7-base.log
```

Not green ⇒ `blocked-B7.md`.

## What already exists that this step builds on

- `src/smd/kit/parser/{parser,repeat,choice}.hpp` — the primitive set as B6
  left it. Your handoff says whether it stopped growing and what B2's
  provisional note was resolved to.
- `src/smd/cl/reader/detail/sharpsign.hpp` — `read_radix_number` (converted by
  B2; **do not touch**) and `read_sharpsign` (this step).
- `src/smd/cl/reader/detail/node.hpp` — `read_node`, inside UUID anchor
  `b803edcd-959d-4364-94f8-13fb256e7b9b`.
- `src/smd/cl/reader/detail/read_node_fwd.hpp` — the forward declaration
  `read_wrapped`, `read_delimited` and `read_sharpsign` all recurse through.
  B1's architecture-doc note explains the arrangement.
- `src/smd/cl/reader/read.hpp` — the umbrella, holding `read_datum<>` and
  `read<>`. Both use `detail::and_then`; both are in scope.
- `src/smd/cl/reader/readtable.hpp` — `macro_kind`, `sharpsign_kind`,
  `macro_of`, `sharp_of`, `syntax_of`.
- `docs/compiler_architecture.org` § `smd::kit::parser`.

## The change

### 1. `read_sharpsign`

Same signature, same behaviour, same five diagnostics at the same positions:
`"unexpected end of input after '#'"`, `"unexpected numeric argument after '#'"`,
`"sized #n(...) vectors not yet supported"`, `"radix must be between 2 and 36"`,
and `"unsupported '#' syntax"` — all at `where`, the `#`'s own position.

Two parts. The infix numeric argument is a digit run folded into an `int`,
capped at 999, absent signalled by `-1`; that is a small parser and should
become one. The dispatch stays a `switch` on `sharp_of`, with each arm naming a
parser rather than open-coding a call, and with the `reject_argument` guard kept
exactly as it is — it is a precondition on four of the seven arms and it
produces one of the pinned messages.

`read_radix_number` was converted in B2 and is called from four arms. Do not
change it. If its signature is awkward from here — for instance if it wants the
radix as a parser-time value rather than a function argument — that is worth a
sentence in your handoff, not a change.

### 2. `read_node`

Same signature, same behaviour, same three diagnostics: `"unexpected end of
input"` at the cursor after skipping, `"unexpected ')'"` at `where`, and
`"expected datum"` at `where`.

The `comma` arm has a lookahead — `,@` versus `,` — that is a two-way decision
on the next character, and the `semicolon` arm deliberately falls through to
`"expected datum"` because a semicolon is consumed as intertoken space before
the switch and so cannot be seen here. Keep the fallthrough and keep its
comment; it looks like a bug and is not.

The anchor `b803edcd-959d-4364-94f8-13fb256e7b9b` is around this function. Per
decision D21 you may move, split or re-place it as the code changes; the only
obligation is that `scripts/verify-transclusions.sh` is green at your merge.

### 3. The public entry points

`read_datum<>` and `read<>` in `src/smd/cl/reader/read.hpp` still spell
`detail::and_then`. Convert them to the layer for consistency, keeping
`"unexpected trailing input"` at `rest.position()` and keeping `read_datum`'s
contract that it returns the cursor after the datum so callers can read a
sequence — `read.test.cpp` has a sequential-read test that pins exactly that.

After this, `grep -rn 'and_then' src/smd/cl/reader/` should find nothing outside
the kit forwarding shims. Report the actual result in your handoff either way.

### 4. Constrain, anchor, and record

Constrain both functions with `reader_context`; that completes the set and
`reader_context` is no longer a guess about one caller. Anchor the converted
`read_node` dispatch.

Record in `docs/compiler_architecture.org`, in place at the kit section, the
D19 observation this step proves out: because a datum is an arena index every
dispatch arm has the same return type, so a future `set-macro-character` is a
table lookup selecting among same-typed parsers rather than a redesign. That is
a durable fact and it is the one the C-series of
`docs/cl-language-scoping.md` will build on.

## Declared file scope

```
src/smd/cl/reader/detail/sharpsign.hpp     (read_sharpsign only)
src/smd/cl/reader/detail/node.hpp          (read_node)
src/smd/cl/reader/read.hpp                 (read_datum, read)
src/smd/kit/parser/*.hpp                   (only if a primitive needs extending)
src/smd/kit/parser/*.test.cpp              (law tests for any such extension)
docs/compiler_architecture.org             (in-place, at the kit anchor)
```

## Verify GREEN after

```sh
make test-matrix > /tmp/verify-B7-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-B7-after.log
wc -c /tmp/verify-B7-after.log
make compile-headers
make lint
./scripts/verify-transclusions.sh
```

## Spot checks

```sh
git diff --name-only cl-parser-combinators..HEAD | grep '\.test\.cpp$' | grep -v kit/parser
```

Must return nothing: no `cl` test changed anywhere in Phase B.

```sh
grep -c 'fails_with' src/smd/cl/reader/read.test.cpp
grep -rn "unsupported '#' syntax\|radix must be between\|expected datum" src/smd/cl/reader/
./.build/*/*/cl_reader_test "*Errors*" "*Sharpsign*" "*Vector*" 2>/dev/null | tail -8
./.build/*/*/cl_conformance_test "*ReaderDifferential*" 2>/dev/null | tail -5
```

`read.test.cpp`'s `reports_errors` is a chain of `fails_with` covering fourteen
messages. Every one of them is now produced by converted code, and this is the
last step that can break any of them.

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
cl: the readtable dispatch, and the last two hand-written parsers

What was left was the spine. read_node switches on macro_of and
read_sharpsign on sharp_of, and D19 says that lookup, not a chain of
character tests, is what the reader is built around.

The readtable turned out to be the easy part of this whole change rather
than the obstacle. Because a datum is an arena index, every arm of both
switches already returns the same type, and same-typed alternatives are
easier to combine than the heterogeneous ones the retired Scheme layer
dealt with. A future set-macro-character is a table lookup selecting
among same-typed parsers, which is the shape D19 asked for and which is
now available rather than merely imaginable.

The switches stay switches. A readtable dispatch selects a branch by
lookup and commits; operator| searches. Turning a selection into a
search would change which diagnostic surfaces when a branch fails, and
D31 does not permit that. What changed is that each arm names a parser
instead of open-coding a call.

The semicolon arm still falls through to "expected datum" and still
carries the comment explaining that a semicolon is consumed as
intertoken space before the switch runs. It looks like a bug and it is
not, which is why the comment outlived the rewrite.
EOF

git checkout cl-parser-combinators
git merge --no-ff step-b7-dispatch
```

## Record measurements

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"B7","lane":null,"outcome":"green","wall_seconds":<measured>,"attempts":<n>,"verify":{"command":"make test-matrix + compile-headers","exit_code":0,"wall_seconds":<measured>,"log_bytes":$(wc -c < /tmp/verify-B7-after.log),"summary_lines_read":<n>},"diff":{"files_changed":<n>,"insertions":<n>,"deletions":<n>},"out_of_scope":[],"note":""}
EOF
```

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-b7-dispatch
```

Mark B7 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff

Read `tmp/plan/step-B8.md`, then write `tmp/plan/handoff-B8.md` (≤ ~150 lines).
B8 closes the series: the D30 compile-time measurement, DIV-0028's dissolution,
the architecture section and the project checklist. Tell it: the result of the
`and_then` grep; the final primitive set of `smd::kit::parser`; every function
now constrained by `reader_context`; and anything you noticed about compile
time across B1–B7, because D30 wants before-and-after numbers and B8 is the
step that has to produce them.
