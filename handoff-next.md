# Next step: Common Lisp pivot, Step L13 (CPS closure backend)

> **2026-07-22 update:** L12 (this worktree) is done — see `handoff.md`'s "Step L12: setq, defun,
> defvar, defparameter landed" section. Per `docs/cl-pivot-plan.md` section 9, **Step L13 depends on
> L12**, now satisfied once this branch merges to `main`. L13 is the next unchecked step in
> `checklist.md`'s spine (Track A).
>
> **Status of L17 (macro expander, Track C), as of this branch's fork point:** `checklist.md` on
> `main` still shows L17 unchecked, and this worktree never touched `src/smd/smdlisp/macroexpand/`
> (that directory does not exist in this branch). Per the plan's parallelism summary, L17 "may run in
> parallel" with L12 through L16 in a separate worktree. **Before starting L13, check `checklist.md` on
> current `main` for whether L17 has landed** — if it has, this note is stale; if not, L13 can proceed
> exactly as below, since L13's file scope (`src/smd/smdlisp/closure/`) does not overlap L17's
> (`src/smd/smdlisp/macroexpand/`) at all, only the top-level `src/smd/smdlisp/CMakeLists.txt` is a
> possible merge-conflict point (an `add_subdirectory` line each step adds).

## What L13 needs from L12 (this step)

L12 landed `setq`/`defun`/`defvar`/`defparameter` on top of L11's direct evaluator. Read `handoff.md`'s
"Step L12" section in full before starting; the essentials L13 will lean on directly:

- **Fifteen core kinds now exist** in `elaborated_core.hpp`'s `core_f_factory` variant: the twelve from
  L10 (`core_integer`, `core_symbol`, `core_keyword`, `core_nil`, `core_true`, `core_quote`,
  `core_cons`, `core_if`, `core_progn`, `core_lambda`, `core_function`, `core_application`) plus three
  new ones from L12 (`core_setq`, `core_defun`, `core_defvar`). **The CPS backend must cover all
  fifteen** — the plan's own merge criterion says "every L11/L12 end-to-end test also passes through
  `compile_to_closure`," so `core_setq`/`core_defun`/`core_defvar` are in scope for L13, not deferred.
- **`environment` is now a mutable reference (`env<Core, MaxBindings> &`), not `const &`,** throughout
  `eval_direct.hpp` — this is what lets `defun`/`defvar`/`defparameter`/`setq` mutate the environment in
  place and have *later* sibling expressions (evaluated with the same threaded environment reference,
  e.g. `core_progn`'s and a lambda body's sequential loops) observe the mutation. **Any CPS/continuation
  threading L13 builds needs to preserve this same property**: whatever plays the role of "the
  environment" in the CPS backend must be mutated in place and shared across a `progn`/lambda-body
  sequence's continuation chain, not copied fresh per step, or `(progn (defun f ...) (f ...))` and
  `((lambda (x) (setq x 2) x) 1)` will regress under CPS even though they work under `eval_direct`. The
  Scheme original's landed `begin`/`core_begin` CPS wiring (`smd::smdscheme::closure::cps_code.hpp`) is
  the plan's named pattern for `progn`'s continuation-chaining shape — read it before designing this,
  since `smdscheme`'s CPS backend evidently already solved (or sidestepped) the equivalent problem for
  `set!`, which is the same class of issue.
- **`setq` returns the assigned value, not `void`** (`env::set_value(symbol, value<Core>) const ->
  foundation::result<value<Core>>`) — a CPS `core_setq` continuation must pass that value forward, not a
  Scheme-style `unspecified` marker (there is no `unspecified` kind in `smdlisp`'s `value<Core>` at all;
  don't introduce one).
- **`store<Core, MaxStore>` (`value.hpp`, adapted from `smd::smdscheme::closure::store`) backs `setq`.**
  `env<Core, MaxBindings>` operates in one of two modes: functional (no store, `setq` is a diagnosed
  error) or mutable (constructed with a `store<Core, default_max_store>*`, shared across every `env`
  copy including closure captures, so `setq` on a captured variable is visible to every closure sharing
  the binding — see `EvalDirectTest - SetqIsVisibleAcrossClosuresSharingTheBinding` in
  `eval_direct.test.cpp` for the shared-mutable-closure idiom this makes correct). **The CPS backend's
  environment representation needs an equivalent mutable-mode story**, or `setq` will only be
  observable within a single call frame rather than across closures that share a binding — get this
  right by construction (mirror the store-pointer-sharing approach) rather than discovering the gap via
  a failing shared-closure test the way L9/L11 discovered their own architecture gaps.
- **`env` gained `mark_special(symbol) -> void` / `is_special(symbol) const -> bool`**
  (`static_vector<symbol, MaxBindings> special_`, independent of `values_`/`functions_`/the store).
  These currently have zero runtime effect beyond being queryable — L16 is where dynamic (re)binding
  behavior for special variables actually arrives. **L13 does not need to build any dynamic-binding
  machinery**; it only needs `core_defvar`'s CPS lowering to call the CPS-backend equivalent of
  `mark_special`/optionally-`define_value`, exactly mirroring `eval_direct`'s `core_defvar` case (see
  below).
- **`defun`/`defvar`/`defparameter` all return `value<Core>{symbol{name}}`** (the function/variable
  name), never the closure/init value — this is ANSI CL's actual return-value contract for these forms,
  not an incidental choice; a CPS lowering that returns something else would be observably wrong (e.g.
  `(print (defun f (x) x))` should print `F`, not a closure).
- **`elaborate_lambda_body` (`elaborate.hpp`) is now the single shared formals/body-elaboration
  function** behind both `lambda` and `defun` — `core_defun.lambda_node` is a genuine `core_lambda` node
  in the arena (same shape `#'(lambda ...)` produces), so **`defun`'s CPS lowering can and should reuse
  whatever CPS lowering L13 builds for `core_lambda`/closure-materialization**, then additionally emit
  the function-namespace-definition side effect and the name-as-return-value. Do not build a second,
  parallel "compile a lambda under CPS" path for `defun`.
- **Top-level sequencing needs no new machinery, confirmed this step.** A program wrapped in one
  top-level `progn` (the plan's own merge-criterion shape, e.g.
  `(progn (defun twice (x) (+ x x)) (twice 4))`) elaborates to one `core_progn` node and Just Works once
  environment mutation is threaded correctly — no separate "run N top-level forms" driver exists or was
  needed. A genuine multi-datum program with **no** enclosing `progn` is still unsupported by anything in
  the codebase (Scheme or Lisp side) — `read_datum` reads exactly one datum and stops. This is very
  unlikely to be L13's problem (L13 compiles a single elaborated `core_type` tree, same as `eval_direct`
  does), but if `compile_to_closure`'s test harness wants to exercise multiple top-level forms, wrap them
  in an explicit `progn` exactly the way L12's own tests do, rather than inventing a multi-form reader
  entry point (that is out of scope here and belongs to a later step, plausibly L22's public API).

## What L13 needs from L11/L10/L9 (unchanged this step, background only)

Not re-explained here — read `handoff.md`'s "Step L9", "Step L10", and "Step L11" sections for the full
architecture (Lisp-2 lookup with no cross-namespace fallback, `nil`/`t` truthiness via one `is_true`
function, closures capturing both namespaces via a raw pointer into an `env_arena`, `funcall`/`apply`
real call semantics through the single `apply_function_value` dispatch point, the `MaxBindings == 16`
`static_assert` wart still open). All of this is architecture the CPS backend needs to reproduce (or
explicitly and knowingly diverge from, with a divergence doc), not just "not break."

## What L13 needs to build

Per `docs/cl-pivot-plan.md` step L13:

`src/smd/smdlisp/closure/{cps_code.hpp,closure_program.hpp}` (+ tests), adapted from the Scheme CPS
backend (`smd::smdscheme::closure::cps_code`/`closure_program`) per the architecture decision recorded
in `docs/cps-direction.md`: **structural recursion over the flat arena** (Option A), not `fix<F>`-based
folding (Option B, rejected because `fix<F>` is not a literal type — a `cps_of`-style function dispatches
on the node variant, recurses by `node_id`/`arena_box`, and threads the continuation as a template
parameter or similar compile-time-native-continuation mechanism). Read `docs/cps-direction.md` in full
before starting; it is short and explains exactly why Option A was chosen over Option B for this
project, a decision this step inherits rather than re-litigates.

Read `smd::smdscheme::closure::cps_code.hpp` and `closure_program.hpp` in full before starting — this is
the "adapt by copy, then diverge" component (per D1 / the plan's reuse inventory), so the Scheme
original's shape is the starting point, not a reference to glance at. Pay particular attention to how it
threads `begin`/`core_begin` (the closest existing analogue to `smdlisp`'s `core_progn`) through
continuation chaining — the plan explicitly names this as the pattern `core_progn` compilation should
follow, and per the note above, it is also the closest existing precedent for "does the Scheme CPS
backend already solve environment-mutation-visibility-across-a-sequence the way L12 needed `eval_direct`
to."

**Merge criterion** (plan section 9, step L13): every L11/L12 end-to-end test also passes through
`compile_to_closure` (or whatever the `smdlisp` equivalent entry point ends up named — check what
`smdscheme::closure::closure_program.hpp` calls its own public compile entry point and mirror that
naming unless there's a concrete reason not to). Concretely this means the L11 merge-criteria three
(`(if nil 1 2)` ⇒ `2`; `((lambda (x) (car (cdr x))) '(1 2 3))` ⇒ `2`; `(funcall #'cons 1 nil)` ⇒ a
`pair_ref` cell) and the L12 merge criterion (`(progn (defun twice (x) (+ x x)) (twice 4))` ⇒ `8`) all
need to produce the same results when run through the CPS/closure-program path as they do through
`eval_direct` — plus, per the plan's phrasing "every L11/L12 end-to-end test," it is worth deliberately
re-running (not just the three/one merge criteria but) a representative sample of the fuller L11/L12
`eval_direct.test.cpp` test suite (closures capturing both namespaces, `apply`/`funcall` real call
semantics, `setq`'s shared-store-across-closures idiom, `defvar`/`defparameter`'s init-vs-no-reinit
rules) through the CPS path, since those are exactly the cases most likely to expose an environment-
threading regression under continuation-passing style that a narrower "just the merge criteria" test
set would miss.

**Deliverable: blog phase 18 draft** (`docs/blog/phase-18-setq-defun-progn.org`, working title "`setq`,
`defun`, `progn`: a programmable core" per the plan's phase table) — this step's blog post covers L12's
material (this is a docs-lags-code-by-one-step pattern already established: L11's phase-17 draft covered
L9/L10's Lisp-2/nil-t material once the evaluator made it end-to-end testable; L13's phase-18 draft
should cover `setq`/`defun`/`progn`'s *semantics*, using L13's CPS test suite as the source of live
UUID-anchored code, now that CPS compilation makes `(progn (defun twice (x) (+ x x)) (twice 4))` an
actual compile-time-computed demo rather than just a direct-evaluator fact). Follow DIV-0004's
now-standing convention: author `#+transclude:` links against this step's own worktree path, not `main`;
the orchestrator repoints them after merge. Watch for the two org-markup footguns L11's phase-17 draft
hit (documented in `handoff.md`'s L11 section): adjacent `~verbatim~` spans need a space or punctuation
between them, and a `~word~` immediately followed by a bare letter has the same problem.

## Standing constraints for L13

- `src/smd/smdscheme/**` is frozen for semantic changes (D1). Read-only reference — this step reads
  `smdscheme::closure::cps_code.hpp`/`closure_program.hpp` closely but does not edit them.
- New/changed files land in `src/smd/smdlisp/closure/` only (the two new files plus their tests). Do not
  touch `src/smd/smdlisp/reader/`, `src/smd/smdlisp/elaborator/`, or `src/smd/smdlisp/macroexpand/`
  (L17's lane, if still in flight — see the status note at the top of this file).
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- File a divergence doc under `docs/divergences/` (`TEMPLATE.md` skeleton) for anything done differently
  than the plan specifies, or any knowing ANSI CL deviation. **Next free number is DIV-0005** (L12 did
  not need one — every deviation discussed in its handoff entry was an internal C++ architecture
  decision the plan explicitly invited, a strict reduction in ANSI divergence, or a pre-existing gap the
  plan's own merge criterion was written to sidestep, not a new cut). **Check `docs/divergences/` for the
  actual latest number before filing** — a parallel L17 worker holds **DIV-0006**, so if L17 landed
  first and also needed a number, DIV-0005 might already be taken; do not assume this document's number
  is still accurate.
- Before handoff: `make compile`, `make test`, `make lint`. Draft blog phase 18 per the plan's phase
  table (this step, unlike L12, does have a documentation deliverable).
- Do not continue past L13 into L14 (`block`/`return-from`) unless blocked; if blocked, document the
  blocker here instead.
