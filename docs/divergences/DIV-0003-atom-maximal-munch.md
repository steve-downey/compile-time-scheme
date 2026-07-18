# DIV-0003: atom reader uses whole-token classification, not greedy-digit integer scanning

- **Status:** accepted-permanent
- **Date:** 2026-07-18
- **Step:** L5 (docs/cl-pivot-plan.md)
- **Authority diverged from:** docs/cl-pivot-plan.md (step L5 sketch: "integers (unchanged rules)")

## What diverged

The plan's step L5 sketch says integers use "the same rules as the Scheme side," which suggested reusing
`smd::smdscheme::reader::integer_p`'s approach unchanged: an optional leading `-` followed by greedily
consuming a run of decimal digit characters, wherever that run happens to stop.

That approach is correct for the Scheme reader because Scheme symbols (per
`smd::smdscheme::parser::is_initial_symbol_char`) can never start with a digit, so a digit-led token is
unambiguously a number.
Common Lisp symbols have no such restriction; conventional symbols like `1+` start with a digit.
A greedy-digit integer parser tried before a symbol parser would misread `1+` as the integer `1` followed by
a stray, unconsumed `+`, rather than as the symbol `1+`.

`smdlisp`'s `atom.hpp` instead reads a whole token first (`detail::atom_token_p`, a maximal run of
constituent characters) and classifies the *entire* token afterward: it is an integer only if it is wholly an
optional leading `-` followed by one or more decimal digits, and a symbol otherwise.
`integer_p`, `symbol_p`, and `keyword_p` (`keyword_p` for the token following the leading `:`) all build on
this shared token scan.

## Why

Discovered by `make test` failing a static_assert for `read_atom("1+")`: the greedy-digit port of the Scheme
approach parsed it as the integer `1`, leaving `+` unconsumed, instead of the symbol `1+`.
The plan sketch's "same rules as Scheme side" did not survive contact with CL's digit-permissive symbol
syntax.

## Consequences

- `atom_p` (and `read_atom`) still try `integer_p` before `keyword_p`/`symbol_p`, but each alternative now
  fully classifies the whole token rather than partially consuming it, so there is no partial-match ambiguity
  to resolve via `operator|`'s no-input-consumed rule; the three are tried in explicit sequence instead,
  matching `smd::smdscheme::reader::detail::read_datum_node`'s existing fallback style.
- A lone `-` (no digits following) reads as the symbol `-`, not a truncated/incomplete integer; this is
  correct ANSI CL behavior (`-` is the subtraction function's symbol) and is covered by a test.
- Step L6 (datum reader) and any later step that reads atoms should call `atom_p`/`read_atom` rather than
  reimplementing greedy-digit scanning, so this fix is not silently lost.
- No test or later step should assume Scheme-style "digit-led token is always a number."

## Revisit condition

None.
Whole-token classification is the correct behavior for a CL-flavored reader and is not expected to change.
