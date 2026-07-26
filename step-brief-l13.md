# step-brief-l13.md — Step L16: special variables and dynamic binding (Track A)

Forward-only brief for the next clean agent on **Track A**. Bounded; not a log.
This file keeps Track A's lane-brief filename (`step-brief-l13.md`); it now
describes **L16**, per the step-brief contract in `AGENTS.md`. Prior-step
narrative lives in `git log`; deliberate deviations live in
`docs/divergences/`. `docs/compiler_architecture.org` still has **no
`smdlisp` section and no UUID anchors** — it documents the pre-pivot Scheme
PoC only, so there is currently nowhere in it to record an `smdlisp`
cross-step fact in place. Do not invent a section; that is an orchestrator
scope decision.

**Status:** L15 (`catch`/`throw`/`unwind-protect`) is merged into this lane's
branch and L16 depends on L12 and L15 (both satisfied). Track C's L19
(`defmacro`) is already merged to `main`; this branch has `main` merged in, so
`macroexpand/**` and `reader/**` are current — still not your lane. Likely
integration conflict surfaces remain `checklist.md` and
`src/smd/smdlisp/**/CMakeLists.txt`.

## The step (plan §9, L16)

`defvar`/`defparameter` already *mark* a name special (`env::mark_special` /
`env::is_special`, step L12) but nothing yet *behaves* differently. L16 makes
the mark mean something: a binding form (`let`/`let*`/lambda parameters) that
binds a name marked special must establish a **dynamic** binding — visible to
every function called during the binding's extent, regardless of lexical
scope — and must restore the previous value on **every** exit path out of that
extent, including the non-local ones L14/L15 built.
**Parity required:** implement in BOTH `eval_direct.hpp` and the CPS backend
`cps_code.hpp`; every merge program must pass on both, exactly as L14 and L15
required.

## What L15 built that L16 should reuse

- **`unwind-protect`'s discipline is exactly the restore-on-every-exit
  discipline you need.** `eval_direct.hpp`/`cps_code.hpp`'s
  `core_unwind_protect` case is a single unconditional loop rather than a
  four-way case analysis, because a value, an ordinary error, a `return-from`
  unwind and a `throw` unwind all already arrive as one `result<value<Core>>`.
  A dynamic rebinding's restore step has the identical shape: do the work,
  regain control unconditionally, restore, then re-return whatever arrived.
  Model the special-binding save/restore on that case, not on a new mechanism.
- **`env_arena` now owns a real dynamic stack** (`push_catch`/`pop_catch`/
  `find_catch`/`catch_depth`, `env.hpp`). It is the right home for a
  special-variable binding stack too, and for the same reason: `env` is copied
  for lexical capture, so anything dynamically scoped must *not* live there,
  while `env_arena` is threaded by mutable reference and never copied. Note
  the deliberate contrast documented on `push_catch`: the catch stack pops and
  reuses slots (capacity bounds nesting depth), unlike `alloc_exit`'s
  append-only slab (capacity bounds total activations). A special-binding
  stack wants the popping kind.
- `env::is_special` is already the query you need at binding time, and it is
  already carried across `env` copies.

## Gotchas that will bite you

- **Sentinel markers are compared by RAW POINTER IDENTITY, and a bare string
  literal will pass in `constexpr` and fail at run time.** `block_unwind_marker`
  (L14) and `throw_unwind_marker` (L15) are each backed by a *named*
  `inline constexpr char[] ..._marker_storage` for exactly this reason: a
  `constexpr char const *` initialized from an anonymous literal is folded to
  that literal's address per use site, and with string-literal merging off (the
  Asan build) equal content gets distinct addresses. **Any new marker L16 adds
  must use the named-object pattern.** See `block_unwind_marker`'s doc comment
  in `env.hpp`, and `EnvTest - ThrowUnwindMarkerHasStableRuntimeIdentity`.
- **Corollary: `static_assert` merge-criteria tests are not sufficient alone.**
  Every constexpr test needs a runtime `TEST_CASE` twin, and you must actually
  run `make test`. This cost L14 the most time and is why L15 pairs all three
  merge criteria.
- **Recursive `defun` is unsupported (DIV-0009).** A closure captures a *copy*
  of the environment before its own name is bound. Do not write a merge
  program that relies on recursion; use iterative or lexically-nested
  formulations. L14 and L15 both had to.
- `env::set_value` is `const` and writes through the shared `store`; a special
  binding that shadows a lexical one has to decide which cell `setq` hits.
  Settle that explicitly and test it, in both evaluators.

## Standing constraints (unchanged)

- `src/smd/smdscheme/**` frozen for semantic changes (D1) — reference only.
  In particular `foundation::static_vector` has **no `pop_back`**; L15's catch
  stack works around that with an explicit live-prefix depth counter
  (`env_arena::depth_`). Reuse that pattern rather than editing the frozen tree.
- Lane = `elaborator/**` + `closure/{eval_direct,cps_code,closure_program,
  value,env}.hpp` (+ tests + per-dir `CMakeLists.txt`). Do NOT touch
  `macroexpand/**` or `reader/**` (Track C).
- Never edit inside a UUID anchor block anywhere.
- C++26/GCC16; Catch2; component header included first and twice; file prolog
  with repo-relative path + Emacs mode line, then SPDX; classical include
  guards; canonical angle includes; co-located tests; constexpr-first.
- File a DIV for any knowing ANSI CL deviation. **DIV-0011 is the highest used**
  (L15); re-check `docs/divergences/` before filing and take the next unused.
- Before handoff: `make compile`, `make test`, `make lint` all green.
- Do not write the blog draft. The orchestrator dispatches blog posts
  separately, *after* the step's `--no-ff` merge, so transclusions can be
  pinned to a `blog/phase-NN` tag that only exists post-merge. Write nothing
  under `docs/blog/`, and do not tick your step's `checklist.md` box.
- Do not continue past L16 into L17. Document blockers here.

## Open defect found during L15 (not fixed, deliberately out of lane scope)

`cps_code.hpp`'s `cps_apply_function_value` builtin `switch` does **not** list
`builtin_op::append`, so `(append ...)` under the CPS backend falls through to
`unknown builtin`. `eval_direct.hpp` handles it correctly. This is a live
`-Wswitch` warning on every build of `cps_code.hpp` and the only warning in
the smdlisp tree. It matters because backquote (L18) lowers `,@x` onto that
builtin, so splicing works under `eval_direct` and fails under CPS. The fix is
to add `case builtin_op::append:` to the existing `cons`…`atom` group, plus a
parity test. Left alone by L15 to keep the step to its stated scope; whoever
picks it up should do it as its own small commit with its own test.
