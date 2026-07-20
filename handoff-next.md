# Next steps: Common Lisp pivot, Step L13 (spine)

> **2026-07-20 update:** L12 (this worktree, `cl-pivot/l12-setq-defun`) is done — see `handoff.md`'s
> "Step L12: `setq`, `defun`, `defvar`, `defparameter` landed" section. Per `docs/cl-pivot-plan.md` section 9,
> **Step L13 depends on L12**, which is now satisfied once this branch merges to `main`. L13 is the next
> unchecked step in `checklist.md`'s spine (Track A).
>
> **Step L17 (macro expander, Track C) may be running concurrently in a separate worktree** — its status is
> *unknown to whoever wrote this file* (this worktree never touched `src/smd/smdlisp/macroexpand/` and has no
> visibility into that worker's progress). Before starting L13, check `checklist.md` and `handoff.md` for
> whether L17 has already landed on `main`; if it has, its own handoff entry supersedes anything this file
> might guess about it. If it hasn't, L13 does not depend on it (per the plan's parallelism summary, Track C
> runs parallel to Track A/B and only needs L11, already satisfied) — proceed with L13 regardless of L17's
> state. Do not touch `src/smd/smdlisp/macroexpand/` from an L13 worktree; if it doesn't exist yet, that's
> expected and not a blocker.

---

## Step L13: CPS closure backend for the baseline (spine, Track A)

### What L13 needs from L12 (this step, just landed)

L12 added four new elaborated-core kinds (`core_setq`, `core_defun`, `core_defvar`, `core_defparameter`,
all in `src/smd/smdlisp/elaborator/elaborated_core.hpp`) and their `eval_direct.hpp` evaluator cases. Read
`handoff.md`'s "Step L12" section in full before starting; the essentials L13 will lean on directly:

- **`core_f_factory`'s variant now has sixteen kinds**, not twelve: the eight from L10 (`core_integer`,
  `core_symbol`, `core_keyword`, `core_nil`, `core_true`, `core_quote`, `core_cons`, `core_if`, `core_progn`,
  `core_lambda`, `core_function`, `core_application` — actually twelve, see L10's handoff entry) plus L12's
  four (`core_setq`, `core_defun`, `core_defvar`, `core_defparameter`). **A CPS/closure-compilation pass that
  pattern-matches on `core_type`'s variant needs a case for all sixteen kinds, not just the twelve L10/L11
  ones** — the plan's own merge criterion ("every L11/L12 end-to-end test also passes through
  `compile_to_closure`") makes this explicit: L12's evaluator-level tests (`setq`/`defun`/`defvar`/
  `defparameter`) need matching CPS/closure coverage, not just L11's.
- **`env`'s mutable-mode / `store` machinery (`env.hpp`, this step) is what makes `setq`/`defun`
  self-recursion work at the tree-walking-evaluator level.** Whatever runtime environment representation the
  CPS/closure backend uses at *its* layer (which may or may not be the same `closure::env` type — check
  `smd::smdscheme::closure::cps_code.hpp`/`closure_program.hpp` for how the Scheme backend's CPS layer
  represents environments/`store`s before assuming `smdlisp`'s `env`/`store` port over unchanged) needs an
  equivalent mutation story for `setq` to work through CPS, and an equivalent reserve-then-patch (or some
  other self-reference mechanism) for `defun` self-recursion to work through CPS. Do not assume this is free
  just because the direct evaluator already has it — CPS conversion of a mutable-store-backed interpreter is
  a real design question, not a mechanical port, and the Scheme original's own `begin`/`set!` CPS wiring
  (which the plan explicitly points at as "the pattern") is the concrete reference to read first.
- **`eval_direct`'s `environment` parameter is `env<...>&` (mutable), not `const&`, as of this step** — this
  was the mechanism that made top-level sequencing "just work" via `core_progn` without any new plumbing (see
  `handoff.md`'s L12 section, "Top-level sequencing decision"). If the CPS/closure backend has its own
  environment-threading discipline, check whether it has (or needs) the analogous "the same environment
  object, not a copy, flows through a `progn`'s sequenced continuations" property — this is likely inherent to
  how CPS naturally threads state through continuations, but confirm rather than assume, since it is exactly
  what let `defun` inside a `progn` see itself later in the same `progn`.
- **`defun` returns the function name (a `closure::symbol`); `setq` returns the assigned value; `defvar`/
  `defparameter` return the variable name (a `closure::symbol`).** All ANSI CL return-value behaviors, not
  Scheme conventions (Scheme's `define`/`set!`/`begin` mostly return an `unspecified` value the Scheme
  original's CPS backend does not have to thread meaningfully) — the CPS backend's compiled representation of
  these four forms needs to produce these specific values as their continuation's argument, not `unspecified`
  or any other placeholder.
- **A durable lifetime lesson repeated at every layer so far (L10, L11): a `value<Core>`'s `symbol`/`keyword`
  hold a bare `std::string_view`, not an owned spelling.** Any CPS/closure-compiled representation that
  produces a `symbol`/`keyword` value (e.g. `defun`'s return value, or a quoted symbol/keyword) needs to keep
  whatever it points into (a core node, an arena) alive for as long as the produced value might be inspected.
  Read L10's and L11's handoff entries for the exact shape of this bug before writing any new test helper that
  returns a value out of a function.

### What L13 needs from L10/L11 (unchanged this step)

Not re-explained here — read `handoff.md`'s "Step L10" and "Step L11" sections — but headline facts: the
direct evaluator (`eval_direct.hpp`) is the semantic reference implementation; the CPS/closure backend must
match its observable behavior exactly (same merge criteria, same error messages where the plan's merge
criteria check them). `apply_function_value` is the single call-dispatch point in the direct evaluator; if the
CPS backend needs an analogous single dispatch point, it likely needs its own version rather than reusing
`apply_function_value` directly (CPS calling convention differs from direct evaluation's call/return).

### Merge criteria (plan section 9, step L13)

> Every L11/L12 end-to-end test also passes through `compile_to_closure`.

This is a broad criterion, not a single expression: it means the CPS/closure backend needs enough coverage to
re-run (or equivalently re-express) L11's and L12's existing `eval_direct.test.cpp`/`elaborate.test.cpp`
scenarios through whatever `compile_to_closure`-equivalent entry point this step builds, not just add one new
narrow test. Read `src/smd/schemepoc/closure/` (the original Scheme-flavored closure backend under the
`schemepoc` project, if still present) and `src/smd/smdscheme/closure/{cps_code.hpp,closure_program.hpp}`
(the Scheme CL-pivot's own already-landed CPS backend, frozen but readable) before designing `smdlisp`'s
version — the plan explicitly says "adapted from the Scheme CPS backend per the architecture in
`docs/cps-direction.md`."

### Standing constraints for L13

- `src/smd/smdscheme/**` is frozen for semantic changes (D1). Read-only reference, never edit.
- New/changed files land in `src/smd/smdlisp/closure/` (new `cps_code.hpp`/`closure_program.hpp` + tests,
  possibly further `eval_direct.hpp`/`env.hpp` touch-ups if a genuine bug is found, matching L11's and L12's
  own precedent of touching pre-existing closure/ files when a step's own new code needs it). Do not touch
  `src/smd/smdlisp/reader/` or `src/smd/smdlisp/macroexpand/` (the latter is L17's lane; see the note at the
  top of this file about its unknown-to-this-worker status).
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- File a divergence doc under `docs/divergences/` (`TEMPLATE.md` skeleton) for anything done differently than
  the plan specifies, or any knowing ANSI CL deviation. **Next free number is DIV-0006** (L12 claimed DIV-0005
  for `defvar`/`defparameter`'s value-form requirement — see `docs/divergences/
  DIV-0005-defvar-defparameter-require-value-form.md`). **Re-check `docs/divergences/` immediately before
  filing** — if L17 landed on `main` first and also claimed a number in this range, renumber to stay unique.
- **This step has a blog deliverable: phase 18**, per the plan's phase table. Follow L6/L10/L11's precedent
  for authoring `#+transclude:` orgit links against this step's own worktree path (see the "Standing
  constraints" section below and `docs/divergences/DIV-0004-orgit-transclusion-worktree-path.md`'s now-standing
  convention) — the orchestrator repoints them to `main` after merge.
- Before handoff: `make compile`, `make test`, `make lint`, and (since this step has a blog deliverable)
  `make blog-md`, checking that only phase 18's `.md` and `index.md`/`index.org` show real content diffs (see
  the org-markup footguns noted below).
- Do not continue past L13 into L14 (`block`/`return-from`) unless blocked; if blocked, document the blocker
  here instead.

---

## Standing constraints (apply to L13 and everything after)

- `src/smd/smdscheme/**` is frozen for semantic changes (D1); blog phases 5-12 transclude live code from it by
  UUID anchor. Read-only reference, never edit.
- New work goes in `src/smd/smdlisp/`, namespace `smd::smdlisp`, one sub-namespace per directory.
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- Per `docs/divergences/DIV-0004-orgit-transclusion-worktree-path.md`'s now-standing convention (resolved
  2026-07-19): if your step has a blog deliverable, author its `#+transclude:` orgit links against your own
  worktree path (e.g. `orgit:~/src/compile-time-scheme/wt-l13::...`), not `main` — the orchestrator repoints
  them to `main` after merge. No fresh divergence doc needed per occurrence. Watch for two org-markup
  footguns L11 hit while drafting phase 17 (see `handoff.md`'s L11 entry for the full explanation): adjacent
  `~verbatim~/~verbatim~` spans with no space between them merge into one giant span in GFM export, and a
  `~word~` immediately followed by a bare letter (e.g. `~static_assert~s`) has the same problem — always
  leave a space, or plain punctuation, immediately after a closing `~`.
- Before handing off any step, verify `make blog-md` (if the step has a blog deliverable) leaves every
  *other* phase's `.md` unchanged — `git status --short docs/blog/` after the render should show only your
  new/updated phase and `index.md`/`index.org`. If other `.md` files show diffs, `git diff` them first: if
  it's just `id="orgXXXXXXX"` churn, `git checkout --` them before committing (matching L6's and L11's
  precedent); if the diff has real content changes, investigate before reverting — L11 hit a case where three
  unrelated phases' `.md` were stale/blank in the committed tree and got correctly filled in by the render,
  which also needed reverting since it's out of lane, but don't assume every case is that same story.
