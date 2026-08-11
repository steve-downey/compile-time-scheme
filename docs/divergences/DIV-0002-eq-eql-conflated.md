# DIV-0002: `eq` and `eql` implemented identically

- **Status:** open
- **Date:** 2026-07-18
- **Step:** L8 (cons cells and list builtins)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

ANSI CL distinguishes `eq` (object identity, implementation-defined for
numbers and characters) from `eql` (identity, plus value equality for
numbers of the same type and characters of the same code).
`smd::smdlisp::closure::apply_prim` (`src/smd/smdlisp/closure/pairs.hpp`)
implements both as the same comparison: `pair_ref` compares by heap-location
identity, every other value alternative compares by `operator==`.

## Why

`smdlisp` has no characters, floats, ratios, or bignums yet (decision D10);
the only value kinds where `eq` and `eql` could differ under the full ANSI
rules — numbers and characters — do not exist as distinct types from what
`eq` already handles here.
The Scheme original (`smd::smdscheme::closure::apply_prim`) makes the same
simplification for `eq?`/`eqv?`, so this is adapted by copy rather than a
new decision.

## Consequences

- `(eq 1 1)` and `(eql 1 1)` both currently return `T`; if bignums or
  floats are added later (out of scope per D10, but tracked), integer `eq`
  may need to become pointer/tag identity while `eql` stays value equality,
  which would require separating the two implementations at that point.
- No later step should rely on `eq` and `eql` disagreeing on any two
  values; no test asserts a difference between them.
- The limitations doc (step L24) must list this divergence.

## Revisit condition

Revisit if/when a numeric tower beyond fixnums (floats, ratios, bignums) or
characters are added to `smdlisp`; until then this is accepted as scope
matching the current value model.

## 2026-08-11: still applies in the rebuild, for a different reason

`src/smd/cl/eval/builtin.hpp`'s `apply_builtin` (decision D12's evaluator,
step R5) implements `eq` and `eql` identically, exactly as `smdlisp` does —
but not for `smdlisp`'s reason. There, both were string comparison on a
symbol name. Here, `eq`'s identity is real (an interned id, a cell index, a
pool offset) and every number that exists is an immediate `int`, so `eq`
and `eql` compare the same `std::variant<...>` structurally either way;
there is nothing yet for which `eql`'s value-equality is weaker than `eq`'s
identity. Boxed numbers, when D19's later lanes add them, are what would
first separate the two. The classification in `docs/divergences/README.md`
(`scope-decision`) and the revisit condition above both still apply
unchanged; only the mechanism producing the coincidence is new.
`src/smd/cl/conformance/corpus.hpp`'s `nil.8` entry pins this behaviour, as
D16 permits for a `scope-decision`.
