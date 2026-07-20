# Next step: Common Lisp pivot, Step L18 (backquote, Track C)

> **2026-07-20 update:** L17 (this worktree, `cl-pivot/l17-macroexpand`) is done — see `handoff.md`'s
> "Step L17: macro expander with host macros landed" section. Per `docs/cl-pivot-plan.md` section 9, **Step
> L18 depends on L17**, which is now satisfied once this branch merges to `main`. L18 (backquote) is the next
> unchecked step on Track C (macros: L17 → L18 → L19).
>
> **Status of Track A (L12, `setq`/`defun`) is unknown to this worktree.** L12 was running concurrently with
> this step in a sibling worktree (`wt-l12`) and may or may not have merged to `main` by the time you read
> this — check `checklist.md` and `git log` on `main` before assuming either way. L18 does **not** depend on
> L12 (backquote only needs the reader/macroexpand/elaborator pipeline, not `setq`/`defun`), so you can start
> L18 regardless of L12's status, but branch your worktree off whatever `main` looks like at the time (which
> should include both L11 and, if it landed first, L12).

## What L17 landed, that L18 builds on

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
  cuts from this step, both potentially relevant to L18/L19:
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

## What L18 needs to build (plan section 9, step L18)

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
were installed). Check with whoever is working Track A (or `main`, if L12 has already merged) about the
current state of `closure/`/`elaborator/` before touching those files, and read L9's/L11's handoff sections
for exactly how a new builtin gets wired in (`builtin_op` enum entry, `list_op` enum entry, `detail::to_list_op`
bridge in `eval_direct.hpp`, `default_env` installing it into the FUNCTION namespace).

**Merge criteria is explicit about testing at two levels:** "expansion tests at both datum and value level" —
i.e. both a shape test (like L17's expansion-shape tests, checking the constructed datum/core structure
directly) and an end-to-end evaluated-value test (like L17's `CondEndToEnd`), for `` `(a ,x ,@ys) ``.

## Standing constraints for L18

- `src/smd/smdscheme/**` is frozen for semantic changes (D1). Read-only reference, never edit.
- This step **does** need reader-layer changes (`src/smd/smdlisp/reader/`), unlike L17, which was scoped to
  avoid touching the reader entirely. That is expected and consistent with the plan's own wording for this
  step ("reader support for `` ` ``, `,`, `,@`") — do not treat a reader change here as something requiring a
  divergence doc; it is the step's explicit job, not a deviation from it.
- New/changed files land in `src/smd/smdlisp/reader/` (new datum kinds) and `src/smd/smdlisp/macroexpand/`
  (template expansion) at minimum; `append` as a new builtin also touches `src/smd/smdlisp/closure/`
  (`value.hpp`, `pairs.hpp`, `eval_direct.hpp`, `env.hpp`'s `default_env`). Check `main`'s state before editing
  `elaborator/`/`closure/` in case L12 landed first and changed things there.
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- File a divergence doc under `docs/divergences/` (`TEMPLATE.md` skeleton) for anything done differently than
  the plan specifies, or any knowing ANSI CL deviation. **Next free number is DIV-0006** — re-check
  `docs/divergences/` immediately before filing in case a concurrent worker (L12, if still in flight, or
  anyone else) claimed a number first.
- Before handoff: `make compile`, `make test`, `make lint`. L18 has **no** blog deliverable (phase 20 arrives
  at L19, `defmacro`, per the plan's phase table) — don't draft one.
- Do not continue past L18 into L19 (`defmacro`) unless blocked; if blocked, document the blocker here
  instead.

## Standing constraints (apply to L18 and everything after)

- `src/smd/smdscheme/**` is frozen for semantic changes (D1); blog phases 5-12 transclude live code from it by
  UUID anchor. Read-only reference, never edit.
- New work goes in `src/smd/smdlisp/`, namespace `smd::smdlisp`, one sub-namespace per directory.
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- Per `docs/divergences/DIV-0004-orgit-transclusion-worktree-path.md`'s now-standing convention: if your step
  has a blog deliverable, author its `#+transclude:` orgit links against your own worktree path (e.g.
  `orgit:~/src/compile-time-scheme/wt-l18::...`), not `main` — the orchestrator repoints them to `main` after
  merge. No fresh divergence doc needed per occurrence. Watch for two org-markup footguns prior steps hit
  while drafting blog posts (see `handoff.md`'s L11 entry for the full explanation): adjacent
  `~verbatim~/~verbatim~` spans with no space between them merge into one giant span in GFM export, and a
  `~word~` immediately followed by a bare letter (e.g. `~static_assert~s`) has the same problem — always leave
  a space, or plain punctuation, immediately after a closing `~`. (L18 itself has no blog deliverable, so this
  only matters once you get to L19.)
- Before handing off any step, verify `make blog-md` (if the step has a blog deliverable) leaves every *other*
  phase's `.md` unchanged — `git status --short docs/blog/` after the render should show only your new/updated
  phase and `index.md`/`index.org`. If other `.md` files show diffs, `git diff` them first: if it's just
  `id="orgXXXXXXX"` churn, `git checkout --` them before committing; if the diff has real content changes,
  investigate before reverting.
