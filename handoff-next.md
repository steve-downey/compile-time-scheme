# Next step: Common Lisp pivot, Step L11 (direct evaluator, + phase 17 draft)

> **2026-07-19 update:** L10 (this worktree, `cl-pivot/l10-elaborator`) is done — see `handoff.md`'s
> "Step L10: CL core model and baseline elaborator landed" section. Per `docs/cl-pivot-plan.md` section 9,
> **Step L11 depends on L10**, which is now satisfied once this branch merges to `main`. L11 is the next
> unchecked step in `checklist.md`. The rest of this file is written for whoever picks up L11.

## What L11 needs from L10 (this step)

L11 builds `src/smd/smdlisp/closure/eval_direct.hpp` (+ tests): a structural-recursive evaluator over the
core tree L10 produced. Read `handoff.md`'s "Step L10" section in full before starting; the essentials:

- The core tree is `smd::smdlisp::elaborator::core_type<MaxNodes, MaxList>`
  (`src/smd/smdlisp/elaborator/elaborated_core.hpp`), a `smdscheme::foundation::fix`-based arena tree over
  **twelve** alternatives: `core_integer`, `core_symbol` (VARIABLE-namespace ref), `core_keyword`
  (self-evaluating), `core_nil`, `core_true`, `core_quote` (atom-only: `variant<int, core_symbol,
  core_keyword>`), `core_cons<R,MaxNodes>` (hermetic pair constructor, quote-only — see below),
  `core_if<R,MaxNodes>` (always 3 branches; a 2-arg source `if` already got an implicit `core_nil`
  alternative at elaboration time), `core_progn<R,MaxNodes,MaxList>`, `core_lambda<R,MaxNodes,MaxList>`
  (params + **body as a sequence**, `static_vector<arena_box,MaxList>`, implicit progn — not a single
  expression), `core_function<R,MaxNodes>` (FUNCTION-namespace name-ref *or* an embedded lambda, see next
  bullet), `core_application<R,MaxNodes,MaxList>`.
- **`core_application.func` is always an `arena_box` to a `core_function` node — never a bare `core_symbol`.**
  This is the load-bearing D4 invariant: evaluating an application means evaluating `func` as a
  `core_function` (which is either "look up this name in the FUNCTION namespace" — `env::lookup_function` —
  or "this *is* the lambda, materialize a closure directly, no lookup"), never falling back to VARIABLE
  -namespace lookup. `std::get<core_function<Core,MaxNodes>>(...)` on `func`'s target node should always
  succeed; if it doesn't, that is an elaborator bug, not a runtime condition to handle gracefully.
- `NIL`/`T` are `core_nil{}`/`core_true{}`, **distinct from `core_symbol`**, both for bare unquoted source
  and for quoted `'nil`/`'t` (elaboration intercepts both spellings before ever producing a `core_symbol` or
  `core_quote`). The evaluator needs one case each: `core_nil{}` → `closure::value<Core>{closure::nil_t{}}`,
  `core_true{}` → whatever the value model treats as canonical true. **Recommendation, not yet decided:**
  `pairs.hpp`'s `apply_prim` (L8) already treats `symbol{"T"}` as the canonical true return value for
  predicates (`null`/`eq`/`eql`/`atom`) — for consistency, `core_true` should probably evaluate to that same
  `closure::value<Core>{closure::symbol{"T"}}` rather than inventing a second "true" representation. This is
  explicitly the decision the L10 plan text deferred to L11 ("follow what the value model supports"); make
  the call, then update `handoff.md`'s L10 entry's parenthetical or add a note here — either way, write it
  down once decided, since L12+ (`eq`/`eql` on `T`, `defun`-returned booleans, etc.) needs to agree with it.
- **Quoted list data is `core_cons`, not a value-level `pairs.hpp` builtin call.** `detail::elaborate_quoted_datum`
  (L10) lowers `'(1 2 3)` to nested `core_cons<Core,MaxNodes>{ arena_box car; arena_box cdr; }` cells
  bottoming in `core_nil{}` — a *hermetic* construction node, deliberately bypassing the FUNCTION-namespace
  `CONS` builtin so a later `(defun cons ...)` (L12) can never break the meaning of already-elaborated quote
  forms. The evaluator needs a `core_cons` case: evaluate `car`/`cdr` (they're already fully elaborated
  sub-expressions — for quote-constructed data these are always literals/`core_cons`/`core_nil`, never
  something needing further lookup) and allocate a `closure::pair_ref` into the shared `pair_heap`, exactly
  the same runtime action `apply_prim(list_op::cons, ...)` (`pairs.hpp`) already performs — L11 will likely
  want to route both through the same helper rather than duplicating cons-cell construction logic.
- `core_symbol`/`core_keyword`'s `name` field is a `reader::folded_name` (owned storage: `array<char,64>` +
  `int`, `.view()` for a `string_view`), **not** a bare `string_view` — this was a deliberate fix mid-step
  for a real AddressSanitizer-caught stack-use-after-return (full story in `handoff.md`'s L10 entry). If L11
  adds any new core-node field that stores a name/spelling, check whether it can trace back to a `read_datum`
  *root* return value (not arena-backed) before deciding whether a view is safe or an owned copy is required.
  `core_function.target`'s `string_view` and `core_lambda.params`' `string_view`s remain views (audited safe
  in L10 — always sourced through `datum_arena.get(...)`, never the un-arena-backed root); don't naively
  "fix" those too, they're fine as-is and changing them would be pure churn.
- Elaborator-level errors carry a default-valued `source_pos` (`foundation::parse_error{{}, "msg"}`) —
  matches the Scheme elaborator exactly, not a shortcut specific to `smdlisp`. Don't expect real positions
  out of `elaborate()`'s errors; only `read_datum`'s errors carry real ones.
- Calling pattern for tests: mirror `src/smd/smdlisp/elaborator/elaborate.test.cpp`'s `elab()` helper (read
  then elaborate, two arenas passed by reference from the caller/`TEST_CASE`, **not** function-local — the
  AddressSanitizer bug above is exactly what goes wrong if a helper function owns the arenas/datum result and
  returns something that outlives it incorrectly; `elab()`'s current form is already safe, since `core_arena`
  is caller-owned and `core_symbol`/`core_keyword` no longer view into `datum_arena` — copy that pattern, don't
  reinvent it).

## What L11 needs from L9 (Lisp-2 environment, already merged before L10 started)

Not re-explained in full here — read `handoff.md`'s "Step L9" section — but the headline facts an evaluator
needs: `env<Core, MaxBindings>` has **two independent namespaces** (`define_value`/`lookup_value` and
`define_function`/`lookup_function`), no parent-link (a nested scope is a *copy* with more bindings
appended), and `default_env<Core, MaxBindings>()` installs `+`, `*`, `CONS`, `CAR`, `CDR`, `LIST`, `NULL`,
`EQ`, `EQL`, `ATOM`, `FUNCALL`, `APPLY` into the **function** namespace only, as `closure::builtin_op`
tagged values. Dispatching a looked-up `builtin` value (bridging `closure::builtin_op` to `pairs.hpp`'s
`list_op` for the 8 shared names, plus giving `funcall`/`apply` real semantics) is explicitly an L10/L11 job
per L9's handoff — L10 didn't touch it (out of scope, no evaluator work), so **L11 owns this bridge**.

## The closure-capture ownership question L11 must finally solve

L9's handoff flagged this as open and explicitly deferred it to "whichever step first constructs real
closures (L10/L11's evaluator)" — L10 built no evaluator, so it is unresolved and now squarely L11's problem:

`closure<Core, MaxBindings>::captured` is currently a **non-owning raw pointer** (`env<Core,MaxBindings>
const *`), because `env.hpp` must include `value.hpp` (for `value<Core>`), so `value.hpp` cannot include
`env.hpp` back to embed a complete `env` by value — a real header-cycle constraint, not a style choice (see
L9's handoff entry for the full reasoning). This means: whatever L11's evaluator does when a `lambda`
elaborates to a runtime closure, it must decide **where the captured `env` instance physically lives** such
that the closure's raw pointer stays valid for as long as the closure might be called. A `constexpr`
call-stack-local `env` (e.g. a temporary built inside `eval_direct`'s recursive call for a `lambda` node)
will **not** survive being returned/stored anywhere — this is the exact same class of bug L10 hit and fixed
for `core_symbol`/`core_keyword` (stack-use-after-return), just one layer up (runtime values instead of core
nodes). L9's handoff sketches the likely fix: an "env arena" in the same stable-index style already used by
`pair_heap` (and by the Scheme `store`) — a closure would capture a stable, arena-owned pointer, never a
pointer onto a C++ call-stack local that could be outlived by the evaluator frame that created it. L11 needs
to actually build that (or an equivalent), not just note it again.

## Standing constraints (apply to L11 and everything after)

- `src/smd/smdscheme/**` is frozen for semantic changes (D1); blog phases 5-12 transclude live code from it by
  UUID anchor. Read-only reference, never edit.
- New work goes in `src/smd/smdlisp/`, namespace `smd::smdlisp`, one sub-namespace per directory. L11 adds to
  `src/smd/smdlisp/closure/` (new file `eval_direct.hpp` + test).
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- File a divergence doc under `docs/divergences/` (`TEMPLATE.md` skeleton) for anything done differently than
  the plan specifies, or any knowing ANSI CL deviation. Next free number is **DIV-0005** (L10 didn't need one;
  every L10 deviation from the Scheme original was a structural C++ adaptation already implied by L9's
  landed choices, not an ANSI CL semantics cut or a plan-contradicting step).
- Before handoff: `make compile`, `make test`, `make lint`. L11 **does** have a blog deliverable (phase 17,
  "`nil`, `t`, and living in a Lisp-2" per the plan's phase table) — draft
  `docs/blog/phase-17-nil-t-lisp2.org` (or similar slug) and run `make blog-md` to confirm it renders; also
  check `docs/divergences/DIV-0004-orgit-transclusion-worktree-path.md` — it documents that a fresh
  worktree's `orgit:` transclusion links must point at the worktree's own path during development (not
  `main`, which won't yet contain code the current branch hasn't merged) and get mechanically repointed to
  `main` after merge. This recurs for L11 exactly as DIV-0004 predicted.
- Merge criteria per the plan (section 9, Step L11), as compile-time static_asserts:
  ```lisp
  (if nil 1 2)                           ; => 2
  ((lambda (x) (car (cdr x))) '(1 2 3))  ; => 2
  (funcall #'cons 1 nil)                 ; => (1)
  ```
- Do not continue past L11 into L12 (`setq`/`defun`/`defvar`/`defparameter`) unless blocked; if blocked,
  document the blocker here instead.
