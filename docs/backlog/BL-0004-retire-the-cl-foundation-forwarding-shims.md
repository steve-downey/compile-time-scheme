# BL-0004: retire the `cl/foundation` forwarding shims

- **Status:** open — scheduled for removal in principle, deliberately not yet
- **Date:** 2026-08-16, corrected 2026-08-23
- **Origin:** step R8 (`bd3d884`), which extracted `smd::kit::foundation` and left eighteen forwarding headers behind so that no caller had to change. Owner's direction on the day R8 landed: schedule the removal now that the extraction is confirmed working, but leave the shims in place while there is still room for regrets.
- **Frozen-tree impact:** scheduled to change at step A3. Today: the shims are `src/smd/cl/foundation/**`, which is the live tree; `src/smd/smdscheme/**` keeps its own independent copies and is not a client of the kit; `src/smd/smdlisp/**` depends on `smdscheme.foundation` and is unaffected. `tmp/plan/` step A3 deletes both trees from trunk, after which neither clause means anything — the shims' only remaining context is where the kit boundary sits, which is the actual question this item turns on.

## What

Delete the eighteen forwarding headers in `src/smd/cl/foundation/` that
re-export `smd::kit::foundation` names, and repoint every `cl` caller at
`<smd/kit/foundation/…>` directly.

The shims are: `static_vector`, `result`, `parse_error`, `source_pos`,
`source_span`, `arena_box`, `functor`, `applicative`, `alternative`,
`foldable`, `traversable`, `monoid`, `identity`, `fold_left_short`,
`trampoline`, `result_instances`, `static_vector_instances`, and `monad`.

`monad` is the eighteenth, added once the Monad typeclass landed in the kit
independently of this item. It forwards `smd::kit::foundation::monad`,
`monad_typeclass` and the `bind`/`join` CPOs, and it is the one shim that
never had a `cl`-side definition to forward from — the typeclass was written
in the kit directly, because `result` and its other typeclasses were already
there — so it exists purely so that `cl` code spells its includes the way the
rest of `cl` does. That makes it the cheapest of the eighteen to retire and
the least informative about whether the kit boundary has settled.

A nineteenth shim exists as of step B2, outside this item's literal title
scope: `src/smd/cl/reader/cursor.hpp`, forwarding `cursor`, `parse_state`
and `advance_while` to `smd::kit::parser`, added when B2 moved those three
out of the reader so `parser<F>` could compose over the same cursor and
parse_state the reader already had. It is `reader/`, not `foundation/`, but
tracked here rather than opening a second item, for the same reason as the
other eighteen: repointing every existing caller was not the point of that
step, keeping the name alive so `read.test.cpp` and `cursor.test.cpp` did
not have to change was.

Staying in `src/smd/cl/foundation/` as real files, not shims:
`node_f`, `topo_fold`, `tagged_tree`, `tagged_tree_instances`,
`tagged_tree_schemes`. These name an AST type and are correctly outside
the kit.

Callers to repoint span `symbol/`, `reader/`, `core/`, `elaborator/`,
`eval/`, `conformance/` and — transitively through `cl.eval` rather than
directly — `sender/`. Enumerate them by walking the link graph, not by
grepping include lines: R7 recorded that `cl.sender` links `cl.eval` and
not `cl.foundation`, so a text grep understates the affected set.

## Why it is not done

R8 chose shims so that the extraction was invisible to every caller,
which made the move verifiable as a refactor: if nothing outside
`src/smd/cl/foundation/` changed and the suite still passed at 1118
tests on both matrix legs, the move was behaviour-preserving — a count
now eighteen shims, one more than R8 itself moved (see "Evidence"). That
was the right call for landing it.

It is not the right end state. Every kit type now has two spellings, and
nothing stops a future caller reaching for the `cl` one. The reason to
wait is that the kit has had exactly one client for one day: if the
boundary is wrong somewhere, the fix is likely to be moving a header
back, and the shims make that cheap. Once the boundary has survived some
real use, they are pure cost.

## Evidence

- Eighteen shim headers today, verified by
  `grep -l 'Forwarding shim' src/smd/cl/foundation/*.hpp | wc -l`.
  R8 itself landed 17 of them, verified by `ls src/smd/cl/foundation/*.hpp`
  against the kit's contents at `19c8702`: 22 headers in `cl/foundation`,
  of which 5 are the AST group and 17 forward. The eighteenth, `monad.hpp`,
  arrived independently once the Monad typeclass landed in the kit
  (`ae3c33a`) — see "What" above.
- The extraction touched no file outside `src/smd/kit/`,
  `src/smd/cl/foundation/`, the docs, `checklist.md` and two
  `CMakeLists.txt` — confirmed from `git show --stat bd3d884`. That is
  the property the shims bought, and also the measure of how much
  repointing this item defers.
- One shim comment had already gone stale within a day of the merge,
  claiming `result_instances.hpp` stays in `cl` when it had moved
  (fixed in `3e54d0a`). A duplicated surface acquires its own drift; this
  is the first instance, found by a reader cross-checking two files.

## Open questions

- Whether the removal is one step or one step per area. Per-area keeps
  each diff reviewable; single-step avoids a half-migrated tree where
  both spellings are live and the shims cannot yet be deleted.
- Whether any kit header should move back to `cl` first. This item
  should not be scheduled while that is still plausible — that is the
  whole reason for the wait.
- Whether `smdscheme` is deleted from trunk before this lands. If so, its
  independent `foundation`/`parser` copies go with it, and the kit's
  relationship to the remaining trees simplifies.

## Cost and risk

Mechanical and wide: an include-path rewrite across six `cl` areas plus
a `CMakeLists.txt` link-line audit, followed by deleting eighteen files.
No semantics change, so `make test-matrix` on both legs is the whole
verification, and `make compile-headers` catches any header that was
relying on a shim for transitive includes — which is the one real risk,
since a shim that includes the kit header may be masking a missing
include in a caller.

## Decision criteria

Schedule it once the kit boundary has stopped moving — concretely, once
either `compile-time-forth` has adopted the kit or a second `cl`-side
consumer has exercised it, and no header has needed to move back.
Close it as declined only if the shims turn out to be load-bearing for
something other than migration convenience, which is not currently
expected.
