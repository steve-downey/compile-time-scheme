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
