# DIV-0025: `lambda`, `#'f`, `funcall` and `apply` are not executable

- **Status:** open
- **Date:** 2026-08-11
- **Step:** R6 (conformance corpus and differential oracle); discovered and
  flagged by R5 (`src/smd/cl/eval`)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

ANSI treats functions as first-class objects: `(lambda ...)` in expression
position yields one, `#'f` retrieves the one named `f`, and `funcall`/
`apply` call one held as a value. None of this is executable in
`src/smd/cl/eval`. `eval::value`
(`src/smd/cl/eval/value.hpp`) has no function alternative — its own comment
says so, deliberately: "there is deliberately no function alternative yet
... adding the alternative before there is a form that yields one would be
a channel with no producer." `machine::branch_visitor::operator()(core::
core_lambda const&)` diagnoses "a function definition is not yet an
expression," and `elaborator`'s `emit_evaluated` diagnoses `#'f` as
"#'f is not yet executable." `funcall` and `apply` are not even special
operators or builtins, so `(funcall f x)` is an ordinary call to the
undefined function `FUNCALL`.

## Why

Decision D19, applied the same way DIV-0024 applies it: do not add a
channel (here, a value alternative) with no producer, and do not add a
producer whose only consumer is the same step. `defun` already needs no
function value — it names one and calls it through the interned function
slot (decision D12) — so nothing in R1–R5's accepted witnesses required
this. The current diagnostic for `(lambda (x) x)` in expression position is
worth noting as poor rather than merely absent: `LAMBDA` is not a
recognized special-operator name in `elaborator::operator_ids`, so the form
falls through to an ordinary call and is reported as "undefined function"
rather than anything naming the real gap.

## Consequences

- No corpus entry may use `(lambda ...)` in expression position, `#'f`,
  `funcall`, or `apply` and expect a value; `src/smd/cl/conformance/corpus.hpp`
  contains none.
- Higher-order idioms — `mapcar`, `reduce`, anything taking a function
  argument — are unreachable until this closes, independent of whether
  those functions themselves exist.

## Revisit condition

Closed when a step adds a function-object value alternative to `eval::value`
together with at least one producer (`lambda` as an expression, or `#'f`)
and at least one consumer (`funcall`, `apply`, or ordinary application of a
value already known to hold one) in the same step — per D19, not a
value alternative in isolation.
