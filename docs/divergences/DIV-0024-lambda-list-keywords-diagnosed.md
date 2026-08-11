# DIV-0024: lambda-list keywords (`&optional`, `&rest`, ...) are diagnosed, not bound

- **Status:** open
- **Date:** 2026-08-11
- **Step:** R6 (conformance corpus and differential oracle); discovered and
  flagged by R5 (`src/smd/cl/eval`)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

ANSI lambda lists support `&optional`, `&rest`, `&key`, `&aux` and more
(ANSI 3.4.1). `elaborator::read_lambda_list`
(`src/smd/cl/elaborator/elaborate.hpp`) accepts only required parameters:
any parameter name starting with `&` is diagnosed as "lambda list keywords
are not yet supported" rather than silently bound as an ordinary parameter
named e.g. `&optional`.

## Why

Decision D19, and the elaborator's own comment beside `read_lambda_list`:
binding `&optional` as an ordinary parameter name would be the kind of
silent misreading D19 rules out — a caller writing `(defun f (x &optional
y) ...)` and getting a two-required-parameter function with a parameter
literally named `&optional` is worse than a diagnosed error. This is a
deliberate, decision-backed scope limit, not an oversight: required
parameters only is what R4/R5 built, and the diagnostic exists specifically
so the gap fails loud rather than silent.

## Consequences

- No corpus entry may use `&optional`, `&rest`, `&key`, or `&aux` in a
  lambda list and expect binding; `src/smd/cl/conformance/corpus.hpp`
  contains none.
- `data-and-control-flow/defun.lsp`'s `defun.5`/`defun.6`/`defun.7`
  (`ansi-test`, `&aux`/`&optional`/`&key` respectively) were not adaptable
  for this reason, among others (they also use `declare (special ...)`,
  itself out of scope).

## Revisit condition

Closed when a step extends `read_lambda_list` to recognize and bind at
least one lambda-list keyword, which is its own scoped piece of work
(binding semantics, not just recognition — `&optional`'s default-value
forms are themselves expressions needing evaluation in the right
environment).
