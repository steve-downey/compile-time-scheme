# DIV-0007: nested backquote is opaque; `append` is two-argument only

- **Status:** accepted-permanent
- **Date:** 2026-07-21
- **Step:** L18 (backquote)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

1. ANSI CL tracks backquote nesting depth: a comma inside a nested backquote
   template belongs to the *inner* backquote unless enough commas cancel
   enough backquotes to reach back out to an outer level.
   `smdlisp`'s backquote expander (`src/smd/smdlisp/macroexpand/expander.hpp`,
   `expand_backquote_template`) does not track nesting depth at all.
   A `reader::datum_backquote` encountered anywhere except at the position
   `expand()`/`expand_backquote_template` itself is currently lowering is
   treated as ordinary literal data: it is wrapped whole in a `(QUOTE ...)`
   form via `detail::make_quote_form`, and its own internal
   `,`/`,@` nodes are never re-expanded.
2. ANSI CL's `append` is variadic (zero or more list arguments, the last of
   which need not be a proper list) and does not require a pair heap for the
   zero- or one-argument cases.
   `smdlisp`'s `append` (`closure::list_op::append`,
   `closure::pairs.hpp::apply_prim`) is a fixed two-argument primitive:
   `(append list1 list2)` only, and always requires a non-null pair heap
   (even though the empty-list-with-nil-heap case would otherwise be
   answerable without one).

## Why

1. Nested backquote with correct depth tracking is a well-known source of
   backquote-implementation complexity (each level needs its own
   "how many unquotes am I `,`-away from firing" counter); the plan's L18
   scope is one level of backquote/unquote/unquote-splice lowering into
   `cons`/`list`/`append`/`quote`, and no merge criterion or later step in
   the plan (through L19, `defmacro`) requires nested backquote. Treating a
   nested backquote as opaque data is the simplest, safest option: it either
   round-trips correctly (if the inner template happens to contain no
   `,`/`,@` — the common case for e.g. a literal quoted-looking sub-form) or
   fails loudly at elaboration time with `"quote: unsupported datum"` (if
   the inner template has embedded `,`/`,@` nodes reaching
   `elaborate_quoted_datum`, which has no case for `datum_backquote`/
   `datum_unquote`/`datum_unquote_splice`), rather than silently
   mis-evaluating a comma at the wrong nesting level.
2. Every actual use in this step's merge criteria and the six L17 host
   macros only ever needs the two-list-argument shape (splicing one
   evaluated list onto "the rest of the current cons chain," which is
   itself always exactly one value). Matching ANSI CL's full variadic
   `append` signature was not attempted since it is not exercised by
   anything through L19; `list`'s own no-heap zero-argument special case
   (`(list)` = `nil`) was not copied to `append` because `append`'s
   two-argument signature has no analogous zero-argument call shape to
   special-case.

## Consequences

- A macro-authoring step (plausibly L19, `defmacro`, or any later use of
  backquote for macro bodies) that wants nested backquote (e.g. a macro that
  itself expands to code containing another backquote template) will hit
  this limitation; the failure mode is either silent pass-through (safe) or
  a diagnosed elaboration error (also safe, just not helpful), never
  incorrect evaluation.
- If a later step needs a fully variadic `append` (e.g. for closer ANSI
  parity or a `format`-adjacent feature), `apply_prim`'s `append` case and
  `detail::append_impl` in `closure/pairs.hpp` need to accept a `std::span`
  of arguments instead of a fixed pair, folding right-to-left.

## Revisit condition

Revisit if a later step (L19 `defmacro` or later) needs nested backquote
templates, or needs `append` with more than two arguments or a `nil`-safe
no-heap path.
