# DIV-0023: `(block nil ...)` is diagnosed, not accepted

- **Status:** open
- **Date:** 2026-08-11
- **Step:** R6 (conformance corpus and differential oracle); discovered and
  flagged by R5 (`src/smd/cl/eval`)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

`(block nil form...)` is ordinary, legal ANSI syntax — `nil` is as valid a
block name as any other symbol (ANSI 3.1.2.1.2.2 places no restriction on
it), and `loop`'s implicit block relies on exactly this. In `src/smd/cl/`,
it is diagnosed instead: `elaborator::emit_block`
(`src/smd/cl/elaborator/elaborate.hpp`) requires `name_at` to find a
`core_variable` at the name position, and `NIL` is never one — the atom
pass (`detail::atom_lowering`, run before the role scan) turns every `NIL`
token into the `core_nil` constant leaf, unconditionally, before anything
downstream can tell "this one names a block" from "this one is data."
`emit_block` therefore reports "a block name must be a symbol" for every
`(block nil ...)`, matching `(return-from nil ...)`'s equivalent failure.

## Why

Ordering, not intent. R4's atom pass runs bottom-up over every leaf before
the role pass runs top-down over the tree (decision D15's three-scheme
elaborator), so by the time a node's role — name vs. expression — is known,
`NIL` has already been rewritten. Fixing this means either teaching the
atom pass about role (which the whole point of splitting the passes was to
avoid) or teaching the role/emission passes to recognize `core_nil` as a
name when a name is expected — a real design question, not a one-line fix,
and one that matters most once `loop` or another iteration macro needs a
`nil`-named implicit block.

## Consequences

- No corpus entry, present or future, may spell `(block nil ...)` or
  `(return-from nil ...)` and expect it to bind or exit a block: today it
  is a diagnosed error, and that is a real limitation, not a spec-derivable
  fact to pin as correct. `src/smd/cl/conformance/corpus.hpp` contains no
  such entry.
- Any macro that expands to an implicit `(block nil ...)` (`loop`, `do`,
  `dolist`, `dotimes`) cannot be built on `block`/`return-from` as they
  stand today; the ordering question above must be answered first.

## Revisit condition

Closed when a step gives the elaborator a way to recognize `nil` as a name
in name position (block name, `return-from` target, or a lambda-list
parameter) without disturbing the atom pass's constant-vs-variable
denotation for `nil` in expression position. Iteration macros are the
concrete forcing function.
