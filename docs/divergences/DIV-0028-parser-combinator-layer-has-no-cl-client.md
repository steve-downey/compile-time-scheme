# DIV-0028: the parser combinator layer has no `cl` client

- **Status:** accepted-permanent
- **Date:** 2026-08-15
- **Step:** R8 (`docs/cl-rebuild-plan.md`)
- **Authority diverged from:** `docs/cl-rebuild-plan.md` §5 ("The kit")

## What diverged

§5 names the kit's contents as `foundation/` (`static_vector`, `result`, `parse_error`, `source_pos`, `source_span`, `arena_box`, `functor`, `applicative`, `alternative`) plus `parser/` (`cursor`, `parser`, `alt`, `parser_ops`), and §6's R8 entry says extraction happens "once Common Lisp is the third working client."
That premise is true for `foundation/`: `src/smd/cl/foundation/` defines and uses all nine files, verified by grep across `reader`, `elaborator`, `eval`, `sender`, `symbol` and `conformance`.
It is not true for `parser/`.
`src/smd/cl/reader/cursor.hpp` is a hand-written, differently-shaped type: no `parser<F>` wrapper, no `alt` combinator, `parse_state` folded directly into the cursor rather than living beside a generic `parser` type.
Nowhere under `src/smd/cl/` defines or uses anything resembling `parser<T>`, `alt`, or `parser_ops`.
R8 therefore extracts only the nine `foundation/` files into `smd::kit::foundation` and leaves `parser/` entirely alone: undiffed, unmoved, not even copied.

## Why

R3 built a readtable-driven, hand-rolled recursive-descent reader instead of adopting the parser-combinator style `smdscheme` and `compile-time-forth` share.
Nothing in `docs/cl-rebuild-plan.md`, the R3 step brief, or `docs/compiler_architecture.org` records that choice as a deliberate rejection of the combinator style; it is simply the shape R3's author chose, and no later step revisited it.
The consequence for R8: even the diminished claim the R8 step brief anticipated — "cl is at least *a* working client, even if `smdscheme` and `forth` can't be re-pointed at the kit in this repository" — is false specifically for `parser/`.
Cl is not a client of the parser combinator layer at all, so extracting it now would produce a kit module with zero real callers anywhere in this repository (`smdscheme` cannot be repointed in place per `AGENTS.md`, and `compile-time-forth` is a separate repository this project can read but not modify).

## Consequences

- `docs/cl-rebuild-plan.md` §5 carries a dated correction (2026-08-15) rather than being rewritten, per the append-only convention already established for §5's own D15 correction.
- The kit R8 ships is `smd::kit::foundation` only, not `smd::kit::parser`. A future step that either rewrites `cl`'s reader onto the combinator style, or adds a fourth front end that needs `parser<T>`, is the trigger to revisit this; until then `parser/` stays exactly as it is in `smdscheme` and `forth`, undisturbed.
- This is not a defect in `cl`'s reader — the hand-rolled shape works, is tested, and is the oracle-verified behaviour R6 checks against. It is a gap in what §5's premise assumed would be true by the time R8 ran, now corrected against the actual code.

## Revisit condition

Either `src/smd/cl/reader/cursor.hpp` and its callers are rewritten onto a `parser<T>`-shaped combinator layer, making `cl` a genuine third client of `parser/`, or a fourth front end in this repository needs the same combinator abstraction independently of `cl`. Until one of those happens, `parser/` is not kit material by the same two-independent-copies standard the `foundation/` half was held to.
