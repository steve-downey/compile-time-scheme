# DIV-0012: `source_literal` is copied into `smd::smdlisp`, not aliased from `smd::smdscheme`

- **Status:** open
- **Date:** 2026-07-26
- **Step:** L22 (public API, Godbolt, FFI parity)
- **Authority diverged from:** docs/cl-pivot-plan.md §9

## What diverged

The plan's L22 entry says "`source_literal` reuse".

`src/smd/smdlisp/smdlisp.hpp` instead defines its own
`smd::smdlisp::source_literal`, character-for-character identical to
`smd::smdscheme::source_literal`, rather than aliasing or `using`-declaring the
Scheme one.

## Why

`smd::smdscheme::source_literal` is not in a language-agnostic target.
It is defined in the *package* header `src/smd/smdscheme/smdscheme.hpp`, whose
only include is `smd/smdscheme/closure/closure_program.hpp` -- the whole Scheme
compiler.
Reusing it therefore means `smdlisp.smdlisp` linking `smdscheme.closure`
PUBLIC, which drags `smdscheme.elaborator`, `smd.fixpoint`, and
`smdscheme.closure`'s INTERFACE `-freflection` onto every consumer of the
Common Lisp public header.
That is a much larger coupling than decision D1's "smdlisp consumes
smdscheme's language-agnostic `foundation` and `parser` targets".

The clean fix -- move `source_literal` down into
`smd::smdscheme::foundation`, where reuse would be free and correct -- is not
available: `smdscheme` is frozen for edits under decision D1 because published
blog posts transclude live code from it by UUID anchor, and `source_literal`
sits *inside* anchor block `44cc988c-7353-43aa-a7d3-8840f92371a6`.

Copying is also the established practice in this subtree: nearly every
`smdlisp` component header opens with "Adapted by copy from
`smd::smdscheme::...`, named to mirror it directly."

## Consequences

- Two identical `source_literal` templates now exist. They are distinct types.
  A `smd::smdscheme::source_literal` cannot be passed where a
  `smd::smdlisp::source_literal` is expected, and vice versa. No code needs
  to; nothing takes one generically.
- Any later step that changes one must consider the other. In practice only
  `smdlisp`'s is changeable, since `smdscheme`'s is frozen.
- Step L24, which consolidates the Common Lisp layer into
  `docs/compiler_architecture.org`, should record the duplication there rather
  than leaving it only in this file.

## Revisit condition

Closed if the D1 freeze on `smd::smdscheme` is lifted (or narrowed to exclude
`source_literal`) and the type is moved to `smd::smdscheme::foundation`, at
which point both package headers can alias the one definition.
