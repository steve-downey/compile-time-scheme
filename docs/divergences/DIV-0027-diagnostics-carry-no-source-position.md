# DIV-0027: elaborator and evaluator diagnostics carry no source position

- **Status:** open
- **Date:** 2026-08-11
- **Step:** R6 (conformance corpus and differential oracle); discovered and
  flagged by R4 (`src/smd/cl/elaborator`) and R5 (`src/smd/cl/eval`)
- **Authority diverged from:** none (a quality gap, not an ANSI or plan rule)

## What diverged

Every `foundation::parse_error` the elaborator or the evaluator raises
carries a default-constructed `source_pos` — `{}`, meaning "no position" —
rather than a real line and column. `elaborator::elaborate_into` and every
`emit_*` helper in `src/smd/cl/elaborator/elaborate.hpp` write
`foundation::parse_error{{}, "..."}`, and every diagnostic in
`src/smd/cl/eval/machine.hpp` and `src/smd/cl/eval/builtin.hpp` does the
same. A corpus entry, or a user's program, that fails gets told *what* is
wrong but never *where*.

## Why

The datum tree itself carries no positions past the reader: `reader::
datum_tree` stores atoms and branch shapes, not the `source_pos` each atom
was read at (R4's step brief already recorded this as inherited from R3).
Every stage downstream of the reader — elaboration, evaluation — has
nothing to attach to a diagnosis. This is infrastructure debt rather than a
one-line fix: giving `foundation::tagged_tree` a position or span column
touches the reader (which would have to populate it), the elaborator's
three passes (which would have to propagate it through `traverse`,
`scan_down`, and `para_short`), and the evaluator (which reads core nodes,
not datum nodes, so it would need its own propagation).

## Consequences

- A conformance corpus is exactly where this is first felt in practice:
  `src/smd/cl/conformance/corpus.hpp`'s `expect_error` cases can assert
  *that* a program is diagnosed, never *where*, because there is nothing
  to assert against.
- No test in this project should assert on `parse_error::where` being
  anything but the default `source_pos{}` for an elaborator- or
  evaluator-raised diagnostic; only the reader (which does thread
  `cursor::position()` through) has real positions to assert on today.

## Revisit condition

Closed when `foundation::tagged_tree` (or a parallel column keyed the same
way) carries a `source_pos` or `source_span` per node, populated by the
reader and read by the elaborator and evaluator when they raise a
diagnostic. Reader and foundation work, not elaborator- or evaluator-local.
