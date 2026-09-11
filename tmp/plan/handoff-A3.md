# Handoff to A3

A2 merged into `cl-retire-trees` (`--no-ff`, merge commit `6e341df`). Both
`iteration/*` tags are untouched by A2 (A2 did not touch tags at all).

## The `smdlisp|smdscheme` grep over `src/smd/cl/`: prose-only, confirmed

```sh
grep -rn "smdlisp\|smdscheme" src/smd/cl/
```

returns exactly these nine lines, all comment prose — no `#include`, no
qualified name, no CMake target:

- `src/smd/cl/foundation/source_pos.hpp:6`
- `src/smd/cl/sender/sender_v.hpp:13`
- `src/smd/cl/sender/sender_v.hpp:15`
- `src/smd/cl/foundation/static_vector.hpp:6`
- `src/smd/cl/foundation/alternative.hpp:6`
- `src/smd/cl/foundation/arena_box.hpp:6`
- `src/smd/cl/foundation/source_span.hpp:6`
- `src/smd/cl/foundation/functor.hpp:6`
- `src/smd/cl/foundation/applicative.hpp:6`

This is the survivor list your own part 1 confirmation should reproduce
exactly (modulo anything a later step under `cl` might add). A2's own
declared scope did not touch any of these files — they were already
prose-only before A2 ran; A2's job was only `oracle_compare.test.cpp`, which
is now gone.

## `oracle_compare.test.cpp`: no genuinely-uncovered case found, nothing ported

All six `TEST_CASE`s' coverage was re-derived, not assumed. Two were exact
matches your own step file already named (`fixnum_of("0", ...)` in
`number.test.cpp`; the comment-inside-list arity check in
`skips_comments_and_whitespace`). The remaining ones checked myself:
`SymbolsFoldAlike`/`Keywords`/`QuoteFamily` all match named functions in
`read.test.cpp` (`reads_symbols_folded`, `whole_token_classification`,
`reads_keywords`, `reads_quote_family`) case-for-case. `Lists`' `"( 1  2 )"`
spacing variant isn't a literal string match anywhere but is a strict subset
of `reads_lists()`'s `"( a ( b c ) d )"`, which exercises the same
intertoken-space handling around parens and between elements.
`ErrorsAgree`'s `both_fail("(")` is not a distinct code path from
`fails_with("(1 2", "expected ')'")`: both hit the same `cur.empty()` check
at the top of `read_delimited`'s loop in `read.hpp` (~line 197), first
iteration vs. a later one. Nothing was ported into `read.test.cpp`.

## `make testinstall`: not a gate, exit code moved 2 -> 0

Baseline (pre-A2, on `cl-retire-trees`): exit 2, the pre-existing
`smd/smdscheme/closure/pairs.hpp: No such file or directory` fatal error,
unrelated to this plan.

Post-A2: exit **0**. Not because anything was fixed — `find_package` now
resolves against a package that no longer claims a `smd::smdscheme` target,
and with the six `add_installtest` calls and the helper removed there is
nothing left in `installtest/` to try to link `smd::smdscheme`, so it
configures and runs zero installed tests. Zero tests passing is not
coverage; do not read the exit-code improvement as anything having been
fixed. `docs/backlog/BL-0005-export-live-trees-and-turn-testinstall-green.md`
is still where real coverage gets added, after Phase B.

## The shrunk top-level `beman_install_library` TARGETS list

Now just:

```cmake
beman_install_library(schemepoc.schemepoc TARGETS smd.fixpoint NAMESPACE smd::)
```

(gersemi collapsed it to one line since it's short enough — that's its
formatting, not a hand edit.) `kit.foundation` and the `cl.*` targets remain
unexported; BL-0005 is where that gets scheduled.

## ctest count after A2

1123 -> 1105 on both `make test-matrix` legs (both still 100% passing):
eleven example tests plus seven `OracleCompareTest` `TEST_CASE`s (including
its own `HeaderIsIdempotent`) left, nothing replaced them. This is the
number your own step file's "roughly 17" estimate should reconcile against
— the exact figure is 18, not 17; `HeaderIsIdempotent` is the one case that
estimate likely missed.

## One thing A2 did not need, in case a spot check surprises you

A2's step file directed adding comments to `src/examples/CMakeLists.txt` and
`installtest/CMakeLists.txt` naming the dead trees by name. Doing so
literally would have made A2's own "must return nothing" spot check over
`src/examples/ installtest/ CMakeLists.txt` fail against itself. Both
comments were reworded to convey the same information without the literal
strings `smdscheme`/`smdlisp`, so the spot check holds as written. Not
something A3 needs to act on; noted only because it's the same class of
plan-arithmetic slip flagged in your own inbound handoff (A1's `grep -c`
count) and I don't want a future reader to think A2 skipped its own
spot check.
