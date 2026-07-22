# DIV-0006: host macros use reserved fixed temp names, not gensym

- **Status:** open
- **Date:** 2026-07-22
- **Step:** L17 (macro expander with host macros)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

`or`, `cond` (its test-only clause form), and `case` each need to evaluate
one sub-expression exactly once while both testing it and using its value
(`or`'s argument, `cond`'s bare-test clause, `case`'s keyform).
ANSI CL's reference expansions achieve this with a freshly generated,
uncapturable symbol (`gensym`).
`src/smd/smdlisp/macroexpand/expander.hpp` instead binds a single fixed,
reserved name per macro via an ordinary `let`: `%OR-TEMP`, `%COND-TMP`,
`%CASE-TMP`.
A user program that lexically binds or references a variable literally
spelled with one of these three names, inside the relevant macro's expansion
scope, can be shadowed by or interfere with the macro's own binding.

## Why

This project has no `gensym` and no backquote/template machinery yet
(decision D9: host macros land in L17, `defmacro` and the compile-time
evaluator needed for a real `gensym` arrive at L18/L19).
`%` and `-` are ordinary constituent characters (`src/smd/smdlisp/reader/cl_chars.hpp`'s
`is_constituent_char`), so a user program could in principle type one of
these three names verbatim; the reserved names were chosen only to make
accidental collision unlikely, not to make it impossible.
Nested uses of the same macro do not collide with each other: each `let`
introduces a fresh lexical scope, and a binding's initializer expression is
evaluated in the *enclosing* scope, before the new scope's own binding
exists, so `(or a (or b c))` and similar nestings resolve correctly by
ordinary lexical shadowing (traced by hand and confirmed by
`ExpanderTest - OrEndToEndSingleEvaluation` and the other end-to-end tests
in `expander.test.cpp`). The only real risk is a user identifier that
happens to collide with a reserved name inside the same macro's scope.

## Consequences

- `(let ((%or-temp 1)) (or nil %or-temp))` (and the analogous `cond`/`case`
  cases) is not guaranteed to behave per ANSI CL; the user's binding can be
  shadowed by the macro's internal one.
- No test in `expander.test.cpp` exercises this collision; all end-to-end
  tests use ordinary variable names.
- The limitations doc (step L24) must list this divergence.

## Revisit condition

Revisit once `defmacro`/backquote (L18-L19) bring real `gensym` machinery;
at that point `expand_or`/`expand_cond`/`expand_case` in
`src/smd/smdlisp/macroexpand/expander.hpp` should generate a fresh symbol
per expansion instead of reusing `%OR-TEMP`/`%COND-TMP`/`%CASE-TMP`.
