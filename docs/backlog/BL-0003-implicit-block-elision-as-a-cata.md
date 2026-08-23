# BL-0003: elide the implicit block by a `cata`, not by an evaluator special case

- **Status:** open — R6 landed; revisit condition restated 2026-08-23 below
- **Date:** 2026-08-06, restated 2026-08-23
- **Origin:** phase 28 names the optimization and declines it ("Omitting the block when a body contains no `return-from` is a real optimization, it's obvious, and it isn't done"); reconsidering it against R1–R4's substrate moves it out of the evaluator entirely.
- **Frozen-tree impact:** none.

## What

A `defun` body is elaborated as `core_defun` over `core_lambda` over `core_block`
named for the function (ANSI 3.1.2.1.2.2), and `eval::machine` pushes a
`detail::k_block` frame on entry to that block. The frame cannot pop until the
body finishes, so a self-call in tail position costs one frame per call. That is
why `enter_closure` pushing no frame of its own does not yet amount to tail calls
(`src/smd/cl/eval/machine.hpp`, the comment in `enter_closure`).

The block is only observable if something inside the body can transfer to it.
"Does this subtree contain a `return-from` naming N" is a synthesized attribute
over the core tree, which is `foundation::cata` with a `static_vector` of
`symbol_id` as the carrier — one ascending pass, no stack, no new traversal
written by hand. Where the attribute says no, `elaborator::emit_definition`
emits the lambda body directly and skips the `core_block` node.

Files: `src/smd/cl/elaborator/elaborate.hpp` (the attribute and the emission
change), `src/smd/cl/elaborator/elaborate.test.cpp`. No evaluator change at all —
which is the point of the item.

## Why it is not done

Nothing has forced it. R5's acceptance witness is `(len '(a b c))`, three frames
deep against a `limits::frames` of 256, so the cost is invisible at the sizes
tested. It is also the kind of change that wants the conformance corpus in place
first: eliding a block changes which programs fit in a given frame budget, and
R6 is where a program long enough to care gets written.

## Evidence

- The frame is pushed unconditionally: `branch_visitor::operator()(core::core_block)`
  pushes `detail::k_block{tag.name}` before evaluating the body
  (`src/smd/cl/eval/machine.hpp`).
- `enter_closure` deliberately pushes nothing, and its comment states the
  remaining cost: "a self-call in tail position costs one frame per call rather
  than none."
- `emit_definition` builds the three-node nest, so the block is created at
  elaboration time and is the elaborator's to withhold
  (`src/smd/cl/elaborator/elaborate.hpp`, anchor `d398d3f2-8862-4663-9e03-28262372feae`).
- The scheme exists and is tested: `foundation::cata` over `tagged_tree`
  (`src/smd/cl/foundation/tagged_tree_schemes.hpp`, anchor
  `11011910-7c9b-42e5-bc50-53bbfd3242d9`). Not verified: that a `static_vector`
  carrier of collected names fits `MaxNodes` comfortably for realistic programs.

## Open questions

- Carrier: collect the set of names returned-from per subtree, or run the `cata`
  once per candidate block name with a `bool` carrier? The set is one pass and
  more useful later; the bool is simpler and repeats work per `defun`.
- Does anything else need the same attribute — an escape analysis for R7, a
  frame-budget estimate for BL-0002's arena sizing — which would argue for
  computing and keeping it rather than consuming it inside emission?
- `(block nil ...)` is currently diagnosed and unclassified (R6's list). If it
  later becomes legal, the attribute must handle a name the atom pass turned
  into a constant.

## Cost and risk

Small: one algebra, one branch in emission, and tests. The risk is that eliding
the block changes a diagnostic — a `return-from` naming a function whose block
was elided must still be an error, and the natural implementation makes it an
*unbound* block rather than a wrong one, so the message has to be checked, not
assumed. Re-verify `make test-matrix`, and re-run any corpus entry whose expected
outcome is an unwind.

## Decision criteria

Schedule it when a corpus entry or a demo program recurses deep enough that
`limits::frames` is reached for a body that never returns-from — that is the
first time the optimization is worth more than the code it costs. Close it as
declined if proper tail calls arrive by another route (a tail-position analysis
in emission would subsume it, and would want the same `cata`).

## Status against R6, 2026-08-23

R6 landed `conformance/corpus.hpp` with 75 entries, of which the relevant ones
are `block.1`, `block.4`, `block.8`, `return-from.1`, `return-from.unaimed`,
`defun.1`, `defun.wrong-arity`, and the recursive-`defun` acceptance witness
`defun.recursive` (`(+ 1 (conformance-len (cdr l)))` over a short list).
None of them recurses anywhere near `limits::frames` (256, `src/smd/cl/eval/machine.hpp`):
`defun.recursive` is the deepest, and it is a handful of frames on a short
list, the same shape as R5's own acceptance witness.
So the corpus this item wanted did land, and it did not change the answer:
nothing in it forces the optimization yet, and the original decision criteria
still has not fired.
The checkable next condition therefore stays exactly the one already stated
above — a corpus entry or demo program that reaches `limits::frames` for a
`return-from`-free body — restated here only to record that R6's landing was
checked and did not close it, rather than being silently skipped again.
