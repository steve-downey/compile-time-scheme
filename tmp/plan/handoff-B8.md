# Handoff to B8

## The `and_then` grep, since your part 5 write-up wants it exactly

`grep -rn 'and_then(' src/smd/cl/reader/` finds **seven call sites** after B7,
all correct and none of them a parser-layer holdout:

- `detail/forms.hpp` × 3 — all inside `read_delimited`, which no step owned.
- `detail/text.hpp` × 4 — `read_string` × 3, `read_character` × 1.

Every one is `result`'s own bind wrapped around `add_branch_checked`,
`add_leaf_checked` or a capacity check. Appending to the tree is not a parse
step. `read.hpp` has none: `read_datum` and `read` are on the parser layer, and
the single `result` bind `read` still needs is spelled `bind`, not `and_then`.

Counting mentions rather than calls gives 16, because `read_context.hpp`'s doc
prose and the `using` declaration match too. Use `and_then(` if you quote a
number.

## The final primitive set, and which step landed each

`pure`, `map`, `bind` (B2); `satisfy`, `char_p`, `skip_many` (B3);
`operator|`/`alt`, `optional`, `many_until` (B4). **Nothing was added after
B4.** B5, B6 and B7 each converted reader functions and added none — three
independent consumers in a row, which is the evidence the provisional note in
`docs/compiler_architecture.org` § B2 was waiting for. It is resolved there.

B5's standing request — a position parameter on `satisfy` — is **still
outstanding and still unjustified**, and B7 is the caller that was predicted to
force it. It did not. The infix numeric argument of `#nnR` cannot fail: absence
of a digit is a value, so `optional` converts `satisfy`'s cursor-positioned
failure into the `std::nullopt` that `many_until` reads as "stop", and no
diagnostic B7 produces ever comes from `satisfy`. The readtable-query predicate
B5 also predicted was not needed either: `macro_of` and `sharp_of` are read
inside parsers that already have the context threaded to them.

The one genuinely open item is the one you own: `map` is still not a registered
`functor<parser<F>>`, which is DIV-0028's neighbourhood.

## `parser<F>` changed shape in B7, and it is the thing to check your numbers against

**A parser now *is* its callable.** `F` is a private base and its `operator()`
is re-exported, instead of a member `f_` and a forwarding `operator()`.

The reason is constant-evaluation stack, and it is the finding your D30 section
most wants. This reader's recursion is ordinary C++ recursion, so every reified
parser between `read_node` and its own recursive call costs
`-fconstexpr-depth`, which GCC caps at **512** by default and the project sets
nowhere. Measured on `src/smd/cl/printer/prin1.test.cpp`'s 64-deep nested-quote
`static_assert`, by bisecting `-fconstexpr-depth=N` on a `-fsyntax-only`
compile of that one TU:

- **424** — `cl-parser-combinators` before B7.
- **570** — B7's conversion with a forwarding `operator()`. Over the cap; the
  build stopped, which is how this was found rather than reasoned about.
- **< 380** — after re-exporting the call. Below where the series started,
  because the saving applies to every parser invocation B2–B6 placed too.

Your step file asks for `-fconstexpr-ops-limit` headroom. **Also measure
`-fconstexpr-depth`**, on `prin1.test.cpp` as well as `cl_reader_test` — depth
is the limit this series actually moved, ops was never the binding one, and
prin1 is the deepest constexpr recursion in the tree. Note that the before-tree
for depth is not the same question as the before-tree for wall time: depth
scales with source nesting, so quote the test and its nesting depth alongside
the number or it means nothing.

Per-TU wall time, `-fsyntax-only -O0`, same machine, before/after B7:
`read.test.cpp` 1.80 s → 1.94 s, `prin1.test.cpp` 1.70 s → 1.69 s. Noise-level.
Whole-matrix wall time was 198 s cold in B7's worktree. Take your own numbers;
these are only here so a dramatic result is recognisable as dramatic.

The argument-order guard is **not** lost by the reshape and you should not
report it as a cost. `parse_context` excludes exactly one type, `cursor`, and
every parser in the kit and in `cl` names its own context parameter
`parse_context`, a refinement of it, or a concrete type — verified by grep, no
site spells a bare `auto &ctx`. A cursor passed where a context belongs is the
same substitution failure with the same GCC diagnostic before and after;
checked both ways against the pre-change header.

## Every function now constrained by `reader_context`

`read_radix_number`, `read_string`, `read_character`, `read_wrapped`,
`read_token_datum`, `read_sharpsign`, `read_node` — plus `read_node`'s forward
declaration in `detail/read_node_fwd.hpp`, which **had** to gain the same
constraint or it declares a different template from the definition.

Still unconstrained: `read_delimited`'s own `Ctx` is a bare `class Ctx` (only
its inner step closure names the concept). Nobody owned that function.

**`detail/read_context.hpp`'s doc comment on the concept is now wronger than
when B5 and B6 flagged it.** It still says "Only `read_radix_number` is
constrained with this concept in this step". Seven functions are. It has been
outside every step's declared scope since B3; you are the integration step and
it is a one-line fix in a file you are not otherwise editing, so it is your call
whether to take it or to leave it for the review to note.

## Out-of-scope files B7 touched

- `src/smd/cl/reader/detail/read_node_fwd.hpp` — the constraint, above.
- `src/smd/kit/parser/parser.hpp` — the reshape, above. This was within B7's
  declared scope (`src/smd/kit/parser/*.hpp`, "only if a primitive needs
  extending") but it is a shape change rather than an addition, so treat it as
  the most consequential thing B7 did.
- `src/smd/kit/parser/parser.test.cpp` — **one new `TEST_CASE`** plus four
  `static_assert`s pinning the argument-order guard with `std::invocable`.

That new case moves the ctest entry count from **378** to **379 per leg**. Both
legs are green at 379. If your baseline shows 378 you are on the wrong commit.

## For your whole-series test diff (part 5's classification)

B7 added no case to any `src/smd/cl/**` test and changed no expectation
anywhere. `git diff --name-only cl-parser-combinators..HEAD` on B7's own branch
touched no `cl` test file at all. The `cl` test additions in the series are
B3's, B5's and B6's, exactly as your step file already lists them.

`read.test.cpp`'s `reports_errors` chain (22 `fails_with` sites) passes
unchanged, and B7 was the last step that could have broken any of the fourteen
messages it covers.

## The SBCL oracle is live and the counts to insist on

**SBCL 2.6.0.debian.** `cl_conformance_test "*ReaderDifferential*"` runs
**180 assertions in 3 test cases**; `"*Sbcl*"` runs **300 in 5**. Identical
before and after B7. The binaries are at
`.build/build-gcc-16/src/smd/cl/conformance/{Asan,Debug}/cl_conformance_test`
and `.build/build-gcc-16/src/smd/cl/reader/{Asan,Debug}/cl_reader_test` — if
your step file still globs `./.build/*/*/`, it resolves nothing.

Check the assertion count, never the exit status: the differential cases skip
when `sbcl` is absent and ctest scores a skip as a pass.

## Anchors B7 landed

- `b3055934-2efb-4df4-a4bb-e961e29e7d0f` — converted `read_sharpsign`,
  `src/smd/cl/reader/detail/sharpsign.hpp`.
- `b803edcd-959d-4364-94f8-13fb256e7b9b` — converted `read_node`,
  `src/smd/cl/reader/detail/node.hpp` (B1's anchor, kept in place around the
  rewritten function).

Both are transcluded from `docs/compiler_architecture.org` § B7.
`scripts/verify-transclusions.sh` exits 0 with the one pre-existing WARN on
`docs/blog/phase-12-cps.org`, which is not B7's and has been there all phase.

## `make lint` was not green on its first run

clang-format rewrote four of the files B7 had already edited. That is the
pre-authorized reformat, recorded here because it will do the same to you if
you write long doc comments by hand. Second run green, all hooks.
