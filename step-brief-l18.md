# step-brief-l18.md — Step L18: backquote (Track C)

Forward-only brief for the next clean agent on **Track C**. Bounded; not a log.
Prior-step narrative lives in `git log` / `docs/history/handoff-archive.md`;
architecture lives in `docs/compiler_architecture.org`. Read the L18 section of
`docs/cl-pivot-plan.md` for the authoritative spec (orchestrator pastes it).

**Status:** L17 (macro expander, Track C) is merged; L18 depends on L17 (satisfied).
L13 (CPS backend, Track A) runs concurrently in a non-overlapping lane — see
`step-brief-l13.md`. Likely cross-lane conflict: tracking docs + top-level
`src/smd/smdlisp/CMakeLists.txt`.

## Goal (plan §9, L18)

Reader support for `` ` ``, `,`, `,@` lowering to datum template nodes, and expansion
of templates into `cons`/`list`/`append` builds during macro expansion. `append` joins
the builtin set. **Unlike L17, L18 is expected to touch `src/smd/smdlisp/reader/`**
(new lexical syntax, presumably a new datum kind analogous to `datum_quote`/
`datum_function`). Decide the datum-kind shape before writing code; do not assume
L17's "expander never touches the reader" precedent applies.

## Merge criteria (plan §9, L18)

`` `(a ,x ,@ys) `` expansion tests at **both** datum and value level.

## What L18 builds on (from L17; detail in macroexpand/expander.hpp + arch doc)

- **Pipeline is read → expand → elaborate → eval.**
  `lisp::macroexpand::expand_and_elaborate<MaxNodes,MaxList>(datum, datum_arena,
  core_arena)` is the entry point later steps call (runs `expand_datum` then
  `elaborate`). `elaborate.hpp`/`eval_direct.hpp` were not touched by L17 and should
  not need touching by L18 — backquote is a datum-to-datum transform, not a new core
  kind.
- **`expand_datum` recurses the whole tree**, macro-expanding list heads to fixpoint,
  **except** through `datum_quote` (`'x`) and `QUOTE`-headed lists (via
  `detail::is_quote_form`). **Backquote needs the inverted rule:** a `` `template ``
  is mostly-literal but its `,`/`,@` escapes ARE code and must still be expanded. The
  plan's wording ("lowering to datum template nodes … during macro expansion")
  suggests a **dedicated template datum kind** the *expander* lowers to
  `cons`/`list`/`append` — i.e. a new `expand_datum` case, not a `host_macro` entry.
- **Emit code that calls builtins, don't build values directly** — mirror how `case`'s
  expansion emits calls to `eql`/`or` (L17). So backquote's expansion should emit a
  call to the FUNCTION-namespace `append` builtin rather than building `append` values;
  this needed no elaborator/evaluator changes at all.
- **`host_macro` registry** (`macroexpand/expander.hpp`): reuse it only if a part of
  backquote genuinely fits "symbol-headed call form → replacement datum" (maybe a
  `(backquote ...)` long form); the `` ` `` reader syntax itself will not fit.
- **Constexpr build-chain gotcha:** do not split "look up a function pointer, then
  compare to `nullptr`" across two steps in a `constexpr`/`static_assert` context —
  GCC16 rejects the comparison as non-constant. Find-and-call inside one loop
  iteration; never store a found-or-not pointer and null-check it afterward.
- **DIV-0006** records that `or`/`cond`/`case` use fixed reserved binding names
  instead of `gensym`. L18 does **not** close it (that needs `defmacro`, L19) but must
  not add a fourth ad-hoc reserved name without checking whether backquote's
  template-substitution machinery could serve double duty.

## Where changes land

- Reader-layer `` ` ``/`,`/`,@` → `src/smd/smdlisp/reader/` (deliberate exception to
  L17's "reader is stable").
- Template → `cons`/`list`/`append` lowering → `src/smd/smdlisp/macroexpand/`.
- `append` builtin: if it needs a new `list_op`/`apply_prim` case →
  `src/smd/smdlisp/closure/pairs.hpp`; if only a new `builtin_op` tag →
  `closure/env.hpp` (default install) + `closure/eval_direct.hpp`
  (`detail::to_list_op` bridge). **Do not touch `elaborator/`** — if backquote turns
  out to need a new core kind, that's a DIV (the plan frames it as expansion-time
  lowering).

## Standing constraints

- `src/smd/smdscheme/**` frozen (D1) — read-only reference, never edit.
- C++26/GCC16 baseline; Catch2; mirror file prolog / include-guard / canonical-include
  conventions used throughout `src/smd/smdlisp/`.
- File a DIV under `docs/divergences/` for anything done differently or any knowing
  ANSI CL deviation. **This lane (L18) is reserved DIV-0008** — use it, and increment
  from there (DIV-0010, DIV-0012, …) if L18 needs more than one, to stay clear of the
  L13 lane (reserved the odd slots from DIV-0007; see its brief). Do **not** grab "the
  next free number" ad hoc while both lanes run concurrently. (Numbering note:
  DIV-0005 is an existing gap and DIV-0006 is L17's; do not reuse either without
  checking `git log`.)
- L18 has **no** blog deliverable (phase 20 arrives at L19). Before handoff:
  `make compile`, `make test`, `make lint`. Do not continue past L18 into L19 unless
  blocked; if blocked, document it here.
