# step-brief-l13.md — Step L15: catch / throw / unwind-protect (Track A)

Forward-only brief for the next clean agent on **Track A**. Bounded; not a log.
This file keeps Track A's lane-brief filename (`step-brief-l13.md`); it now
describes **L15**, per the step-brief contract in `AGENTS.md`. Prior-step
narrative lives in `git log`; architecture lives in
`docs/compiler_architecture.org`; deliberate deviations live in
`docs/divergences/`.

**Status:** L14 (`block`/`return-from`) is merged; L15 depends on L14
(satisfied). Track C runs concurrently in a non-overlapping lane. Likely
integration conflict surfaces remain `checklist.md` and
`src/smd/smdlisp/CMakeLists.txt` (and any per-dir `CMakeLists.txt` you add).

## The step (plan §9, L15)

`catch` establishes a **dynamic** exit keyed by an *evaluated* tag value
(`eq` comparison); `throw` searches the dynamic exit stack innermost-first;
both share the one-shot exit machinery L14 built (see below). `unwind-protect`
registers cleanup forms that run on **every** exit path through its extent:
normal completion, `return-from`, and `throw`; in CPS this must wrap both the
normal continuation and the escape path.
**Parity required:** implement in BOTH `eval_direct` and the CPS backend; the
merge programs must pass on both, exactly as L14 required.
Merge criteria: cleanup-ordering tests (innermost cleanups first); `throw`
through nested `unwind-protect`; uncaught `throw` is a diagnosed error.
Deliverable: blog phase 19 draft (covers L14's material, one step behind, per
the docs-lag-code-by-one-step pattern).

## What L14 built that L15 reuses — and the one gotcha that will bite you

L14's non-local-exit mechanism lives in `src/smd/smdlisp/closure/env.hpp` and
is reused verbatim by both evaluators (`eval_direct.hpp`, `cps_code.hpp`,
`core_block`/`core_return_from` cases):

- **`exit_record<Core>` + `env_arena::alloc_exit`** — one fresh, live record
  per dynamic activation, arena-owned (stable pointer, survives closure
  capture). `env::define_block`/`lookup_block` are a third, independent
  namespace (D4) threaded on the *ambient* env by mutable reference (never a
  copy), the same discipline `progn`/`setq`/`defun` rely on. `catch`/`throw`
  want the same shape, but keyed by an evaluated **tag value** (`eq`), not a
  lexical name — so the tag lives in a *dynamic* stack, not the lexical block
  namespace. Consider a parallel dynamic exit stack in `env`/`env_arena`
  rather than overloading the block namespace.
- **The unwind travels through the frozen `result<value<Core>>` error channel**
  as a `parse_error` whose `message` is a single sentinel pointer
  (`block_unwind_marker`); every existing `!has_value()` guard already "skips
  intervening frames" for free. `throw` will want its own analogous sentinel.

- **GOTCHA (this is what silently broke L14 at run time and cost the most
  time):** the sentinel is compared by **raw pointer identity**
  (`err.message == block_unwind_marker`). A `constexpr char const *`
  initialized directly from a *bare string literal* is constant-folded to the
  literal's address at each use site; with string-literal merging off (Asan
  build) those sites get **distinct addresses for identical content**, so the
  identity check passes in `constexpr` (all the merge-criteria `static_assert`s
  went green) yet **fails at run time**, and the unwind escapes its handler.
  The fix, already in `env.hpp`: back the sentinel with a **named**
  `inline constexpr char[] block_unwind_marker_storage` and point the
  `char const *` at it — one named object has one address. **Any new sentinel
  L15 adds (e.g. a `throw` marker) MUST use this named-object pattern, not a
  bare literal.** See `block_unwind_marker`'s doc comment in `env.hpp`.

- `unwind-protect` cleanup must run even while a `block_unwind_marker` /
  throw-marker error is propagating — i.e. on the `!has_value()` path, not only
  the value path. In CPS specifically, the cleanup wraps both continuations.
  Model it on how `core_block` intercepts, runs its own logic, then re-returns
  the propagating error unchanged when the unwind is not aimed at it.

## Standing constraints (unchanged)

- `src/smd/smdscheme/**` frozen for semantic changes (D1) — reference only.
- Lane = `elaborator/**` + `closure/{eval_direct,cps_code,closure_program,
  value,env}.hpp` (+ tests + per-dir `CMakeLists.txt`). Do NOT touch
  `macroexpand/**` or `reader/**` (Track C).
- C++26/GCC16; Catch2; header included first and twice; mirror file prolog /
  include-guard / canonical-angle-include conventions.
- Every public `constexpr` API gets a `static_assert`/constexpr test. Note the
  `static_assert` merge-criteria tests are **not sufficient on their own** —
  L14 proved a constexpr-green mechanism can still fail at run time (the marker
  gotcha). Always add the runtime `TEST_CASE` twin and run `make test`.
- File a DIV for any knowing ANSI CL deviation. Next free Track-A number is
  **DIV-0010 or higher** — but DIV-0010 is claimed by the concurrent Track-C
  lane, so re-check `docs/divergences/` before filing and take the next unused.
- Before handoff: `make compile`, `make test`, `make lint` all green. If the
  blog deliverable lands, verify `make blog-md` leaves every *other* phase's
  `.md` unchanged and hand-edit `docs/blog/index.{org,md}` for just the new
  entry (don't commit the regenerated, anchor-churned `index.md`).
- Do not continue past L15 into L16 unless blocked; document blockers here.

## L14 discoveries L15 must account for

- **Recursive `defun` is unsupported — DIV-0009.** A closure captures a *copy*
  of the environment before its own name is bound, so a function cannot call
  itself (`undefined function` at run time); this is independent of
  block/throw. Do **not** write a merge program that relies on recursion
  (the predecessor's recursive `block` test was replaced with a non-recursive
  same-name-nested-block witness for exactly this reason). Iterative / nested
  formulations only, until a self-reference mechanism is added.
- L14's own-activation property is witnessed by
  `SameNameNestedBlockReturnFromTargetsInnermost` (both evaluators):
  `(block b (+ 100 (block b (return-from b 5)))) => 105`. `catch`/`throw`
  ordering tests should follow the same non-recursive, lexically-nested style.
