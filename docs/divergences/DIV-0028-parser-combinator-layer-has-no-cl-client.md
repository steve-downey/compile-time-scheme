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

## 2026-09-11: the revisit condition fired, and firing it dissolved the record (step B8)

The revisit condition above named two triggers.
The first one fired: steps B1–B7 of the parser-combinator plan rewrote `src/smd/cl/reader/` onto a `parser<F>`-shaped combinator layer, `smd::kit::parser`, built in `src/smd/kit/parser/` by B2 and finished by B4.
`src/smd/cl/reader/cursor.hpp` is now a forwarding shim onto `smd::kit::parser::cursor`, and every reader function that parses — `read_radix_number`, `read_string`, `read_character`, `read_wrapped`, `read_token_datum`, `read_sharpsign` and `read_node` — is composed from the layer's nine combinators.

Satisfying the condition showed that the condition was framed wrongly, which is a better outcome than satisfying it cleanly would have been.

The framing was "making `cl` a genuine **third** client", and it counted to three because at the time this record was written the repository had three front ends.
Phase A deleted two of them.
`src/smd/smdscheme/**` and `src/smd/smdlisp/**` were removed from trunk at step A3 under decision D32 and are preserved at the annotated tags `iteration/smdscheme-final` and `iteration/smdlisp-final`.
So `smd::kit::parser` has exactly **one** client, and the fourth-front-end trigger has no prospect of firing either.

One client is what makes the layer worth having, not a shortfall against it.
R8's lesson, which this record was the evidence for, was that extracting a module for its own sake produces something with no callers; the answer to that turned out not to be "wait for more clients" but "have one real one".
The two-independent-copies standard the `foundation/` half was held to was the right standard for a *move* — nine files that already existed twice — and is the wrong standard for a *design*, which is what `smd::kit::parser` is: it was written in B2–B4 against a consumer that arrived in the same step as each primitive, and no primitive was added without one.
The set closed after B4 and three further converting steps added nothing to it.

This is a **dissolution, not a closure**.
The record did not stop being true by being fixed.
Every sentence in "What diverged" was accurate when written and is still accurate about the code it describes.
What changed is that the proposition itself — that a kit module needs a count of clients before it earns its place — stopped being a meaningful statement about this repository once the repository had one front end.
There is nothing left here to close, revisit, or re-verify.

The one thing this record did decide that still stands: `functor<parser<F>>` is deliberately unregistered.
An instance whose only justification is that the typeclass exists is the over-eagerness this record named, and seven converting steps produced no caller that needs the `fmap` CPO to find a parser.
See `docs/compiler_architecture.org` §B2 ("Settled, B8") and §B8.
