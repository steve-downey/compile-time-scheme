# step-brief (Track C) — for L24: documentation consolidation

Forward-only brief for the next clean agent. Bounded; not a log.
It carries what **L20 (multiple values)** and **L23 (`tagbody`/`go`)** leave to
L24, and nothing else. L23 was the last implementation step; every box in
`checklist.md` except L24 is ticked. When L24 lands, delete this file.

## What L24 must account for from L20

### The core AST gained one node kind, and lost the one you might expect

`elaborator::core_multiple_value_bind<R, MaxNodes>` (anchor
`98a22b58-964f-4853-be88-20a1d4a9b9d6` in `elaborated_core.hpp`) is the only
new variant alternative. There is deliberately **no** `core_values` node:
`values` is a *function* in ANSI CL, so it is an ordinary builtin
(`closure::builtin_op::values`, installed as `VALUES` by all three
`default_env` overloads) and `(values 1 2)` elaborates to a plain
`core_application`. That is what makes `#'values`, `(funcall #'values 1 2)`
and `(apply #'values '(1 2))` work.

`multiple-value-bind` stays a special operator because it is a *binding*
form; it lowers to a real `core_lambda` that the evaluators **call**, which is
how it inherits implicit progn, arity checking, closure capture, and L16's
dynamic-binding rule without restating any of them.

### The evaluators' return channel is n-ary

`closure::value_list<Core, MaxValues>` = `static_vector<value<Core>,
MaxValues>`, with `default_max_values == 8` (anchor
`a4a90af9-fd64-4f75-ad47-609c7c1e50fd` in `value.hpp`, alongside
`primary_value` and `single_value`). Each closure-backend operation is a pair,
`_values` primitive + single-value adapter: `eval_direct_values`/`eval_direct`,
`apply_function_value_values`/`apply_function_value`, and on the CPS side
`cps_dispatch`/`cps_apply_function_value` invoking continuations with a
`value_list` (`detail::identity_k`, `closure_program::call_values`). Anchor
`4f5a55a0-...` in `eval_direct.hpp` is where that convention is explained.
`MaxValues` is a *defaulted* template parameter, which is why every
pre-existing four-argument explicit instantiation still compiles.

The honest one-line summary for prose: *multiple values propagate through
returns, not through escapes, and not into the sender backend.*

## What L24 must account for from L23

### `tagbody` is lowered to basic blocks at elaboration time

`elaborator::core_tagbody<R, MaxNodes, MaxList>` and `elaborator::core_go`
(anchor `8e268a26-7eaa-470f-86e5-3597d376b700` in `elaborated_core.hpp`) are
the two new variant alternatives, bringing the core to 23 kinds. The
interleaved source body is split *once*, in the elaborator (anchor
`45420d1b-7082-46d4-a27a-9aef17024f8c` in `elaborate.hpp`, a deliberate two
passes so a forward `go` resolves): statements in order in
`core_tagbody::statements`, and a tag -> first-statement-index table in
`core_tagbody::labels`. No evaluator re-derives the blocks. `core_go` carries
only its tag; `tagbody` always evaluates to `nil`.

Tags are **symbols or integers**, per ANSI. `core_tag` is
`variant<int, string_view>`, and at run time a tag is keyed by an ordinary
`closure::value`, whose `operator==` is `eq` for symbols and `eql` for
integers — exactly ANSI's rule, with no comparison code written for it.
`closure::detail::tag_key<Core>` (`eval_direct.hpp`) is the one converter, used
by all three backends.

### `go` reuses the L14 exit discipline, with one twist

`env` gained a fourth namespace (`define_tag`/`lookup_tag`, returning
`closure::tag_target`, anchor `222e7d9c-2121-4831-bd34-b4110a310577` in
`env.hpp`) and a third unwind marker, `go_unwind_marker` (anchor
`2a233beb-0954-4da5-98c7-7bde8b28f58e`, same file). No new record type: a
`tagbody` allocates an ordinary `exit_record` via `alloc_exit`, and the
payload — a value for `block` — holds the *basic-block index to resume at*.

The twist, and the sentence worth writing down in prose: `return-from` and
`throw` end their target's extent, but **`go` does not**. The owning `tagbody`
claims the transfer, resumes at the requested block, and sets its own record
`live` again. That re-arming is what makes `tagbody` iterate rather than merely
escape, and it is the only place in the project where a spent exit record comes
back to life.

### The lowering itself, in each backend

- CPS (anchor `7d388d02-7ba7-45ab-84cd-38b213b37ab2` in `cps_code.hpp`) — the
  one L24 will most want to transclude. It is a **trampoline**, not mutually
  tail-calling block continuations, and the comment there says why: a CPS `go`
  that tail-called its target would add one C++ frame per iteration, so the
  compile-time loop would exhaust constexpr depth instead of counting to three.
  A `tagbody` is a continuation barrier exactly as `block`/`catch`/a call are.
- Direct evaluator: anchor `152148ae-5a16-4a5b-9f88-258efb2bc5c7`
  (`eval_direct.hpp`).
- Sender backend: anchor `a07baaf4-fabe-41d3-a9a6-fbd5c52dbff5`
  (`sender_eval.hpp`). **Real support, not a DIV-0018-style stub** — and it
  needed no new machinery at all: a `go` is a nonlocal transfer, `set_stopped`
  already *is* that channel, and `live` alone answers "aimed at me?" without any
  sentinel. A dead-extent `go` stays on the error channel, like a dead-extent
  `return-from`.

### The demonstration to lead with

```lisp
(let ((n 0)) (tagbody top (setq n (+ n 1)) (if (eql n 3) nil (go top))) n)  ; => 3
```

`static_assert`-green in `eval_direct.test.cpp`, `cps_code.test.cpp` and
`smdlisp.test.cpp` (through `compiled_lisp`), and runtime-green in the sender
backend (DIV-0015: no constexpr twin there). Iteration without recursion,
running in the compiler.

One thing to say out loud if L24 quotes the plan's own wording: the plan spells
that loop `(if (< n 3) (go top))`, and **the builtin set has no `<`** (it is
`+ * cons car cdr list null eq eql atom funcall apply append values`). The tests
run the verbatim `<` spelling too, with `<` installed as a `foreign_function`;
that is not a `tagbody` limitation and is not recorded as a divergence, but it
will surprise anyone who types the plan's line into a demo.

## Divergences to describe, not to re-derive

- **DIV-0017 / DIV-0018** — the sender backend diagnoses `values` and
  `multiple-value-bind` rather than supporting them. Still open; closing one
  closes both. Note for accuracy: after L23 these are the *only* two forms the
  sender backend does not support.
- **DIV-0019** — `return-from`/`throw` carry only the primary value, because the
  exit/catch record payloads are single-valued. `go` carries no value at all, so
  it is unaffected.
- **DIV-0020** — the multiple-value operator set is exactly `values` and
  `multiple-value-bind`; `default_max_values` is 8, below ANSI's minimum of 20.
- **DIV-0021** (new, L23) — a same-named inner `block`/`tagbody` shadows its
  outer one for the rest of the enclosing body, because exit bindings go into
  the ambient environment and nothing removes them. Diagnosed, not silently
  wrong. The `block` half is inherited from L14 and was simply unreachable until
  `tagbody` made bodies re-enterable.

**DIV-0021 is the highest used**; re-check `docs/divergences/` before filing.

## Standing constraints (unchanged)

- `src/smd/smdscheme/**` is frozen (D1) — read-only reference, never edit.
- UUID anchors: see the anchor section in `AGENTS.md`. Inside
  `src/smd/smdlisp/**` the *contents* of an anchored region may be edited; only
  deleting or nesting an anchor pair is forbidden. L23 placed its new arms
  *outside* the existing `e6a2c1de`/`180a37f4` regions rather than inside them,
  which is the only reason its own anchors are legal — worth knowing before
  adding an anchor near an evaluator's visit.
- `docs/compiler_architecture.org` is still smdscheme-only by design. **Opening
  its `smdlisp` section is L24's job**; L20 and L23 both deliberately added
  nothing to it.
- C++26/GCC16 baseline; Catch2; the suite is **838 tests** as of the L23 merge
  (795 before it).
- Before handoff: `make compile`, `make test`, `make lint`.

## One decided-but-unstarted change, still unowned

Independent of L23 and L24: raise `closure::default_max_values` in
`closure/value.hpp` from **8 to 20**, to meet ANSI's `multiple-values-limit`
minimum. A clean worktree exists at `../wt-maxvalues` on
`cl-pivot/raise-max-values`. `MaxValues` is the width of the `static_vector`
payload every continuation and every recursive evaluator frame carries, in
constexpr evaluation as well as at run time, so the change wants its build cost
measured before and after rather than assumed. DIV-0020 argues for 8 and must be
**superseded by appending** a note, not edited — these docs are append-only.
