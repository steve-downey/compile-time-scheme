# step-brief-l13.md — Step L21: sender backend for the CL core (Track A)

Forward-only brief for the next clean agent on **Track A**. Bounded; not a log.
This file keeps Track A's lane-brief filename (`step-brief-l13.md`); it now
describes **L21**, per the step-brief contract in `AGENTS.md`.
Prior-step narrative lives in `git log`; deliberate deviations live in
`docs/divergences/`.

**Status:** L16 (special variables / dynamic binding) is merged into this
lane's branch, so L11–L16 and L17–L19 are all satisfied. `docs/compiler_architecture.org` still has **no `smdlisp` section and no UUID
anchors** — that document's own introduction defers the Common Lisp layer to
step L24, and that deferral has now been re-decided by five workers. Do not
add one; it is settled, and it is not a scope call a worker makes.

## The step (plan §L21)

Build `src/smd/smdlisp/sender/`, adapted from the Scheme sender/mendler
backends: the CL core evaluated over Beman Execution senders, using
`smd::fixpoint`'s `mendler_para` where the Scheme side does.
Make decision D5's thesis executable: `return-from`/`throw` complete the
corresponding scope's sender early; `unwind-protect` is an adapter that runs
its cleanup on `set_value`, `set_error` **and** `set_stopped`; an uncaught
`throw` surfaces on the error channel.
Port the graph-dump toolchain so a `catch`/`throw`/`unwind-protect` program's
sender graph can be rendered.

**Merge criteria:** sender-backend parity for the L11–L15 test set, plus a DOT
dump of an `unwind-protect` example.
L16 and L20 coverage may follow in-step **or** be recorded as a gap with its
own divergence doc — the plan says so explicitly, so decide early and say
which in the commit message rather than drifting.

## What L14–L16 built that L21 must reproduce or consciously drop

- **Three kinds of nonlocal control now exist, and they are distinguished by
  raw pointer identity, not by content.** `block_unwind_marker` and
  `throw_unwind_marker` (`closure/env.hpp`) are each backed by a *named*
  `inline constexpr char[] ..._marker_storage`, because a `constexpr char
  const *` initialized from a bare string literal is folded to that literal's
  address per use site and, with string-literal merging off (the Asan build),
  equal content gets distinct addresses — so the identity check passes in
  constant evaluation and silently fails at run time. If the sender backend
  introduces any new sentinel, it must use the same named-object pattern.
  See `block_unwind_marker`'s doc comment and
  `EnvTest - ThrowUnwindMarkerHasStableRuntimeIdentity`.
- **`env_arena` is where everything dynamically scoped lives**, and the reason
  is uniform: an `env` is *copied* to make a lexical capture, so anything with
  dynamic extent must not live in it. `env_arena` is threaded by mutable
  reference and never copied. It now owns three distinct things, with three
  distinct storage disciplines, and the differences are load-bearing:
  - `alloc_exit` — an append-only slab of `exit_record` (`block`). Capturable
    by a closure, so records must outlive their form; capacity bounds *total
    activations*.
  - `push_catch`/`pop_catch`/`find_catch`/`catch_depth` — a stack of
    `catch_record`. Not capturable; capacity bounds *nesting depth*.
  - `push_dynamic`/`pop_dynamic`/`dynamic_depth` — a stack of
    `dynamic_binding` (L16). Not capturable; capacity bounds nesting depth.
  Frozen `foundation::static_vector` has no `pop_back`, so both stacks carry
  an explicit live-prefix counter (`depth_`, `dyn_depth_`) as the authority.
- **Dynamic binding is shallow binding, and it is entirely a save/restore over
  one `store` cell.** A special variable has exactly one cell; binding it adds
  no binding anywhere, it overwrites that cell and restores it afterwards.
  `env::set_value` therefore needs no special case: the innermost dynamic
  binding *is* the cell, which is why `setq` of a special already works.
  The one decision point is `detail::bind_lambda_parameters`
  (`closure/eval_direct.hpp`), shared verbatim by both existing backends —
  every binding form (`let`, `let*`, `lambda`, `defun` parameters) lowers to a
  lambda application, so covering L16 in the sender backend means calling that
  one helper plus `detail::unwind_dynamic_bindings` where the sender's call
  path binds parameters. Both helpers are template-on-the-env/arena-type and
  carry no dependency on `eval_direct` itself.
- **Restore-on-every-path is achieved by having only one path, not by case
  analysis.** In both existing backends the closure-body loop `break`s instead
  of returning, so a value, an ordinary error, a `return-from` unwind and a
  `throw` unwind all leave through a single statement that restores
  unconditionally — the same shape `core_unwind_protect` has. A sender backend
  has to reconstruct that property across `set_value`/`set_error`/
  `set_stopped` rather than inherit it from C++ control flow; that is the
  substance of this step, not an incidental detail.
- **Applying a closure is a continuation barrier under CPS** (see
  `cps_apply_function_value` in `closure/cps_code.hpp`): every body form,
  including the last, is dispatched with `identity_k`/`identity_k` rather than
  tail-passing the caller's `cont`/`k`, because a tail-passed continuation
  would run while the dynamic bindings were still in effect. Expect the
  sender-graph equivalent of the same constraint: whatever completes a call's
  sender must complete *after* the restore, not before it.

## Gotchas that will bite you

- **A `static_assert` merge criterion is not evidence on its own.** Step L14
  proved a constexpr-green unwind mechanism can still be broken at run time,
  and only the runtime test caught it. Every `static_assert` needs a runtime
  `TEST_CASE` twin and you must actually run `make test`.
- **Recursive `defun` is unsupported (DIV-0009):** a `defun` captures a *copy*
  of its environment before its own name is bound. Do not write a merge
  program that relies on self-recursion; L14, L15 and L16 all had to work
  around it.
- **DIV-0013** (GCC trunk, `-fsanitize=null` under the default Asan config):
  a `static_assert` involving `compiled_lisp<Source>` can fail while its
  runtime twin passes. Read that divergence before assuming your code is wrong.
- **DIV-0014** (new, L16): dynamically binding a special that has no value
  (`(defvar *x*)` with no init) is a *diagnosed error*, and dynamic binding
  needs a mutable environment (`default_env(heap, store)`). Programs written
  for the sender backend must give specials a value first.

## Known open wart, deliberately not fixed by L16

`MaxBindings` is still hard-wired to 16 in both evaluators and in
`closure_program`/`smdlisp.hpp`'s docs. L16 was asked to parameterize it in
passing and **declined**: `value<Core>` names `closure<Core,16>` and
`foreign_function<Core>` unconditionally, so a real fix has to thread
`MaxBindings` through `value`, `store`, `pair_cell`, `pair_heap`, `apply_prim`,
`env`, `env_arena` and both evaluators (~180 sites in 7 headers) *and* edit
inside at least five existing UUID anchor blocks — `82728208` (`is_true`),
`2d0e5b9a` (`env::set_value`'s out-of-line definition), `1d30e953` and
`e6a2c1de` (`apply_prim` call sites in `eval_direct.hpp`), plus the
`cps_code.hpp` anchors — which the standing rules forbid. That combination
makes it its own step with an explicit dispensation, not a passenger on
another one. Do not half-do it here either.

## Standing constraints (unchanged)

- `src/smd/smdscheme/**` frozen for semantic changes (D1) — reference only.
- Lane = `elaborator/**` + `closure/**` + the new `sender/**` (+ tests +
  per-directory `CMakeLists.txt`). Do NOT touch `macroexpand/**` or
  `reader/**`.
- Vendor Beman Execution as the git submodule at `vendor/execution`, integrated
  with `add_subdirectory`. No FetchContent, no vcpkg, no `find_package` for it.
- Never edit inside a UUID anchor block. Add new anchors (real `uuidgen`
  output) for regions worth quoting, *with* the step — a blog post pinned to
  the step's merge tag can only transclude anchors that exist at that merge.
- C++26/GCC16; Catch2; component header included first and twice; file prolog
  with repo-relative path + Emacs mode line, then SPDX; classical include
  guards; canonical angle includes; co-located tests; constexpr-first.
- File a DIV for any knowing ANSI CL deviation. **DIV-0014 is the highest
  used**; re-check `docs/divergences/` and take the next unused number.
- Before handoff: `make compile`, `make test`, `make lint` all green, and
  `make compile` warning-free — the smdlisp tree currently has zero warnings.
- Do not write the blog draft and do not tick your `checklist.md` box until the
  orchestrator says so: blog posts are dispatched separately *after* the step's
  `--no-ff` merge, so transclusions can be pinned to a tag that exists.
- Do not continue past L21. Document blockers here.
