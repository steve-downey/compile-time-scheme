# Handoff to the integration review

B8 merged as `5665d8b` on `cl-parser-combinators`, with one follow-up commit
`8d15c70` correcting a composition claim in the D30 section (the twelve extra
translation units are the kit's six test files per configuration, not six
headers plus six tests; the branch was already published, so this is a normal
commit rather than an amend). Both matrix legs green at
379 ctest entries; the SBCL oracle runs 180/3 and 300/5 in both legs, checked
before and after. Not pushed — the orchestrator pushes.

## The D31 whole-series diff result, exactly

`git diff --stat d6ae364..633ee81 -- 'src/smd/cl/**/*.test.cpp' 'src/smd/cl/conformance/'`

```
 src/smd/cl/reader/read.test.cpp | 57 ++++++++++++++++++++++++++++++++++++++++-
 1 file changed, 56 insertions(+), 1 deletion(-)
```

**Every hunk is an addition. No expectation was modified. The conformance
corpus was not touched at all.** That is not an eyeball judgement: all 17
`fails_with(input, message)` pairs present at `d6ae364` were extracted and
compared as a set against the 21 present at `633ee81`. Zero removed, zero
changed, four added. The one deleted line is `fails_with("|abc",
"unterminated |");` becoming `... &&` so the conjunction chain could be
extended — same input, same message.

Attribution by `git blame`, matching the step file's prediction exactly:

| Addition | Commit | Step |
|---|---|---|
| `#\| unterminated` and `#\| outer #\| inner \|# still open` → `"unexpected end of input"` | `ab33b6f` | B3 |
| `#\` → `"expected character after #\\"`; `"abc\` → `"unterminated string"`; `string_capacity_boundary()` | `4bbd23b` | B5 |
| `wrapped_branch_error_sits_at_the_marker()` | `346988d` | B6 |

Each closed a diagnostic no test covered at all. No `defect` is pinned (D16
holds). **Nothing here needs reporting in bold; there is no earlier-step
defect to name.**

The whole series across all paths: 31 files, +3322/−666. Twenty-two files
added (`src/smd/kit/parser/` ×13, `src/smd/cl/reader/detail/` ×8,
`docs/backlog/BL-0008`), nine modified. **No `DIV-` file was created by any
of B1–B7**, so every reserved divergence number in Phase B went unused,
including B8's DIV-0042.

## The final primitive set, and which step landed each

Nine combinators in six headers under `src/smd/kit/parser/`:

| Primitive | Header | Step |
|---|---|---|
| `pure`, `map` | `parser.hpp` | B2 |
| `bind` (+ `fmap`, via `monad<parser<F>>`) | `parser_instances.hpp` | B2 |
| `satisfy`, `char_p` | `parser.hpp` | B3 |
| `skip_many` | `repeat.hpp` | B3 |
| `operator\|` / `alt`, `optional` | `choice.hpp` | B4 |
| `many_until` | `repeat.hpp` | B4 |

Plus `cursor`/`advance_while` (`cursor.hpp`, moved out of `cl` in B2) and the
`parse_context` concept with `no_context` (`parse_context.hpp`, B2).

**The set closed after B4.** B5, B6 and B7 each converted reader functions and
added nothing — three independent consumers in a row. The one standing request,
a position parameter on `satisfy`, is still outstanding and still unjustified:
B7 was predicted to force it and did not, because `#nnR`'s infix digit run
cannot fail.

## Amendments and blocks written during the run

**None.** No `amendment-NN.md` and no `blocked-NN.md` exists in `tmp/plan/`
for any of the fourteen steps.

## Part 5 — where the executed series diverged from `docs/cl-parser-scoping.md` § 4

The owner amends the note from this list; it is deliberately not folded in here.
§ 4's sketch is P1–P10 as corrected 2026-08-16. Executed was B1–B8 plus the
out-of-band `typeclass-resync`.

1. **The order inverted at the front.** The sketch builds the kit first (P1, P2)
   and splits `read.hpp` fourth (P4). Executed split first (B1) and built the
   kit second (B2). Splitting first is what gave every later step a file it
   alone owned; the sketch's order would have had B2–B4 diffing the kit against
   a `read.hpp` that was still about to move.
2. **P3 never existed as a step, but two of its three jobs happened inside B2.**
   `src/smd/cl/reader/cursor.hpp` did become an R8-style forwarding shim, and
   the duck-typed `Ctx` did become the named `reader_context` concept — both in
   B2, not in a step of their own.
3. **The D30 "before" measurement did not happen at P3, by design.** B8's step
   file takes it from the merge base instead, on the reasoning that git is more
   reliable than asking seven cleared agents to remember. Worth keeping: the
   depth measurement it produced is the one that mattered, and no step before
   B7 could have produced it.
4. **The split is eight headers, not eleven.** `read.hpp` retains the two public
   entry points and pulls in eight `detail/` headers.
5. **The kit is a Monad first, not an Applicative first.** P1 asks for "the
   applicative primitives, `empty`, and `bind`, with Functor, Applicative and
   Monad law tests". Executed landed `pure`/`map`/`bind` and a
   `monad<parser<F>>` registration. `parser<F>` satisfies neither `monad_impl`
   nor `monad_object`, and structurally cannot: `operator()` is a template over
   the threaded context, so the parsed value type is not a property of the
   parser type and no `value_type` can name it. `apply` is therefore absent from
   overload resolution rather than wrong.
6. **There is no `empty`, no Alternative instance, and no Alternative law
   tests.** P2 asks for them. Nothing needed an identity for choice.
7. **There is no `parser_ops` facade with a Monad tier.** P2 asks for one.
   Callers reach `bind` through the CPO, brought in by a `using` in
   `detail/read_context.hpp` because a CPO is an object and ADL will not find it
   from a `parser<F>` argument.
8. **`many`/`some` became `many_until`, and the naming is load-bearing.** P2
   asks for `many`/`some`. What landed owns no capacity and no container:
   `read_delimited` keeps its own capacity check and accumulator so its
   "too many elements" diagnostic keeps the exact position it had. The retired
   `many<Capacity>` truncated silently on both edges, which D31 forbids.
9. **The layer stopped growing after B4 — the plan's guess was right.** Three
   converting steps in a row added nothing.
10. **The fan-out never happened; the whole series ran serial.** P6–P8 were
    three parallel lanes (`token`, `text`, `forms`). Executed: B5 took `text`,
    B6 took `forms` *and* `token` together, and neither ran concurrently.
11. **`scan_token`/`classify_number` stayed out of scope, exactly as § 4
    promised.** DIV-0003 is structurally unbreakable rather than merely tested;
    `token_p` only lifts `scan_token`'s existing result into the parser
    vocabulary.
12. **One step the sketch has no counterpart for**: `typeclass-resync`,
    inserted 2026-09-10 to carry `main`'s typeclass rename onto the branch.
13. **B7 changed the shape of `parser<F>` itself, which no sketch step
    anticipated.** A parser now *is* its callable (`F` a private base with
    `operator()` re-exported) rather than holding one and forwarding. This was
    forced by constant-evaluation depth, not chosen. It is the most
    consequential single edit in the series.

## Two things the review should look at that are not defects

- **`src/smd/cl/reader/detail/read_context.hpp`'s doc comment on
  `reader_context` is stale.** It still says "Only `read_radix_number` is
  constrained with this concept in this step". Seven functions are, plus
  `read_node`'s forward declaration. B7's handoff offered this to B8 as a
  judgement call; B8's step file forbids it — "No source changes", enforced by a
  spot check requiring the `.hpp`/`.cpp` diff to be empty — so it was
  deliberately left. It has been outside every step's declared scope since B3.
- **`read_delimited` is the one reader function nobody owned.** Its `Ctx` is a
  bare `class Ctx`; only its inner step closure names `reader_context`. All
  seven `and_then(` call sites left in `src/smd/cl/reader/` are inside it
  (`detail/forms.hpp` ×3) and in `detail/text.hpp` (×4), and every one is
  `result`'s bind around a tree append or a capacity check, not a parse step.

## DIV-0028 classification

Left as **`process`**, deliberately. The README's taxonomy calls `process`
"about workflow or tooling, with no bearing on the object language", which is
exactly what DIV-0028 is — a statement about what R8 extracted into the kit.
Nothing observable in Common Lisp ever depended on it. `scope-decision` was the
only other candidate and it is about limits on what the project *implements*,
which this is not.

## Anchors

B8 landed none and needed none: it changed no source. Every UUID anchor in
`src/smd/kit/parser/` and `src/smd/cl/reader/` is transcluded from
`docs/compiler_architecture.org`, except `datum.hpp`'s `23ee18c4…`, which is
transcluded by the pinned `docs/blog/phase-26` post. Nothing is orphaned and
nothing is missing. `scripts/verify-transclusions.sh` exits 0 with one
pre-existing WARN on `docs/blog/phase-12-cps.org`, which predates this plan.

## Checklists

All fourteen plan lines in the repository root `checklist.md` are ticked, plus
`typeclass-resync`. No step merged without ticking its line.
