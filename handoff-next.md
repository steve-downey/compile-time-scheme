# Next steps: Common Lisp pivot, Step L12 (spine) and Step L18 (backquote, Track C)

> **2026-07-22 update:** L17 (macro expander with host macros) is done — see `handoff.md`'s "Step L17: macro
> expander with host macros landed" section. Part 2 below now covers **L18 (backquote)**, the next Track C
> step, instead of L17. Part 1 (L12) is **carried forward unchanged from the previous revision of this file**:
> this L17 worker did not check whether L12 has merged to `main` yet (L12 runs in a separate, concurrent
> worktree per the plan's parallelism summary), so treat Part 1 as still-current unless `checklist.md` on
> `main` already shows L12 checked off, in which case skip straight to Part 2.

> **2026-07-20 note (retained):** L11 (worktree `cl-pivot/l11-eval-direct`) is done — see `handoff.md`'s
> "Step L11: direct evaluator landed (+ phase 17 draft)" section. Per `docs/cl-pivot-plan.md` section 9,
> **Step L12 depends on L11**, which is satisfied once that branch merges to `main`. L12 is the next unchecked
> step in `checklist.md`'s spine (Track A).

---

## Part 1 — Step L12: `setq`, `defun`, `defvar`, `defparameter` (spine, Track A)

### What L12 needs from L11 (this step)

L11 built `src/smd/smdlisp/closure/eval_direct.hpp` (+ tests): the structural-recursive evaluator. Read
`handoff.md`'s "Step L11" section in full before starting; the essentials L12 will lean on directly:

- **Evaluator API:** `eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(node, arena, environment, envs) ->
  foundation::result<value<Core>>`. `MaxBindings` must be `16` (`static_assert`ed — `value<Core>`'s
  `closure<Core>` alternative is still hard-wired to `MaxBindings == 16` in the `value.hpp` alias; this wart
  is still open, worker discretion whether to fix it in passing). `MaxEnvs` is the capacity of the new
  `env_arena` (below).
- **`apply_function_value<MaxNodes, MaxList, MaxBindings, MaxEnvs>(func_val, args, arena, heap, envs) ->
  foundation::result<value<Core>>`** is the single call-dispatch point — `core_application`, `funcall`, and
  `apply` all go through it. If `defun`'s call semantics need anything beyond what ordinary application
  already does (they shouldn't — `defun` only changes *how a function gets bound*, not how it's called),
  extend this function rather than adding a second dispatch path.
- **Lisp-2 lookup (D4) is fully wired**: `core_symbol` → `env::lookup_value` only; `core_function` (an
  application head, `#'name`, `(function name)`, or an embedded `(lambda ...)`) → `env::lookup_function` only,
  or a direct recursive evaluation for the embedded-lambda case. Neither path falls back to the other, ever.
  `setq`/`defun`/`defvar`/`defparameter` must preserve this: `defun` defines in the function namespace,
  `defvar`/`defparameter`/`setq` (of a lexical) define/mutate in the variable namespace, and nothing should
  need to search both.
- **`env<Core, MaxBindings>` (`env.hpp`) is still read-only once bound** — `define_value`/`define_function`
  exist, but there is no `set_value`/mutate-in-place operation yet. **This is explicitly L12's job.** L9's
  `env` has no backing `store` (unlike the Scheme original, which grew one for `set!` — see
  `smd::smdscheme::closure::store`/`env::assign` in `src/smd/smdscheme/closure/value.hpp` for the pattern to
  adapt). The plan's own guidance (`docs/cl-pivot-plan.md` step L12): adapt the *landed* `set!` machinery —
  `core_set`, the `store` class of mutable binding cells, and its wiring through `smdscheme`'s evaluator — as
  the basis for `setq`. Concretely this likely means: `env.hpp` gains a `store<Core, MaxStore>`-shaped
  companion (or `smdlisp` adapts `smdscheme::closure::store` by copy, matching L7-L10's "adapt by copy, then
  diverge" pattern), `env::define_value` grows a mode where a binding names a store location rather than
  holding its value inline (exactly `smdscheme::closure::env`'s "functional vs. mutable" dual-mode design,
  already summarized in `handoff.md`'s L9 section), and a new `env::set_value(symbol, value) -> result<value>`
  (or similar name — the plan sketch in section 9 literally writes `set_value(symbol, value) -> result<void>`,
  but note `setq` **returns the assigned value per CL, not Scheme's `unspecified`** — the plan says so
  explicitly: "`setq` returns the assigned value per CL, not the Scheme `unspecified`" — so the return type
  should probably be `result<value<Core>>` echoing the assigned value, not `result<void>`; make the call and
  write down which one you picked and why, the same way L11 had to make the `core_true` representation call).
- **`env_arena<Core, MaxBindings, MaxEnvs>` (`env.hpp`, new this step)** owns every captured environment; a
  closure's `captured` pointer is a stable pointer into it, never a stack-local. If `defun`/`defvar`
  materialize any new closures or capture any new environments (they shouldn't need to at the top level, but
  double-check), route through the same arena, don't invent a second ownership mechanism.
- **`env::pairs() -> pair_heap<Core, default_max_pairs>*`** (new this step, mirrors
  `smd::smdscheme::closure::env`'s `pairs_`/`pairs()`) is how `cons`/`car`/`cdr`/`list` and `core_cons`
  construction reach the shared heap. If `setq`'s `store` needs similar threading, follow this exact pattern:
  a constructor parameter and an accessor added to `env`, plumbed through `default_env`'s overloads, not a
  redesign of how state gets shared.
- **A durable lifetime lesson, read before writing any new evaluator code that returns a `value<Core>` out of
  a helper function:** `value<Core>`'s `symbol`/`keyword` (`value.hpp`, unchanged since L7) hold a bare
  `std::string_view`, not an owned spelling — unlike `core_symbol`/`core_keyword` (elaborator layer, L10
  fixed these to own a `folded_name` after an AddressSanitizer catch). This means a `value<Core>` returned
  from `eval_direct` can view into whatever `Core` node (root or arena-resident) produced it. **The caller
  must keep the exact `node` argument passed to `eval_direct` alive for as long as the resulting
  `value<Core>` might be inspected** — L11 hit this exact bug in its own test helper (see `handoff.md`'s L11
  section, "A second stack-use-after-return bug") and fixed it by making the elaborated root a caller-owned
  out-parameter, mirroring how `core_arena` must already be caller-owned. Any `setq`/`defun` test helper
  should follow the same pattern (`elab()`'s and L11's `run()`'s shape: arenas and the elaborated root are
  reference parameters from the `TEST_CASE`, never function-locals a helper returns past).
- **Builtins are `closure::builtin_op` tags dispatched in `apply_function_value`'s `builtin` case**
  (`eval_direct.hpp`); the bridge from `builtin_op` to `pairs.hpp::list_op` is `detail::to_list_op` in that
  same file. `add`/`multiply` are **variadic** (`(+)` ⇒ `0`, `(*)` ⇒ `1`), not fixed 2-arg like the Scheme
  original — don't assume the old restriction if porting any more Scheme evaluator code forward.
- **`funcall`/`apply` have real call semantics**, not `apply_prim` cases — see `handoff.md`'s L11 section for
  the exact mechanics if `defun`-defined functions need to interact with either (they should just work,
  since a `defun`-bound function is an ordinary function-namespace value like any builtin/closure, but this
  is worth a test).

### What L12 needs from L10 (baseline elaborator, unchanged this step)

Not re-explained here — read `handoff.md`'s "Step L10" section — but headline facts: the elaborator recognizes
exactly `quote`, `if`, `progn`, `let`, `let*`, `lambda`, `function` as special operators (folded-spelling
comparison). **`setq`/`defun`/`defvar`/`defparameter` are not recognized yet** — elaborating any of them today
falls through to "ordinary application," which will either misbehave or error depending on whether `SETQ`
etc. happen to be bound in the function namespace (they are not). L12 needs new core kinds and new elaborator
special-form cases for at least `setq` (assignment: `(setq name expr)` → mutate `name`'s nearest lexical
binding, error if unbound) and `defun` (top-level function definition: `(defun name (params...) body...)` →
define `name` in the function namespace as a closure/lambda). `defvar`/`defparameter` mark a symbol special
(the mark is stored now per the plan, used in L16 for dynamic binding — no special-variable *behavior* is
needed yet, just recording that the symbol is special).

**Top-level sequencing, per the plan:** "the program is an implicit progn of top-level forms." Check whether
`elaborate()`'s current entry point (`smd::smdlisp::elaborator::elaborate`, `elaborate.hpp`) already handles a
sequence of top-level forms, or only ever elaborates one datum — L10's `elab()` test helper reads and
elaborates a single datum per call, and nothing in L4-L11 exercised multiple top-level forms sharing one
top-level environment. This is likely new plumbing L12 needs: either the reader needs to hand back multiple
top-level datums (check `read_datum`'s current contract — does it read one form and stop, or the whole input?),
or `elaborate`/`eval_direct` need a "run a sequence of top-level forms in one environment, threading definitions
forward" entry point that doesn't exist yet. The merge-criteria example makes this concrete:
`(progn (defun twice (x) (+ x x)) (twice 4))` ⇒ `8` — note this is phrased as a single `progn` wrapping both
forms, which may be the intended way to sidestep the "how do multiple top-level forms share state" question
entirely for this step (elaborate `progn` once, `defun` inside it defines into the *same* environment
`(twice 4)` then looks up) — worth checking whether that sidesteps needing new top-level-sequencing machinery
at all, or whether real multi-form top-level input (no wrapping `progn`) is also expected.

### Merge criteria (plan section 9, step L12)

```lisp
(progn (defun twice (x) (+ x x)) (twice 4))  ; => 8, at compile time
```

Plus, per the gap analysis and D4: `setq` mutates the nearest **lexical** variable binding (special-variable
interaction is explicitly deferred to L16 — don't build dynamic binding now, just get lexical `setq` and
`defun`/`defvar`/`defparameter` working), error if the target is unbound, and `setq` returns the assigned
value.

### Standing constraints for L12

- `src/smd/smdscheme/**` is frozen for semantic changes (D1). Read-only reference, never edit.
- New/changed files land in `src/smd/smdlisp/elaborator/` (new core kinds, new special-form cases) and
  `src/smd/smdlisp/closure/` (`env.hpp`'s new mutable-binding machinery, `eval_direct.hpp`'s new core-kind
  cases). Do not touch `src/smd/smdlisp/reader/` or `src/smd/smdlisp/macroexpand/` (the latter doesn't exist
  yet — that's L17's lane, see Part 2).
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- File a divergence doc under `docs/divergences/` (`TEMPLATE.md` skeleton) for anything done differently than
  the plan specifies, or any knowing ANSI CL deviation. **Next free number is DIV-0005** (neither L10 nor L11
  needed one — every deviation discussed in their handoff entries was an internal C++ architecture decision
  the plan explicitly invited, not an ANSI semantics cut or a plan-contradicting step; L12 may be the step
  that finally needs DIV-0005, e.g. if the top-level-sequencing question above forces a real scope cut).
- Before handoff: `make compile`, `make test`, `make lint`. L12 has **no** blog deliverable (phase 18 arrives
  at L13, the CPS closure backend, per the plan's phase table) — don't draft one.
- Do not continue past L12 into L13 (CPS closure backend) unless blocked; if blocked, document the blocker
  here instead.

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

## Standing constraints (apply to both L12 and L18, and everything after)

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
