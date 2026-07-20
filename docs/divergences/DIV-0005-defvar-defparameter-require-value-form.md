# DIV-0005: `defvar`/`defparameter` require a value form; no docstring support

- **Status:** accepted-permanent
- **Date:** 2026-07-20
- **Step:** L12 (docs/cl-pivot-plan.md)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

ANSI Common Lisp's `defvar` grammar is `(defvar name [value [documentation]])`: both the initial-value
form and the trailing documentation string are optional.
A `(defvar name)` with no value form proclaims `name` special without binding it (or, if `name` is already
bound, does nothing further).
`defparameter`'s grammar is `(defparameter name value [documentation])`: the value form is mandatory, but
the documentation string is still optional.

`smdlisp`'s elaborator (`elaborate.hpp`'s `DEFVAR`/`DEFPARAMETER` cases) requires exactly the two-element
name-plus-value shape for both forms (`(defvar name value)` / `(defparameter name value)`) and rejects
anything else, including the value-less `(defvar name)` form ANSI CL permits, and rejects any trailing
docstring argument.

## Why

The plan's own step L12 sketch does not specify `defvar`/`defparameter`'s exact grammar, only that they
"define in the VARIABLE namespace and mark the symbol special."
Supporting the value-less `defvar` form requires either an optional `arena_box` in `core_defvar` (e.g. a
`std::variant<std::monostate, arena_box<R, MaxNodes>>`) or a second core kind, and the evaluator's
`core_defvar` case would need a corresponding branch that proclaims special and does nothing else.
Docstring support requires accepting and discarding an extra trailing string-typed argument, which
`smdlisp` cannot even represent yet (no string atom kind exists — see D10, "out of scope").
Neither addition is needed by the merge criterion (`(progn (defun twice (x) (+ x x)) (twice 4))`), and
requiring a value form keeps `core_defvar`/`core_defparameter`'s shape identical (a name plus one
`arena_box`), matching the precedent already set by L10's "lambda/progn/let/let\* require at least one body
expression" restriction (no divergence doc filed there, treated as an in-scope-subset cut).
This one is filed because, unlike the body-expression restriction (baked into `core_lambda`'s very shape),
omitting `defvar`'s value form is valid, commonly-used ANSI CL that a reader of this code might reasonably
expect to work.

## Consequences

- `(defvar *x*)` with no value form is a compile-time elaboration error ("defvar: expected name and value"),
  not a proclaim-only special-variable declaration.
- No docstring argument is accepted on either form; a third argument is an arity error.
- A later step that wants full ANSI `defvar`/`defparameter` grammar (plausibly L16, when special-variable
  semantics actually start to matter, or L24's documentation-consolidation pass) should widen
  `core_defvar`/`core_defparameter` to make the value form optional; this does not need to wait for string
  support, since a value-less `defvar` never needs to represent a docstring at all.
- `docs/cl-limitations.md` (step L24, per the plan's own consolidation step) should roll this up alongside
  every other D10-adjacent scope cut.

## Revisit condition

Revisit when a later step needs `(defvar name)` (proclaim-only, no binding) to work -- most plausibly L16
(special variables and dynamic binding), where the distinction between "proclaimed special, unbound" and
"proclaimed special, bound" first becomes semantically observable.
