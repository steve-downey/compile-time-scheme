# Next steps: Common Lisp pivot — Step L13 (spine, Track A) and Step L18 (backquote, Track C)

> **2026-07-22 update (orchestrator merge):** Both **L12** (spine) and **L17** (macro expander, Track C)
> are done and merged to `main` — see `handoff.md`'s "Step L12: setq, defun, defvar, defparameter landed"
> and "Step L17: macro expander with host macros landed" sections, and `checklist.md`. The two
> next-available steps are **L13 (CPS closure backend)**, which depends on L12 (satisfied), and **L18
> (backquote)**, which depends on L17 (satisfied). Per `docs/cl-pivot-plan.md`'s parallelism summary they
> live in non-overlapping lanes — `src/smd/smdlisp/closure/` for L13, `src/smd/smdlisp/reader/` +
> `src/smd/smdlisp/macroexpand/` for L18 — and may run concurrently in separate worktrees. Part 1 below is
> L13; Part 2 is L18; whoever continues should read only their own Part plus the shared "Standing
> constraints" at the end. The only likely integration conflict between the two lanes is the tracking docs
> (`checklist.md`, `handoff.md`, this file) and the top-level `src/smd/smdlisp/CMakeLists.txt`.

## Part 1 — Step L13: CPS closure backend (spine, Track A)

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
  than the plan specifies, or any knowing ANSI CL deviation. **Next free number is DIV-0005** — L17 landed
  DIV-0006, so DIV-0005 is the next free number; check `docs/divergences/` before filing in case another
  step lands first.
- Before handoff: `make compile`, `make test`, `make lint`. Draft blog phase 18 per the plan's phase
  table (this step, unlike L12, does have a documentation deliverable).
- Do not continue past L13 into L14 (`block`/`return-from`) unless blocked; if blocked, document the
  blocker here instead.

---

## Part 2 — Step L18: backquote (Track C)

### L17 is done — what it left behind

`src/smd/smdlisp/macroexpand/{expander.hpp,expander.test.cpp,CMakeLists.txt}` landed (Track C, `docs/cl-pivot-plan.md`
step L17). Read `handoff.md`'s "Step L17: macro expander with host macros landed" section in full before
starting; the essentials L18 will build directly on top of:

- **The pipeline is now read → expand → elaborate → eval.** `lisp::macroexpand::expand_and_elaborate<MaxNodes,MaxList>(datum,
  datum_arena, core_arena) -> result<core_type<MaxNodes,MaxList>>` is the entry point every later step should
  call instead of a bare `elaborator::elaborate(...)` — it runs `expand_datum` first (macro expansion, including
  backquote once L18 lands) and then calls `elaborate` unchanged on the result. `elaborate.hpp`/`eval_direct.hpp`
  were not touched by L17 and should not need to be touched by L18 either: backquote, like `cond`/`when`/etc.,
  is a datum-to-datum transform, not a new core kind or evaluator case.
- **`expand_datum<MaxNodes,MaxList>(datum, arena) -> result<datum>`** is the whole-tree recursive expander:
  it macro-expands the head of every list position to fixpoint (`macroexpand`), then recurses into whatever
  list results, **except** through `reader::datum_quote` (the reader's `'x`) and an ordinary list headed by
  the symbol `QUOTE` (the equivalent long spelling, detected by `detail::is_quote_form`) — both are treated as
  literal data and never macro-expanded or recursed into. **Backquote needs the analogous, but inverted, rule:**
  a `` `template `` form (however L18 represents it — plan section 9 says "reader support for `` ` ``, `,`, `,@`
  lowering to datum template nodes") is mostly-literal but has escape points (`,`/`,@`) that ARE code and must
  still be macro-expanded. Decide up front whether backquote gets a new datum kind (mirroring how `datum_quote`/
  `datum_function` are dedicated reader-level nodes, per L6's "the reader preserves source reality" convention)
  or is lowered eagerly to `cons`/`list`/`append` calls at read time; the plan's own wording ("lowering to datum
  template nodes... during macro expansion") suggests the former — a dedicated template node the *expander*
  (not the reader) lowers to `cons`/`list`/`append` builds, which would make backquote itself a new
  `expand_datum` case, not a `host_macro` entry (its unquote-escape structure isn't a plain call-form the
  `host_macro` registry's `(call-form, arena) -> datum` shape fits) — read `docs/cl-pivot-plan.md` step L18
  again before committing to a shape.
- **`append` joins the builtin set** per the plan (currently `+`/`*`/`cons`/`car`/`cdr`/`list`/`null`/`eq`/`eql`/`atom`/
  `funcall`/`apply`, all in `closure::builtin_op`, `src/smd/smdlisp/closure/value.hpp`, installed into the
  FUNCTION namespace by `closure::default_env`, `src/smd/smdlisp/closure/env.hpp`). This is `closure/`-lane
  work (L9/L11's territory), not `macroexpand/`-lane work — if L18 needs `append` to *build* backquote's
  expansions at compile time (not just as a builtin the compiled program can call), check whether `pairs.hpp`'s
  `apply_prim`/`list_op` needs a new `append` case, or whether backquote's expansion can just emit a call to
  the FUNCTION-namespace `append` builtin exactly the way `case`'s expansion emits calls to `eql`/`or` (L17's
  approach — building *code* that calls existing builtins, not building *values* directly) — the latter is
  almost certainly the right pattern to reuse, since it needed no elaborator/evaluator changes at all.
- **The `host_macro` dispatch pattern** (`src/smd/smdlisp/macroexpand/expander.hpp`): if any part of backquote's
  implementation genuinely fits the "symbol-headed call form → replacement datum" shape (unlikely for the
  `` ` `` reader syntax itself, but maybe for a `(backquote ...)`-spelled long form, mirroring how `(quote ...)`
  is the long spelling `expand_datum` already special-cases alongside `'x`), reuse `host_macro`/`default_macro_table`
  rather than inventing a second registry.
- **A build-chain gotcha to avoid re-discovering:** do not split a "look up a function pointer, then compare it
  to `nullptr`" pattern across two steps inside code that will be evaluated in a `constexpr`/`static_assert`
  context — GCC16 rejects that specific comparison as non-constant even when the pointer is perfectly
  well-defined at runtime (see `handoff.md`'s L17 section for the exact error and the fix: find-and-call inside
  one loop iteration, never store a "found or not" pointer and compare it to null afterward).
- **DIV-0006** (`docs/divergences/DIV-0006-macro-temp-name-capture.md`) documents that `or`/`cond`/`case` use
  fixed reserved binding names (`%OR-TEMP`/`%COND-TMP`/`%CASE-TMP`) instead of `gensym`, because there is no
  `gensym`/backquote machinery yet. **L18 does not close this divergence** (backquote alone doesn't provide
  `gensym`; that needs `defmacro`'s compile-time evaluator, L19) but should not make it worse — any macro
  helper `expand_datum`/backquote-lowering machinery introduces should not add a fourth ad-hoc reserved name
  without checking whether backquote's own template-substitution machinery could serve double duty.

### What L18 needs to build (plan section 9, step L18)

Reader support for `` ` ``, `,`, `,@` lowering to datum template nodes, and an expansion of templates into
`cons`/`list`/`append` builds during macro expansion. `append` joins the builtin set.

**Note the plan lists this as reader-layer work** ("Reader support for `` ` ``, `,`, `,@`") — unlike L17, which
was told "do not touch `src/smd/smdlisp/reader/` ... stable since L6," L18 is explicitly expected to touch the
reader (new lexical syntax for backquote/unquote/unquote-splicing, presumably a new datum kind analogous to
`datum_quote`/`datum_function`). This is a real difference from L17's constraints — read `docs/cl-pivot-plan.md`
step L18 and decide the datum-kind shape before writing code; do not assume L17's "the expander never touches
the reader" precedent applies here.

### Merge criteria (plan section 9, step L18)

`` `(a ,x ,@ys) `` expansion tests at both datum and value level.

### Standing constraints for L18

- `src/smd/smdscheme/**` is frozen for semantic changes (D1). Read-only reference, never edit.
- Reader-layer changes for `` ` ``/`,`/`,@` land in `src/smd/smdlisp/reader/` (a deliberate exception to L17's
  "reader is stable" note, per the plan's own wording for this step — see above). Expansion-time lowering
  (template → `cons`/`list`/`append` builds) lands in `src/smd/smdlisp/macroexpand/`. If `append` needs a new
  `list_op`/`apply_prim` case, that lands in `src/smd/smdlisp/closure/pairs.hpp`; if it only needs a new
  `builtin_op` tag routed to an existing/new `apply_prim` case, that also touches `src/smd/smdlisp/closure/env.hpp`
  (default builtin installation) and `src/smd/smdlisp/closure/eval_direct.hpp` (`detail::to_list_op`'s bridge
  table). Do not touch `src/smd/smdlisp/elaborator/` — nothing about backquote should need a new core kind or
  special form; if it turns out to, that is worth a divergence doc, since the plan frames backquote as
  expansion-time lowering, not an elaborator feature.
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- File a divergence doc under `docs/divergences/` for anything done differently than the plan specifies, or any
  knowing ANSI CL deviation. **Next free number is DIV-0007** — check `docs/divergences/` for the actual latest
  `DIV-NNNN` before filing; L12 (if it landed first) may have already claimed DIV-0005, and this document's
  number may be stale by the time you read it.
- Before handoff: `make compile`, `make test`, `make lint`. L18 has **no** blog deliverable (phase 20 arrives
  at L19, `defmacro`, per the plan's phase table) — don't draft one.
- Do not continue past L18 into L19 (`defmacro`) unless blocked; if blocked, document the blocker here instead.

---

## Standing constraints (apply to both L13 and L18, and everything after)

- `src/smd/smdscheme/**` is frozen for semantic changes (D1); blog phases 5-12 transclude live code from it by
  UUID anchor. Read-only reference, never edit.
- New work goes in `src/smd/smdlisp/`, namespace `smd::smdlisp`, one sub-namespace per directory.
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- Per `docs/divergences/DIV-0004-orgit-transclusion-worktree-path.md`'s now-standing convention (resolved
  2026-07-19): if your step has a blog deliverable, author its `#+transclude:` orgit links against your own
  worktree path (e.g. `orgit:~/src/compile-time-scheme/wt-l12::...`), not `main` — the orchestrator repoints
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
