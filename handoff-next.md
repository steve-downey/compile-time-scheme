# Next steps: Common Lisp pivot, Step L12 (spine) and Step L17 (macros, Track C)

> **2026-07-20 update:** L11 (this worktree, `cl-pivot/l11-eval-direct`) is done — see `handoff.md`'s
> "Step L11: direct evaluator landed (+ phase 17 draft)" section. Per `docs/cl-pivot-plan.md` section 9,
> **Step L12 depends on L11**, which is now satisfied once this branch merges to `main`. L12 is the next
> unchecked step in `checklist.md`'s spine (Track A). **Step L17 (macro expander, Track C) also depends on
> L11** (specifically on the pipeline being read → elaborate → eval, which L11 completed) and, per the plan's
> parallelism summary, "may run in parallel" with L12 through L16 — it lives in `src/smd/smdlisp/macroexpand/`,
> a directory neither L12 nor anything through L16 touches, so a second worker can pick up L17 concurrently
> with whoever takes L12, in a separate worktree, once both branches are off `main` post-merge. This file
> covers both, in that order; whoever's continuing should read only their own section plus "Standing
> constraints" below.

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

## Part 2 — Step L17: macro expander with host macros (Track C, parallel-eligible)

### Why this can run in parallel with L12-L16

Per `docs/cl-pivot-plan.md`'s parallelism summary: "Track C (macros): L17 → L18 → L19 runs parallel to L14–L16
after L11." L17's own dependency line in section 9 says "Dependencies: L10 (pipeline), independent of L14–L16
— may run in parallel with them." L11 (this step) completed the read → elaborate → eval pipeline L17 needs to
insert into (read → **expand** → elaborate → eval); L17 does not depend on `setq`/`defun` (L12), `block`
(L14), or `catch`/`throw` (L15) existing first. It lives entirely under a **new** directory,
`src/smd/smdlisp/macroexpand/`, which no other in-flight step touches — a genuinely separate lane, safe to
run as a second concurrent worktree once `main` has this branch's L11 work merged in.

### What L17 needs to build

`src/smd/smdlisp/macroexpand/expander.hpp` (+ tests): a **datum-to-datum** pass that runs between the reader
and the elaborator — i.e. it operates on `smd::smdlisp::reader::datum_type<MaxNodes, MaxList>` trees (L6),
*before* anything becomes a `core_type` node (L10). This is a new architectural layer; nothing in L4-L11
built any datum-transforming code, only datum-*producing* (reader) or datum-*consuming* (elaborator, evaluator)
code. Read `src/smd/smdlisp/reader/datum_type.hpp` (L6) fully before starting — the six datum kinds
(`datum_integer`, `datum_symbol`, `datum_keyword`, `datum_list`, `datum_quote`, `datum_function`) and the
arena/`arena_box` shape are exactly what expander code will construct and consume.

Per the plan's sketch:

```cpp
// Host macro: C++ function from a call-form datum to a replacement datum,
// writing into the same arena.  Registry is a static_vector keyed by symbol.
struct host_macro {
    symbol name{};
    // (tree, call-node) -> new node id, or error
    macro_fn expand{};
};
// macroexpand-1 applies one step at the head position;
// macroexpand iterates to fixpoint under a MaxExpansions budget
// (budget exhaustion is a diagnosed error, not a hang).
```

Note: the plan's sketch uses a bare `symbol` type — `smd::smdlisp`'s reader layer doesn't have a `symbol`
type of its own (that's `closure::symbol`, a *value*-layer type from L7, used post-evaluation). For a
datum-to-datum pass, matching by spelling likely means comparing `reader::folded_name` or its `.view()`
against a `std::string_view` key, the same way the elaborator's special-form dispatch already does
(`detail::elaborate_list` in `elaborate.hpp` compares `sym.name.view() == "IF"` etc.) — reuse that pattern,
don't invent a new symbol-comparison mechanism.

**Implement `cond`, `when`, `unless`, `and`, `or`, `case` as host macros** expanding to `if`/`progn`/`let` —
matching their actual macro status in ANSI CL (these are never special operators in real Common Lisp,
always macros defined in terms of `if`/`progn`/etc.). This is a good test of whether the expander's shape is
right: if implementing these six requires ad-hoc special-casing rather than each being an ordinary
`host_macro` entry, the expander's API probably needs adjusting before more macros get added in L18/L19.

**Wire the pass into the pipeline:** read → expand → elaborate. This likely means a new top-level entry point
(analogous to `elab()`'s test helper, or a real `expand_and_elaborate()` public function) that threads a
*third* arena or reuses the datum arena for macro-expansion output — check whether expansion can write its
replacement nodes into the *same* datum arena the reader already populated (simplest, and consistent with
"the expander speaks the reader's datum language") or needs its own. `elaborate()`'s existing signature takes
`(datum, datum_arena, core_arena)` — the natural integration point is probably a new function with the same
shape that runs expansion first, `(datum, datum_arena, core_arena) -> result<core_type>`, calling `elaborate`
internally on the expanded datum tree rather than the raw one.

### Merge criteria (plan section 9, step L17)

Expansion-shape tests (each of the six macros expands to the expected `if`/`progn`/`let` shape) plus an
end-to-end test: `(cond (nil 1) (t 2))` ⇒ `2` — read, expand, elaborate, evaluate, matching L11's evaluator's
existing `nil`/`t` truthiness and `if` semantics with no evaluator changes required (the macro expander's
whole point is that `cond` never needs its own core kind or its own evaluator case).

### Standing constraints for L17

- `src/smd/smdscheme/**` is frozen for semantic changes (D1). Read-only reference (though there is likely
  nothing analogous to adapt-by-copy here — `smdscheme` has no macro layer at all, per the plan's gap
  analysis table: "Macros ... Absent (no macro layer at all)"). This is new machinery, not an adaptation.
- New files land in `src/smd/smdlisp/macroexpand/` only. Do not touch `src/smd/smdlisp/elaborator/` or
  `src/smd/smdlisp/closure/` (L12's lane, if running concurrently) or `src/smd/smdlisp/reader/` (stable since
  L6; the expander consumes `datum_type` but should not need to change its shape — if it turns out to need a
  reader-layer change, that is worth a divergence doc, since it means the plan's "datum-to-datum pass with no
  reader involvement" premise didn't survive contact with the implementation).
- `src/smd/smdlisp/CMakeLists.txt` will need a new `add_subdirectory(macroexpand)` plus a new
  `src/smd/smdlisp/macroexpand/CMakeLists.txt` (mirror `reader/`'s or `elaborator/`'s — STATIC library target
  named `smdlisp.macroexpand`, `FILE_SET` for the header(s), linking whatever of `smdlisp.reader`/
  `smdlisp.elaborator`/`smdscheme.foundation` it actually needs). If L12 is running concurrently in a sibling
  worktree and also touches the top-level `src/smd/smdlisp/CMakeLists.txt`, expect a merge conflict at
  integration time on that one file — not on anything else, since the two steps' new files don't overlap.
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions.
- File a divergence doc under `docs/divergences/` for anything done differently than the plan specifies.
  **Next free number is DIV-0005** (shared counter with L12 — whichever step's worker gets there first claims
  it; check `docs/divergences/` for the latest `DIV-NNNN` before filing, don't assume the number in this
  document is still accurate if L12 landed first).
- Before handoff: `make compile`, `make test`, `make lint`. L17 has **no** blog deliverable (phase 20 arrives
  at L19, `defmacro`, per the plan's phase table) — don't draft one.
- Do not continue past L17 into L18 (backquote) unless blocked; if blocked, document the blocker here instead.

---

## Standing constraints (apply to both L12 and L17, and everything after)

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
