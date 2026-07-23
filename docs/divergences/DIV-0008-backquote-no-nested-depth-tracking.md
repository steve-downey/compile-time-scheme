# DIV-0008: nested backquote has no depth tracking

- **Status:** open
- **Date:** 2026-07-22
- **Step:** L18 (backquote)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

ANSI CL tracks backquote nesting depth: inside a nested `` `(a `(b ,c)) ``,
an unquote must be preceded by as many extra commas as there are enclosing
backquotes to reach the outer expansion (`,,c` unquotes `c` at the outer
level; a lone `,c` at that nesting depth is still data, one level of
backquote away from being live). `src/smd/smdlisp/macroexpand/expander.hpp`'s
`detail::expand_backquote_template` does not implement this: a `` ` ``
template nested inside another `` ` `` template is lowered as opaque
literal data (wrapped whole in a native `datum_quote` node, the same
"everything else" branch used for plain atoms), with no attempt to look
for or lower any `,`/`,@` escapes it contains.

## Why

The plan's step L18 goal names the single-level case explicitly (`` `(a
,x ,@ys) `` expansion tests, plan §9) and step L19 (`defmacro`) is the
first step that gives host code a reason to write nested backquote
(a macro whose expansion itself contains a `` ` `` template, which is the
classic nested-backquote use case in real Common Lisp macros). Implementing
correct depth-tracked nested unquote now would add real complexity
(threading a depth counter through `expand_backquote_template`/
`expand_backquote_list_elems`, and deciding how a `,,x` reader spelling
should even lower to a datum, which the reader does not support yet) for a
case this step's merge criterion does not exercise.

## Consequences

- A user-written `` `(a `(b ,c)) `` does not behave per ANSI CL: `c` is not
  looked for, and the inner `` `(b ,c) `` becomes fully literal data,
  unquote and all, rather than a nested template whose innermost unquote is
  still live one level down.
- No test in `expander.test.cpp` exercises nested backquote at all: the
  single `` `x `` and `` `(a ,x ,@ys) `` shapes are the tested subset.
- The limitations doc (step L24) must list this divergence.

## Revisit condition

Revisit if/when `defmacro` (L19) or a later step needs a macro expansion
that itself constructs a nested backquote template; at that point
`detail::expand_backquote_template` needs a depth parameter (incremented on
entering a nested `datum_backquote`, decremented on the unquote that
matches the current depth) instead of treating any nested `` ` `` as
opaque data.
