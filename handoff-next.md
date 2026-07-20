# Next steps: Common Lisp pivot, Step L13 (spine, Track A) and Step L18 (backquote, Track C)

> **2026-07-20 update (orchestrator):** L12 (`cl-pivot/l12-setq-defun`) and L17 (`cl-pivot/l17-macroexpand`)
> both landed on `main` this round, run in parallel in sibling worktrees — see `handoff.md`'s
> "Step L12: `setq`, `defun`, `defvar`, `defparameter` landed" and "Step L17: macro expander with host macros
> landed" sections. That satisfies the dependencies of the next two steps:
>
> - **Step L13 (CPS closure backend, spine / Track A) depends on L12** — now satisfied.
> - **Step L18 (backquote, Track C) depends on L17** — now satisfied.
>
> These two steps are in **disjoint file lanes** (L13 in `src/smd/smdlisp/closure/`; L18 in
> `src/smd/smdlisp/reader/` + `src/smd/smdlisp/macroexpand/`, with one shared `append`-builtin touch to
> `closure/` — see the coordination note in Part 2), so they are **parallel-eligible**, exactly as L12/L17
> were: dispatch one worker per step, each in its own worktree off current `main`. This file has a section per
> step; whoever picks up a step should read only their own Part plus "Standing constraints (apply to both)" at
> the end.
>
> **Divergence numbering:** DIV-0001..DIV-0006 are now taken. DIV-0005 = L17's macroexpand hygiene/walk cuts;
> DIV-0006 = L12's `defvar`/`defparameter` value-form requirement. **Next free number is DIV-0007** — but
> re-check `docs/divergences/` immediately before filing, since L13 and L18 may again run concurrently and
> either could claim DIV-0007 first; the second to land renumbers.

---

# Part 1 — Step L13: CPS closure backend for the baseline (spine, Track A)

### What L13 needs from L12 (just landed)

L12 added four new elaborated-core kinds (`core_setq`, `core_defun`, `core_defvar`, `core_defparameter`,
all in `src/smd/smdlisp/elaborator/elaborated_core.hpp`) and their `eval_direct.hpp` evaluator cases. Read
`handoff.md`'s "Step L12" section in full before starting; the essentials L13 will lean on directly:

- **`core_type`'s variant gained four kinds this step** (`core_setq`, `core_defun`, `core_defvar`,
  `core_defparameter`) on top of L10's set. **A CPS/closure-compilation pass that pattern-matches on
  `core_type`'s variant needs a case for every kind, including these four** — the plan's own merge criterion
  ("every L11/L12 end-to-end test also passes through `compile_to_closure`") makes this explicit: L12's
  evaluator-level tests (`setq`/`defun`/`defvar`/`defparameter`) need matching CPS/closure coverage, not just
  L11's.
- **`env`'s mutable-mode / `store` machinery (`env.hpp`, from L12) is what makes `setq`/`defun`
  self-recursion work at the tree-walking-evaluator level.** Whatever runtime environment representation the
  CPS/closure backend uses at *its* layer (which may or may not be the same `closure::env` type — check
  `smd::smdscheme::closure::cps_code.hpp`/`closure_program.hpp` for how the Scheme backend's CPS layer
  represents environments/`store`s before assuming `smdlisp`'s `env`/`store` port over unchanged) needs an
  equivalent mutation story for `setq` to work through CPS, and an equivalent reserve-then-patch (or some
  other self-reference mechanism) for `defun` self-recursion to work through CPS. Do not assume this is free
  just because the direct evaluator already has it — CPS conversion of a mutable-store-backed interpreter is
  a real design question, not a mechanical port, and the Scheme original's own `begin`/`set!` CPS wiring
  (which the plan explicitly points at as "the pattern") is the concrete reference to read first.
- **`eval_direct`'s `environment` parameter is `env<...>&` (mutable), not `const&`, as of L12** — this
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

### What L13 needs from L10/L11 (unchanged)

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
narrow test. Read `src/smd/smdscheme/closure/{cps_code.hpp,closure_program.hpp}`
(the Scheme CL-pivot's own already-landed CPS backend, frozen but readable) before designing `smdlisp`'s
version — the plan explicitly says "adapted from the Scheme CPS backend per the architecture in
`docs/cps-direction.md`."

### Standing constraints for L13

- `src/smd/smdscheme/**` is frozen for semantic changes (D1). Read-only reference, never edit.
- New/changed files land in `src/smd/smdlisp/closure/` (new `cps_code.hpp`/`closure_program.hpp` + tests,
  possibly further `eval_direct.hpp`/`env.hpp` touch-ups if a genuine bug is found, matching L11's and L12's
  own precedent of touching pre-existing closure/ files when a step's own new code needs it). Do not touch
  `src/smd/smdlisp/reader/` or `src/smd/smdlisp/macroexpand/` (the latter is Track C's lane; L18 is running
  there, possibly concurrently).
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- File a divergence doc under `docs/divergences/` (`TEMPLATE.md` skeleton) for anything done differently than
  the plan specifies, or any knowing ANSI CL deviation. **Next free number is DIV-0007** — re-check
  `docs/divergences/` immediately before filing; if L18 (or anyone) landed a number first, renumber to stay
  unique.
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

# Part 2 — Step L18: backquote (Track C)

### What L17 landed, that L18 builds on

Read `handoff.md`'s "Step L17" section in full before starting. The essentials:

- **Pipeline shape:** `src/smd/smdlisp/macroexpand/expander.hpp` is a datum-to-datum pass between the reader
  and the elaborator (decision D9). `expand_and_elaborate<MaxNodes, MaxList>(pd, datum_arena, core_arena,
  table, max_expansions) -> result<core_type>` is the pipeline entry point: `expand()` (whole-tree macro walk)
  then `elaborator::elaborate()`. `expand()` writes every new node into the *same* datum arena the reader
  already populated — no second arena. This is almost certainly the integration point L18's backquote
  expansion should also go through, or at minimum should be consistent with: backquote's job (per the plan)
  is "lowering to datum template nodes, and an expansion of templates into `cons`/`list`/`append` builds
  during macro expansion" — i.e. backquote expansion is itself a form of datum-to-datum rewriting that
  presumably belongs in this same pass, or a closely related one that runs at the same pipeline stage.
- **Macro registry API, already built and exercised by six macros:** `host_macro<MaxNodes, MaxList>{name,
  expand_fn}` where `expand_fn` is a real function pointer `macro_fn<MaxNodes, MaxList>` (call-form datum in,
  replacement datum out, writing into the shared arena), collected in a `macro_table<MaxNodes,
  MaxList>` (`static_vector`, capacity `max_host_macros = 16`, currently holding 6 entries via
  `default_macro_table<MaxNodes, MaxList>()`). `macroexpand_1`/`macroexpand` operate at one expression's head
  position only (matching ANSI CL's own `macroexpand-1`/`macroexpand` exactly); `expand()` is the whole-tree
  recursive walk that actually makes macros fire everywhere they're used, not just at the top level.
- **`expand()`'s one special case is `QUOTE`:** a list headed by the symbol `QUOTE` is never walked into (its
  argument is literal data). **Backquote needs the equivalent treatment done *correctly*, which is the whole
  point of L18:** unlike `quote`, backquote's body is *mostly* literal data but selectively re-admits code at
  `,`/`,@` positions. If L18 adds new datum kinds for backquote/unquote/unquote-splicing (the plan says
  "Reader support for `` ` ``, `,`, `,@` lowering to datum template nodes" — this sounds like new
  `reader::datum_*` kinds, i.e. a **reader-layer change**, not purely a macroexpand-layer one), `expand()`'s
  walk will need to know how to handle those new kinds too: walking into `,`/`,@` positions (code) while
  leaving the surrounding backquote template alone (data), analogous to how it already handles `QUOTE`
  specially but with finer granularity.
- **DIV-0005 is filed** (`docs/divergences/DIV-0005-macroexpand-not-hygienic-or-form-aware.md`) for two scope
  cuts from L17, both potentially relevant to L18/L19:
  1. **No gensym.** `or`/`case`'s host-macro expansions use fixed, ordinary (interned) temp-variable spellings
     (`%OR-TMP`, `%CASE-KEY`), not hygienic fresh names, because there is no uninterned-symbol machinery yet.
     **The plan explicitly schedules gensym-equivalent machinery for L18/L19** (backquote/`defmacro`) — if you
     build any kind of fresh-symbol generator as part of L18, DIV-0005's revisit condition says to reconsider
     whether `or_expand`/`case_expand` in `expander.hpp` should switch to it. Not required for L18's own merge
     criteria, but worth a look since the machinery would be sitting right there.
  2. **`expand()`'s whole-tree walk is special-form-agnostic** (recognizes only `QUOTE`, nothing else) — see
     above; this is the mechanism you'll likely need to extend or parallel for backquote's data/code boundary.
- **Symbol matching pattern, reused throughout `expander.hpp`:** compare `reader::folded_name::view()` (or a
  `datum_symbol`'s `.name.view()`) against a `std::string_view` literal, e.g. `sym.name.view() == "QUOTE"`.
  Reuse this, don't invent a new comparison mechanism, matching what L17 did and what the elaborator already
  does.
- **Building new datum/list structure from scratch:** `expander.hpp`'s `detail::make_symbol`/
  `detail::make_symbol_box` helpers (build a fresh `datum_symbol` node from a string literal, folding via
  `reader::detail::fold`, and allocate it into the arena) are a reusable pattern if L18 needs to synthesize
  `CONS`/`LIST`/`APPEND` call forms the way L17's macros synthesize `IF`/`PROGN`/`LET` calls. Worth reading
  `cond_expand`/`or_expand`/`case_expand` in `expander.hpp` as worked examples of "build a new list datum by
  hand, reusing existing `arena_box` handles where possible instead of copying subexpressions."

### What L18 needs to build (plan section 9, step L18)

> Reader support for `` ` ``, `,`, `,@` lowering to datum template nodes, and an expansion of templates into
> `cons`/`list`/`append` builds during macro expansion. `append` joins the builtin set.
> Merge criteria: `` `(a ,x ,@ys) `` expansion tests at both datum and value level.
> Dependencies: L17.

Read this literally: it names **two** pipeline stages, not one — "reader support ... lowering to datum
template nodes" (new reader-level datum kinds recognizing `` ` ``/`,`/`,@` syntax) and "an expansion of
templates into `cons`/`list`/`append` builds during macro expansion" (the actual backquote-to-code lowering,
which sounds like it belongs in or alongside `src/smd/smdlisp/macroexpand/`, per this step's constraint list
below). This is very likely a two-file-or-more step touching **both** `src/smd/smdlisp/reader/` (new datum
kind(s) for backquote/unquote/unquote-splicing, mirroring how L6 added `datum_quote`/`datum_function` for
`'`/`#'`) and `src/smd/smdlisp/macroexpand/` (the template-to-`cons`/`list`/`append` expansion, alongside or
integrated with L17's `expand()`). Check whether this is better modeled as new `datum_*` node kinds (like
`datum_quote`) or whether backquote can be read as ordinary lists headed by synthetic markers — the plan's own
phrasing ("lowering to datum template nodes") suggests genuine new node kinds, analogous to L6's
`datum_function` for `#'`.

**`append` joins the builtin set** — this is a `closure`/`elaborator`-layer change (a new `builtin_op` in
`value.hpp`, a new `list_op` case in `pairs.hpp`, matching L9's pattern for how `cons`/`car`/`cdr`/`list` etc.
were installed). **Coordination note:** `src/smd/smdlisp/closure/` is also L13's lane (spine), which may be
running concurrently. `append` only *adds* a new builtin — it does not change any existing `closure/`
declaration — so a conflict with L13 is unlikely, but branch off current `main` (which includes L12's
`closure/` state) and expect a possible small merge at integration if both steps touch `value.hpp`/`env.hpp`'s
`default_env`. Read L9's/L11's handoff sections for exactly how a new builtin gets wired in (`builtin_op` enum
entry, `list_op` enum entry, `detail::to_list_op` bridge in `eval_direct.hpp`, `default_env` installing it into
the FUNCTION namespace).

**Merge criteria is explicit about testing at two levels:** "expansion tests at both datum and value level" —
i.e. both a shape test (like L17's expansion-shape tests, checking the constructed datum/core structure
directly) and an end-to-end evaluated-value test (like L17's `CondEndToEnd`), for `` `(a ,x ,@ys) ``.

### Standing constraints for L18

- `src/smd/smdscheme/**` is frozen for semantic changes (D1). Read-only reference, never edit.
- This step **does** need reader-layer changes (`src/smd/smdlisp/reader/`), unlike L17, which was scoped to
  avoid touching the reader entirely. That is expected and consistent with the plan's own wording for this
  step ("reader support for `` ` ``, `,`, `,@`") — do not treat a reader change here as something requiring a
  divergence doc; it is the step's explicit job, not a deviation from it.
- New/changed files land in `src/smd/smdlisp/reader/` (new datum kinds) and `src/smd/smdlisp/macroexpand/`
  (template expansion) at minimum; `append` as a new builtin also touches `src/smd/smdlisp/closure/`
  (`value.hpp`, `pairs.hpp`, `eval_direct.hpp`, `env.hpp`'s `default_env`). See the L13 coordination note above
  before editing `closure/`.
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- File a divergence doc under `docs/divergences/` (`TEMPLATE.md` skeleton) for anything done differently than
  the plan specifies, or any knowing ANSI CL deviation. **Next free number is DIV-0007** — re-check
  `docs/divergences/` immediately before filing in case L13 (if running concurrently) claimed a number first.
- Before handoff: `make compile`, `make test`, `make lint`. L18 has **no** blog deliverable (phase 20 arrives
  at L19, `defmacro`, per the plan's phase table) — don't draft one.
- Do not continue past L18 into L19 (`defmacro`) unless blocked; if blocked, document the blocker here
  instead.

---

## Standing constraints (apply to both L13 and L18, and everything after)

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
  `~word~` immediately followed by a bare letter (e.g. `~static_assert~s`) has the same problem — always leave
  a space, or plain punctuation, immediately after a closing `~`. (Of the two next steps, only L13 has a blog
  deliverable, phase 18; L18 does not.)
- Before handing off any step, verify `make blog-md` (if the step has a blog deliverable) leaves every *other*
  phase's `.md` unchanged — `git status --short docs/blog/` after the render should show only your new/updated
  phase and `index.md`/`index.org`. If other `.md` files show diffs, `git diff` them first: if it's just
  `id="orgXXXXXXX"` churn, `git checkout --` them before committing; if the diff has real content changes,
  investigate before reverting.
