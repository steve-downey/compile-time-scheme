# step-brief (L23 lane) — Step L23: `tagbody` / `go`

Forward-only brief for the clean agent that picks up L23. Bounded; not a log.
Written by the orchestrator, not by a predecessor worker: L23 was decided and
scoped but never started, so this brief carries the scope decision rather than a
handoff. Prior-step narrative lives in `git log`; deviations live in
`docs/divergences/`.

**Status:** every non-optional pivot step is merged. `main` is at `d63a6d2`,
795 tests green. Only L23 and L24 are unchecked in `checklist.md`. A clean
worktree already exists at `../wt-l23` on branch `cl-pivot/l23-tagbody-go`,
based on `d63a6d2` with nothing written to it.

Read `step-brief-l18.md` alongside this one — it describes the L20 multiple-value
channel (`value_list`, the `_values` primitive / single-value adapter pairs) that
your evaluator arms must interoperate with. Do not restate its facts here.

## The step

Lower `tagbody` to a set of basic-block continuations indexed by tag. `go` is a
one-shot transfer per invocation using the L14 exit discipline: the tags' extent
is the `tagbody`'s dynamic extent.

**Decision D8 made `tagbody`/`go` optional, and the author has since decided to
implement it fully.** That is settled — do not re-litigate it and do not defer it
to a divergence doc. If something is genuinely blocking, report it as a blocker.

**Scope is all three backends.** L20 stubbed its own forms in the sender backend
(diagnosed error rather than support, DIV-0018). That was L20's scope call and it
is explicitly **not** the precedent here; the author asked for real support in:

- `elaborator/{elaborated_core,elaborate}.hpp` — new core kinds, new special operators
- `closure/eval_direct.hpp` — direct evaluator arms
- `closure/cps_code.hpp` — the basic-block continuation lowering (the substance)
- `closure/closure_program.hpp`, `smdlisp.hpp` — if signatures move
- `sender/sender_eval.hpp` — real support
- co-located `.test.cpp` for each

## ANSI semantics that are easy to get subtly wrong

- `(tagbody . body)` is a sequence of *statements* and *tags*. A tag is a symbol
  or an integer; anything else is a form to evaluate. Tags are not evaluated and
  are not expressions. Integer tags compare with `eql`.
- `tagbody` returns `nil`, always. Statement values are discarded.
- `(go tag)` resumes executing statements at that tag in the lexically enclosing
  `tagbody`. Tag resolution is **lexical, at elaboration time** — an unknown tag
  is an elaboration error.
- The extent is **dynamic**. A `go` to a tag whose `tagbody` invocation has ended
  is a diagnosed error (D5), exactly as `return-from` to a dead block is. Reuse
  the L14 `exit_record` liveness machinery; do not invent a parallel mechanism.
  The load-bearing case is a closure created inside a `tagbody` that `go`s to its
  tags — test both the live and the dead version.
- `go` out of an `unwind-protect`'s protected form must run its cleanup.

Symbols-only is an acceptable narrowing **with a divergence doc** if integer tags
prove disproportionate — but decide deliberately and say which you chose.

## Merge criterion

Working `tagbody`/`go` in all three backends, with tests in each. At minimum:

```lisp
(tagbody a (go c) b (setq x 1) c)     ; skips b; returns nil
(let ((n 0)) (tagbody top (setq n (+ n 1)) (if (< n 3) (go top))) n)   ; => 3
```

The second is the point of the step: `tagbody` is how Common Lisp expresses
iteration without recursion, so a real loop running at compile time is the
demonstration. Plus: `go` through `unwind-protect` runs cleanup; a dead-extent
`go` is diagnosed, not a hang; `tagbody` returns `nil`.

## Standing constraints

- `src/smd/smdscheme/**` is frozen (D1) — read it, copy from it, never edit it.
- UUID anchors: see the anchor section in `AGENTS.md`. Inside `src/smd/smdlisp/**`
  the *contents* of an anchored region may be edited; only deleting or nesting a
  pair is forbidden. Three workers have now been slowed by an over-broad reading
  of this rule. Add real `uuidgen` anchors around the basic-block lowering and the
  extent check, since L24 may transclude them.
- `docs/compiler_architecture.org` has no `smdlisp` section by design; opening one
  is L24's job. Add nothing to it.
- **DIV-0021 is the next free divergence number**; re-check the directory first.
- C++26 / GCC16, Catch2. Suite is **795 tests** at `d63a6d2`; it should only grow.
- Before handoff: `make compile`, `make test`, `make lint`. Then tick L23 in
  `checklist.md` and rewrite `step-brief-l18.md` so it hands L24 what it needs
  from both L20 and L23.

## One other decided-but-unstarted change

Independent of L23, and not yours unless the orchestrator assigns it: the author
decided to raise `closure::default_max_values` in `closure/value.hpp` from **8 to
20**, to meet ANSI's `multiple-values-limit` minimum. A clean worktree exists at
`../wt-maxvalues` on `cl-pivot/raise-max-values`, currently identical to `main`.

`MaxValues` is the width of the `static_vector` payload every continuation and
every recursive evaluator frame carries, in constexpr evaluation as well as at
run time, so the change wants its build cost measured before and after rather
than assumed. DIV-0020 argues for 8 and must be **superseded by appending** a
note, not edited — these docs are append-only.
