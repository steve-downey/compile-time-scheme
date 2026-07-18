# DIV-0001: single package, keywords only, uppercase-fold-only reader

- **Status:** accepted-permanent
- **Date:** 2026-07-18
- **Step:** plan-time (decision records D2 and D7 of docs/cl-pivot-plan.md)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

ANSI CL defines a package system (`defpackage`, `in-package`, package markers `pkg:sym` / `pkg::sym`, `COMMON-LISP` and `COMMON-LISP-USER` packages) and a configurable readtable case (`:upcase`, `:downcase`, `:preserve`, `:invert`) with `|…|` and `\` escapes.
`smdlisp` implements a single implicit package plus the `KEYWORD` package for `:foo` keywords, and unconditionally folds unescaped symbol names to uppercase with no escape syntax.

## Why

The package system is orthogonal to the project thesis (one-shot control flow over CPS and sender backends) and would dominate the reader and environment budget without illuminating anything about compile-time compilation.
Uppercase folding matches the ANSI default readtable, so all standard-looking code reads correctly.

## Consequences

- Symbols compare by folded name only; `foo`, `Foo`, and `FOO` are the same symbol (steps L5, L9).
- No symbol can contain lowercase letters or package markers; tests must not rely on either.
- The limitations doc (step L24) must list this divergence.

## Revisit condition

None.
Accepted as permanent scope for the proof of concept.
