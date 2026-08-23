# Step B4 — choice and bounded repetition; `read_delimited` onto the combinator layer

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. One front end, `src/smd/cl/`, over one substrate, `src/smd/kit/`. B1
split the reader into `src/smd/cl/reader/detail/*.hpp`. B2 created
`smd::kit::parser` (`pure`, `map`, `bind`, threaded context) and converted
`read_radix_number`. B3 added `satisfy`, `char_p`, `skip_many` and converted
the two intertoken-space skippers.

**Integration branch: `cl-parser-combinators`.**

**Reserved for this step:** blog phase **42**, divergence number **DIV-0038**.

## Phase B's standing constraints

- No observable behaviour change: no new syntax, no changed diagnostic text, no
  different error position.
- DIV-0003 must not regress; `1+` reads as the symbol `1+`.
- `scan_token` and `classify_number` are out of scope for all of Phase B.
- Existing reader tests and the conformance corpus pass unchanged. Adding a
  case is fine; **changing an existing expectation is a halt**.

## Why, and what changed under this step since it was first written

**The D15 ladder this step was written to remove is already gone.** Since
this plan was first drafted, commit `93e4bc7` rewrote `read_delimited`'s
element step from the `if (!element.has_value()) { return element; }` ladder
D15 forbids into an `and_then` chain:

```cpp
auto const stepped = and_then(
    read_node(cur, ctx),
    [&](parse_state<int> const &element) -> foundation::result<cursor> {
        if (children.size() >= children.capacity()) {
            return foundation::parse_error{cur.position(), "too many elements"};
        }
        children.push_back(element.value);
        return element.rest;
    });
if (!stepped.has_value()) {
    return stepped.error();
}
cur = stepped.value();
```

That commit's own message explains, correctly, why the surviving
`stepped.has_value()` is not the ladder D15 rules out: it is the unfold's
loop-continuation test, not a short-circuit-on-error idiom — "a traversal
can only stop by asking the effect whether it has already failed … and
bind, which skips work but not iteration, cannot answer it." **Do not
re-litigate that; it is correct, and it is not this step's job to remove.**

**What this step still does, and it is real work, not nothing.** The element
step is a `bind` now, but it still lives inside a hand-written
`while (true)` loop carrying a comment claiming a "substrate generic
algorithm" exemption:

```cpp
typename Ctx::child_list children;
// Substrate generic algorithm: the reader's element unfold. The
// elements do not exist as a structure until this loop reads them,
// so this produces what traverse later consumes.
while (true) {
```

That comment is the same claim B3 found and rejected for `skip.hpp`'s two
loops, for the same reason: `docs/cpp-rules.md`'s exception is for the
substrate's own generic algorithms, and this loop is in the reader, not the
substrate. `93e4bc7`'s own commit message says as much about itself: "that
last test goes when the loop itself becomes monadic over a parser carrying
its own state, which is the combinator layer's work and not this one's." **This
step is that combinator-layer work.** The deliverable is not "remove the
ladder" — it is "replace the raw `while (true)` and its false exemption with
the bounded, diagnosing repetition this step builds," the same move B3 made
for the skippers, applied to the one loop B3 could not touch because the
combinator it needs did not exist yet.

The two traps below are exactly as sharp as they were before the ladder was
removed — they are about the *repetition's* behaviour at its two edges
(too many elements, and running out of input), not about the ladder.

## Two traps, and they are opposite

**The retired `many<Capacity>` truncates.** Read it:

```sh
git show iteration/smdscheme-final:src/smd/smdscheme/parser/alt.hpp
```

Its loop is `while (result.size() < Capacity)` and it then returns **success**.
An over-long input silently stops being collected. `read_delimited` does the
opposite: it checks `children.size() >= children.capacity()` and diagnoses
`"too many elements"` at the current position. A stock `many` would truncate,
then fail later somewhere else with the wrong message at the wrong position.

So the collecting repetition this step adds must **diagnose at capacity**, not
truncate. That is a different combinator from the retired one and it should be
named so nobody mistakes it — something like `many_bounded<Capacity>` that
carries a `char const*` message and a failure position, or a repetition
parameterised on what to do at capacity. Whichever you choose, its doc comment
must say why it is not the obvious `many`, and `docs/compiler_architecture.org`
must record it, because "we already have `many`" is the exact thought that
would undo it.

**`skip_many` from B3 succeeds on exhaustion; this one must not.** B3's
repetition treats running out of input as success, deliberately, to preserve
the unterminated-block-comment behaviour. `read_delimited` treats running out
of input as `"expected ')'"` at the current position. Two repetitions, opposite
answers to the same question, both correct for their caller. Do not unify them.

## Setup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-b4-delimited -b step-b4-delimited cl-parser-combinators
cd ../step-b4-delimited
git submodule update --init --recursive
```

## Verify GREEN baseline

```sh
make test-matrix > /tmp/verify-B4-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-B4-base.log
```

Not green ⇒ `blocked-B4.md`.

## What already exists that this step builds on

- `src/smd/kit/parser/parser.hpp` and `repeat.hpp` — the primitive set as B3
  left it. Your handoff carries the exact spellings and B2's parser-equality
  helper for law tests.
- `src/smd/cl/reader/detail/forms.hpp` — `read_wrapped` and `read_delimited`.
  **Only `read_delimited` is in scope**; `read_wrapped` is B6's.
- `src/smd/cl/reader/detail/read_node_fwd.hpp` — the `read_node` forward
  declaration `read_delimited` calls through.
- `src/smd/cl/reader/detail/read_context.hpp` — `reader_context`,
  `add_branch_checked`, and B2's `token_p`.
- `src/smd/cl/foundation/static_vector.hpp` — `child_list` is a
  `static_vector`; `size()`, `capacity()`, `push_back`.
- The ordered-choice semantics the retired layer used, at
  `git show iteration/smdscheme-final:src/smd/smdscheme/parser/parser.hpp`:
  `operator|` tries the left parser and falls through to the right **only if
  the left failed at the position it started**, so a partially-consumed
  alternative propagates its error rather than backtracking. Keep that rule; it
  is the right one and `read_node`'s dispatch in B7 depends on it.

## The change

### 1. Choice, in the kit

`src/smd/kit/parser/choice.hpp`, new: `operator|`, `alt` as its named alias,
and `optional`. Context-threaded like everything else. Alternative law tests,
`static_assert`ed: left identity of the failing parser, associativity of
choice, and the no-backtracking-after-consumption rule stated as a test rather
than only as a comment.

Assess the diagnostics while you are here. Hand-rolled parsers produce targeted
messages (`"expected ')'"`); a chain of alternatives drifts toward "expected
one of". `docs/cl-parser-scoping.md` § 5 asks for that assessment explicitly
and asks for it early rather than at the end. Record in your handoff and in
`docs/compiler_architecture.org` what `operator|` does to the error when both
alternatives fail — which position and which message survive — because B7
builds the readtable dispatch out of it and B7's messages are pinned by tests.

### 2. Bounded collection, in the kit

Add the diagnosing repetition to `src/smd/kit/parser/repeat.hpp` beside
`skip_many`, with the capacity behaviour described above. Its law tests should
include the one that matters: at capacity **plus one** it fails, with the
supplied message, at the position of the element that would not fit.

### 3. Convert `read_delimited`

Same signature, same behaviour, same three diagnostics at the same three
positions:

- `"expected ')'"` at `cur.position()` when input runs out;
- `"too many elements"` at `cur.position()` when the child list is full;
- `"datum tree full"` from `add_branch_checked`, unchanged.

And the same success: a branch of the given kind whose children are the
elements in source order, with the cursor after the closing delimiter.

The loop becomes: repeat, bounded and diagnosing, of "skip intertoken space,
then either the closing delimiter — which ends the repetition — or one
`read_node`". What disappears into the repetition is not a ladder — `93e4bc7`
already turned the element step into the `and_then`/`bind` chain quoted
above — it is the raw `while (true)` and the "substrate generic algorithm"
comment over it, which was never true of a loop in the reader. Build the new
repetition's element step **from** that existing `bind` rather than
rewriting it from scratch; it is already the right shape; it is only the
loop wrapped around it that this step replaces. Constrain the function with
`reader_context` while you are in it, as B2's pattern establishes.

Watch the element order. `docs/cpp-rules.md` is explicit that invariant **I2** —
leaf index order is left-to-right source order — holds only because the reader
builds children left to right, and no type checks it. The repetition must
preserve that; a collection built in any other order would pass most tests and
silently change what `fold_map` and `traverse` mean over the tree.

### 4. Anchors and the architecture doc

Anchor the converted `read_delimited` and the diagnosing repetition — this is
the step's centre and the post's. Update the `smd::kit::parser` section in
place: the primitive set grows again; the two repetitions differ deliberately
and why; the choice diagnostics assessment from part 1.

Also record, at the anchor over `read_delimited`, that the function is now
fully onto the combinator layer — the `bind` `93e4bc7` already gave its
element step, plus the repetition this step gives its loop — and that D15's
ladder had already left the function before this step touched it; this
step's contribution is removing the last raw loop and its inaccurate
exemption claim, not the ladder. Then check:
`grep -rn 'has_value()) {' src/smd/cl/reader/` and say in your handoff
whether any remain, and in which function, so the step that owns each one
knows.

## Declared file scope

```
src/smd/kit/parser/choice.hpp              (new)
src/smd/kit/parser/choice.test.cpp         (new)
src/smd/kit/parser/repeat.hpp              (bounded collecting repetition)
src/smd/kit/parser/repeat.test.cpp         (its law tests)
src/smd/kit/parser/CMakeLists.txt          (FILE_SET + test source)
src/smd/cl/reader/detail/forms.hpp         (read_delimited only)
docs/compiler_architecture.org             (in-place, at the kit anchor)
```

## Verify GREEN after

```sh
make test-matrix > /tmp/verify-B4-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-B4-after.log
wc -c /tmp/verify-B4-after.log
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
grep -cE '\bwhile\b|\bfor\b' src/smd/cl/reader/detail/forms.hpp    # expect 0
grep -n 'Substrate generic algorithm' src/smd/cl/reader/detail/forms.hpp   # expect 0
grep -n 'has_value()) {' src/smd/cl/reader/detail/forms.hpp     # expect 0 (already 0 before this step)
grep -n 'too many elements' src/smd/cl/reader/detail/forms.hpp src/smd/kit/parser/repeat.hpp
grep -n "expected ')'" src/smd/cl/reader/detail/forms.hpp
```

The diagnostic strings must still exist and still be produced at the same
positions. `read.test.cpp` pins `fails_with("(1 2", "expected ')'")`; the
over-long-list case is pinned too — find it and check it by name rather than
trusting the suite count.

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
cl: read_delimited's last raw loop becomes a repetition

read_delimited's D15 ladder -- if (!element.has_value()) return
element; -- was already gone before this step: 93e4bc7 turned the
element step into an and_then chain, leaving one has_value() as the
unfold's loop-continuation test rather than the short-circuit-on-error
idiom D15 forbids. That commit said as much of itself: the remaining
raw while loop is the combinator layer's work, not that one's. This
step is that work.

The loop carried a "substrate generic algorithm" comment claiming an
exemption docs/cpp-rules.md grants only to the substrate's own generic
algorithms, from code that lives in the reader. B3 found and rejected
the same claim over skip.hpp's two loops; this is the one loop B3
could not touch because the repetition it needed did not exist yet.
It does now: the existing bind-based element step slots into a
bounded, diagnosing repetition, and the while loop and its comment
both go.

The repetition is deliberately not the obvious one. The retired layer's
many<Capacity> stops at capacity and succeeds, which would silently
truncate an over-long list and then fail somewhere else with the wrong
message; read_delimited diagnoses "too many elements" at the position of
the element that would not fit, and the new combinator does the same.
It is named so nobody reaches for it thinking it is many.

That makes two repetitions in the kit with opposite answers to running
out of input -- B3's skip_many succeeds, so an unterminated block
comment keeps consuming, and this one fails, so an unclosed list is
diagnosed where it goes wrong. Both are right for their caller and they
should not be unified.
EOF

git checkout cl-parser-combinators
git merge --no-ff step-b4-delimited
```

## Record measurements

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"B4","lane":null,"outcome":"green","wall_seconds":<measured>,"attempts":<n>,"verify":{"command":"make test-matrix + compile-headers","exit_code":0,"wall_seconds":<measured>,"log_bytes":$(wc -c < /tmp/verify-B4-after.log),"summary_lines_read":<n>},"diff":{"files_changed":<n>,"insertions":<n>,"deletions":<n>},"out_of_scope":[],"note":""}
EOF
```

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-b4-delimited
```

Mark B4 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff

Read `tmp/plan/step-B5.md`, then write `tmp/plan/handoff-B5.md` (≤ ~150 lines).
B5 converts `read_string` and `read_character`. Tell it: the spelling of the
choice and bounded-repetition combinators; what `operator|` does to the error
when both alternatives fail; which `if (!…has_value())` sites remain in the
reader and where; and whether the bounded repetition's capacity message ended
up as a constructor argument or something else, since B5's string accumulator
has the same "too long" shape.
