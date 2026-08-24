# BL-0007: `cl/sender/sender_v.hpp`'s header comment justifies itself by a rule that no longer exists

- **Status:** open — deliberately left by A3, which was told to leave comment
  prose alone; not a defect in that step
- **Date:** 2026-08-23
- **Origin:** Phase A close-out, orchestrator acceptance sweep after A3.
- **Frozen-tree impact:** none — both retired trees were deleted at A3 (D32).

## What

`src/smd/cl/sender/sender_v.hpp`'s header comment explains why the file is a
fresh copy rather than a reuse of an existing vocabulary header, and gives as
its reason:

> both `smd::smdscheme` and `smd::smdlisp` are read-only trees (dead-ended and
> behavioural-oracle respectively), so nothing under `src/smd/cl/**` may depend
> on either.

That sentence is present tense and is now false twice over. Neither tree is in
the worktree, and the never-edited rule it invokes was retired by D32 and
executed at A3.

The comment's *conclusion* survives — nothing under `src/smd/cl/**` depends on
either tree, more absolutely than before — but the stated justification is
gone. A reader who follows the reasoning arrives at a rule they cannot find,
in the four documents that used to state it.

The adjacent sentence, "the same discipline `smd::smdlisp::sender_v` already
established for the pivot", is a historical claim about where the idea came
from and reads correctly as history. It should stay.

## Why it is not done

A3 was explicitly instructed to leave surviving comment prose alone, and its
step file named this file among the expected survivors. Widening a
thirty-thousand-line deletion to rewrite prose in a file it was otherwise not
touching would have been the wrong trade. This is the residue of that correct
decision, not a miss.

## Evidence

`src/smd/cl/sender/sender_v.hpp:14-17`, verified on `main` at the Phase A
merge. The wider sweep that found it:

```sh
git grep -n "smdscheme\|smdlisp" -- '*.hpp' '*.cpp' 'CMakeLists.txt' '*.yml' src/
```

returns 27 lines. Twenty-six are unambiguously historical and correct as they
stand: fourteen provenance comments under `src/smd/kit/foundation/` that A3
tagged with `at iteration/smdscheme-final` so they stay checkable, four
prose-only mentions in the same directory, seven R8 shim comments under
`src/smd/cl/foundation/` describing why the typeclass framework was extracted,
and `sender_v.hpp:13`'s history note. `sender_v.hpp:15` is the only line that
asserts a present-tense fact that stopped being true.

## Open questions

- Rewrite in place, or append? This is a source file, so neither the
  append-only convention of `docs/divergences/**` nor the refine-in-place rule
  of `docs/backlog/**` applies; ordinary source-comment editing does. Rewrite
  in place is almost certainly right, and the question is only whether the
  history sentence and the justification sentence should be separated so the
  first cannot rot with the second.
- Is a general sweep wanted for present-tense claims about the retired trees,
  or only this one line? This sweep found exactly one. A recurrence is
  unlikely now that the trees are gone, which argues for fixing the line and
  not building a check.

## Cost and risk

One sentence in one comment. No behavioural risk at all: the file is
vocabulary aliases and the change cannot reach code. `make lint` would need to
be green afterward, which for a comment edit means `codespell` and
`clang-format`.

## Decision criteria

Worth doing as a pre-authorized incidental fix by whatever step next edits
`src/smd/cl/sender/`, rather than as a step of its own — it is smaller than
the commit message describing it. Worth closing as `declined` only if the view
is that a comment citing a retired rule is self-evidently historical to any
reader, which the orchestrator does not think is true here, because the
sentence is written in the present tense and states a constraint rather than a
recollection.
