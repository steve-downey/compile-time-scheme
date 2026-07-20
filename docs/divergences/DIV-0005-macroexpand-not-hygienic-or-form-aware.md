# DIV-0005: macro expander is unhygienic and form-agnostic (generic datum walk, no gensym)

- **Status:** accepted-permanent
- **Date:** 2026-07-20
- **Step:** L17 (docs/cl-pivot-plan.md)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

Two related simplifications in `src/smd/smdlisp/macroexpand/expander.hpp`'s host macros (`cond`, `when`,
`unless`, `and`, `or`, `case`) and its whole-tree `expand()` walk, both scoped deliberately for this step:

1. **No macro hygiene (no gensym).** `or_expand` and `case_expand` bind a helper variable (`%OR-TMP`,
   `%CASE-KEY` respectively) via a synthesized `let`, to evaluate a shared subexpression exactly once. ANSI
   CL macros that need a fresh binding are expected to use `gensym` (or `gentemp`) to produce an uninterned
   symbol guaranteed not to collide with any symbol the macro's caller could have written. This project has no
   notion of uninterned/fresh symbols yet (that machinery does not exist until L18/L19's backquote and
   `defmacro` work, per the plan). `%OR-TMP`/`%CASE-KEY` are therefore fixed, ordinary (interned) symbol
   spellings, not gensyms: a user form that happened to reference a variable literally named `%OR-TMP` or
   `%CASE-KEY` inside the relevant macro's arguments could observe unintended variable capture.

2. **`expand()`'s whole-tree walk does not know special-form argument shapes.** It recognizes exactly one
   special case (a list headed by the symbol `QUOTE` is never walked, since its argument is literal data),
   and otherwise recurses into every list's elements uniformly, including a `lambda` form's parameter list or
   a `let`/`let*` binding list. If a program reuses one of the six macro names as an ordinary variable or
   parameter name (e.g. `(lambda (cond) cond)`), the walk may misinterpret that unrelated list (e.g. a
   zero/one-element parameter list literally spelled `(cond)`) as an attempted macro call, and the relevant
   macro's own argument-count validation may then reject it with a spurious error.

## Why

Both are genuine scope cuts, not bugs discovered by a failing test — no merge-criteria test for this step
exercises either case, and both are permitted (indeed expected) simplifications for a step whose explicit job
is to establish the macro-expansion mechanism and its API shape, not to build a full ANSI-conformant hygienic
macro system or a special-form-aware code walker.

A fully general fix for (2) would require the expander to carry a table of "argument extents" per special
form (which positions are code, which are binding/parameter lists, which are data) — essentially re-deriving
the elaborator's own special-form knowledge inside the macro-expansion pass. Decision D9 deliberately keeps
macro expansion independent of the elaborator; duplicating that knowledge here would work against that
separation for a case that, in practice, essentially never arises (real Common Lisp code does not name
variables after standard macros).

A fully general fix for (1) requires `gensym`, which the plan schedules for L18 (backquote) and L19
(`defmacro`), not L17.

## Consequences

- `or` and `case`'s expansions are not usable inside a user form that itself binds a variable literally named
  `%OR-TMP` or `%CASE-KEY` — an extremely narrow restriction, expected never to bite ordinary test or example
  code.
- `expand()`'s recursive walk should not be relied upon to correctly handle a program that names a variable,
  parameter, or `let`/`let*` binding after one of the six macro names (`cond`, `when`, `unless`, `and`, `or`,
  `case`).
- L18/L19 should revisit whether the temp-variable names above should become real gensyms once that machinery
  exists, rather than leaving this permanently accepted.
- No later step should assume `expand()` understands the argument shape of any special form; it only knows
  about `QUOTE`.

## Revisit condition

Revisit the hygiene half (1) once L18/L19 land real gensym support — replacing the fixed temp-variable
spellings with fresh uninterned names would close that gap entirely.
The form-agnostic-walk half (2) has no scheduled revisit: a fully special-form-aware code walker is a much
larger undertaking than this project's scope calls for, and the risk it guards against (a program naming a
variable after a macro) is accepted as permanent.
