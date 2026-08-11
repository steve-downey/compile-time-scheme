# DIV-0022: the reader has no syntax for the dotted-pair consing dot

- **Status:** open
- **Date:** 2026-08-11
- **Step:** R6 (conformance corpus and differential oracle)
- **Authority diverged from:** ANSI Common Lisp; docs/cl-pivot-plan.md (D6)

## What diverged

`(a . b)` does not read as the dotted pair ANSI 2.4.1 specifies.
`src/smd/cl/reader/read.hpp`'s `read_delimited` reads list elements until
`)` with no special case for a bare `.` token; the `.` character has no
reader-macro role in `standard_readtable` (`src/smd/cl/reader/readtable.hpp`)
and is an ordinary constituent, so it tokenizes into a symbol named `.`.
`(a . b)` therefore reads as the three-element proper list `(a |.| b)`,
not the pair `(a . b)`.

Cons cells fully support dotted pairs at the value level (`eval::value_cons`,
`eval::heap::cons`) and at the elaboration level (`core::core_cons`,
`emit_quoted_list`'s pairs); only the reader's textual syntax is missing.

## Why

This is not a new decision: `docs/cl-pivot-plan.md` §(cons cells) already
states "Reader syntax for dotted pairs (`(a . b)`) is deferred until a step
needs it; file a divergence doc when deferring bites," and D6 there records
the pivot's reader shipping with no dotted-pair syntax at all. R3 (the
rebuild's reader step) inherited that same scope rather than reopening it;
nothing in its own brief asked for it.

It bit during R6: adapting `cons/cxr.lsp`'s `cons.24`
(`(cdr '(a . b)) => b`) into the conformance corpus requires reading
`(a . b)` as a pair, and the reader silently reads three symbols instead —
a `defun` corpus entry would have measured `length` at 3 rather than
diagnosing anything, which is exactly the kind of silent wrong answer D19
and D16 exist to keep out of the corpus. The entry was dropped rather than
adapted; see the comment beside where it would have been in
`src/smd/cl/conformance/corpus.hpp`.

## Consequences

- No corpus entry, present or future, may spell a dotted pair as source
  text (`'(a . b)`, `(cons 'a 'b)` read back and printed, etc.) until this
  closes. Building one from `cons`/`list` calls is unaffected — only the
  reader's `.` syntax is missing.
- The differential harness (`src/smd/cl/conformance/sbcl_differential.test.cpp`)
  never emits a form spelled with a consing dot for the same reason.
- Any later step adding reader syntax should special-case a lone `.` token
  inside `read_delimited`, per ANSI 2.4.1: exactly one element must follow
  it before the closing `)`.

## Revisit condition

Closed when a step gives `read_delimited` (or its successor) a rule for a
bare `.` token between list elements, per ANSI 2.4.1.
