# Handoff to A4

A3 merged into `cl-retire-trees` (`--no-ff`, merge commit `03f3781`, on top of
A2's `6e341df`). `src/smd/smdscheme/` and `src/smd/smdlisp/` no longer exist
in the worktree; both are frozen at annotated tags.

## ctest count and wall time

`make test-matrix`, both legs, went from **1105 to 304** ctest entries across
A3's deletion, both legs still `100% tests passed`. That is the number your
own step file's baseline-verify step should reproduce.

**I do not have a genuinely cold post-deletion `make test-matrix` wall time to
give you.** My worktree inherited a `.build/` already warmed by the prior
(blocked) A3 attempt's baseline run, so every `make test-matrix` I ran after
the deletion was an incremental rebuild (~28s total, both legs, dominated by
ctest reporting rather than compilation) — not the cold-worktree figure your
step file expects to size A4/A5 against. Treat the first `make test-matrix`
you run in your fresh `step-a4-cl-printer` worktree as the real cold number
for this tree shape; it will be your baseline-verify step's own measurement,
not something I could hand you.

## The `git show` incantation for reading a file out of a tag

```sh
git show iteration/smdlisp-final:<repo-relative-path>
git show iteration/smdscheme-final:<repo-relative-path>
```

Confirmed working post-deletion: `git show
iteration/smdlisp-final:src/smd/smdlisp/smdlisp.hpp` prints the file. Note
`smdlisp` never had a printer of its own to compare against — grepping
`iteration/smdlisp-final` and `iteration/smdscheme-final` for `prin1` or
`print_datum` before you start may save you a dead-end look; I did not do
that grep myself, only confirmed the tags resolve.

## The confirmed survivor list inside `src/smd/cl/`

`grep -rn "smdlisp\|smdscheme" src/smd/cl/` still returns exactly the same
nine comment-prose lines A2's handoff named (unchanged by A3, since A3's own
part 1 confirmed A2's list still holds before deleting anything):

- `src/smd/cl/foundation/source_pos.hpp:6`
- `src/smd/cl/sender/sender_v.hpp:13` and `:15`
- `src/smd/cl/foundation/static_vector.hpp:6`
- `src/smd/cl/foundation/alternative.hpp:6`
- `src/smd/cl/foundation/arena_box.hpp:6`
- `src/smd/cl/foundation/source_span.hpp:6`
- `src/smd/cl/foundation/functor.hpp:6`
- `src/smd/cl/foundation/applicative.hpp:6`

All are historical prose ("once `cl` became a second real client of the same
generic typeclass framework `smd::smdscheme` and `smd::forth` apply to..."),
none are includes, qualified names, or targets, and none are yours to fix —
`AGENTS.md`'s own rule says leave them. `src/smd/cl/printer/` is new territory
with nothing to inherit from either dead tree except by reading a tag, which
the `git show` incantation above covers.

## `docs/mrdocs.yml` — your own step file already knows this, confirming it

Its `input:` list is now the seven live roots (`kit/foundation`,
`cl/foundation`, `cl/symbol`, `cl/reader`, `cl/core`, `cl/elaborator`,
`cl/eval`) — no `cl/printer` yet, no `cl/sender` (excluded, matching the old
`smdscheme` config's own exclusion of sender/reflection). Your step file
already says to add `../src/smd/cl/printer` after this step merges; do that
in `docs/mrdocs.yml`'s `input:` list and, if you want it documented, extend
`docs/antora.yml`'s `using-namespaces` with `smd::cl::printer` — A3's own
declared scope didn't list either file for you, and it doesn't need to,
since it's your printer, but nothing stops you from adding two lines each.

**`mrdocs` is not installed here** (no `.tools/`, not on `PATH`), so `make
docs` cannot be verified locally — I verified `docs/mrdocs.yml`'s config only
by confirming every `input:` path exists on disk. Same limitation carries
forward to whatever you add.

## `docs/modules/ROOT/pages/{architecture,index}.adoc`

Both were rewritten against `cl`'s pipeline (reader / core / elaborator /
eval / sender, plus symbol and foundation) with no printer section, since
`cl` had no printer before this step. `index.adoc`'s Quick Example is now
`smd::cl::conformance::evaluate_program("(+ 1 2)")`, copied from
`conformance/driver.test.cpp`'s own `one_form_is_its_own_value` case (a real,
compiling, constexpr-evaluated call) — not hand-invented code. Neither page
mentions a printer; adding one is entirely optional and not part of your
declared file scope, which doesn't list either `.adoc` file.
