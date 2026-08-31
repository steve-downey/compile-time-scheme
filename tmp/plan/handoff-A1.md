# Handoff to A1

A0 merged `plan-reconciliation` into `main` (`--no-ff`, merge commit
`094f065`). No source under `src/` touched; ctest count unchanged at 1123 per
leg on both matrix legs, `make lint` and `verify-transclusions.sh` both green
(142 resolved, 1 documented WARN).

## `cl-retire-trees` does not exist yet

Not "does not exist yet" as a maybe — checked directly: `git branch -a | grep
-i retire` on the post-merge `main` returns nothing at all. The older
`retire-three-trees` branch your step file mentions as "nine commits stale at
`1ace4ad`" is **gone**, not stale — it has been deleted from this repository,
not merely superseded. Do not go looking for it as a base to compare against
or salvage from; there is nothing there. The orchestrator creates
`cl-retire-trees` off the post-merge `main` (containing `094f065`) before you
start. If it is absent when you go to work, that is the gate your own step
file already describes — write `blocked-A1.md` — but do not reach for
`retire-three-trees` as a substitute of any kind; it is not partially useful,
it is not there.

## The `docs/compiler_architecture.org` link count is confirmed, not corrected

Re-counted fresh on post-merge `main`, independently of your step file's own
numbers:

```
grep -o '\[\[file:' docs/compiler_architecture.org | wc -l          # 39
grep -o '\[\[file:[^]:]*::' docs/compiler_architecture.org | wc -l  # 37
grep -c 'file:\.\./src/smd/smdscheme' docs/compiler_architecture.org  # 7
grep -c 'file:\.\./src/smd/smdlisp'   docs/compiler_architecture.org  # 21
grep -c 'godbolt_lisp' docs/compiler_architecture.org                 # 3
```

7 + 21 + 3 = 31. Your step file's arithmetic (39 total, 37 transclusions, 31
of them pointing at code A3 deletes, 6 surviving) is right as written; nothing
to correct. A0 touched no file under `docs/compiler_architecture.org` or
`src/`, so this number could not have moved between A0's baseline and now.

## Scoping-note status-line wording, for your cross-references

Both notes' opening `**Status:**` line now reads (verbatim, so A1's and A3's
cross-references can match):

- `docs/cl-language-scoping.md`: "ratified by merge, PR #48, 2026-08-22. As
  provisionally as anything here — later facts and experience can override an
  earlier decision, and D32 below is the first exercise of exactly that."
- `docs/cl-parser-scoping.md`: "ratified by merge, PR #50, 2026-08-22. As
  provisionally as anything here — later facts and experience can override an
  earlier decision."

D32 lives in a new "Amendment, 2026-08-23" section appended to the end of
`docs/cl-language-scoping.md`, after its existing "§ 6. What happens next".
It does not touch §1–§6 in place except the status line above.

## BL-0002: closed, not restated

Closed outright, `superseded by D14`, checked directly against the code:
`grep -rln "MaxNodes\|Capacity" src/smd/cl --include=*.hpp` outside
`src/smd/cl/foundation/` returns nothing, and `core/ast.hpp` documents the
per-node types as deliberately non-parameterized on capacity. No open
question remains on it; nothing downstream should re-open it.

## `make lint` — nothing to carry forward

Ran clean (exit 0) both before and after this step's edits, including
`codespell` and `markdownlint` over every file this step touched. No
complaint to warn you about in the directories you'll be writing into.

## Not relevant to you, but on the record for later steps

BL-0004 now says eighteen shims throughout (was internally inconsistent
between seventeen and eighteen); BL-0003 stays open with its revisit
condition restated against R6's actual corpus (nothing in it reaches
`limits::frames`, so the original criterion still has not fired). Root
`checklist.md` now carries this plan's fourteen steps under a new "Plan
reconciliation and reader combinators" section, with only A0 ticked.
