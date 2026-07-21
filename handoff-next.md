# Next step: Common Lisp pivot, Step L19 (`defmacro`, Track C)

> **2026-07-21 update:** Step L18 (backquote, `cl-pivot/l18-backquote`) landed — see `handoff.md`'s
> "Step L18: backquote landed" section. That satisfies L19's stated dependency ("Dependencies: L18 (and
> transitively L11)").
>
> **Status of L13 (CPS closure backend, Track A/spine) is unknown to this handoff.** L13 was dispatched to run
> concurrently with L18 in a sibling worktree, touching `src/smd/smdlisp/closure/` (new `cps_code.hpp`/
> `closure_program.hpp` + tests). L18 also touched `src/smd/smdlisp/closure/` (additive only: a new
> `builtin_op::append`/`list_op::append`, a new `to_list_op` bridge case, a new `default_env` builtin install —
> see `handoff.md`'s L18 section). Whoever picks up L19 should check whether L13 has landed on `main` yet
> before starting (`git log --oneline main` or ask the orchestrator) and branch off current `main` either way;
> if L13 hasn't landed, L19 depends on L11/L18 only and does not need to wait, per the plan's parallelism
> summary (Track C runs parallel to L14-L16, and by extension is independent of L13's spine work — only the
> `closure/` file lane might need a small merge at integration, same story L18 already went through cleanly).
>
> **Divergence numbering:** DIV-0001..DIV-0007 are now taken (DIV-0007 = L18's nested-backquote/`append`-arity
> scope cuts). **Next free number is DIV-0008** — re-check `docs/divergences/` immediately before filing, since
> L13 (if still in flight) could file its own divergence doc concurrently and claim a number first.

---

## What L18 landed, that L19 builds on

Read `handoff.md`'s "Step L18: backquote landed" section in full before starting. The essentials:

- **Reader: three new datum kinds** (`reader::datum_backquote<R,MaxNodes>{templ}`,
  `reader::datum_unquote<R,MaxNodes>{target}`, `reader::datum_unquote_splice<R,MaxNodes>{target}`), lowering
  `` ` ``/`,`/`,@` syntax — the datum tree's variant is now 9 alternatives, not 6. `` ` ``, `,`, and `,@` are all
  terminating macro characters now (`cl_chars.hpp`).
- **Macroexpand: `expand_backquote_template` (`macroexpand/expander.hpp`) lowers a backquote template into an
  ordinary `CONS`/`APPEND`/`QUOTE` call-form datum tree**, e.g. `` `(a ,x ,@ys) `` → `(CONS (QUOTE A) (CONS X
  (APPEND YS NIL)))`. `expand()`'s whole-tree walk was extended with a new case: a `datum_backquote` node is
  lowered, then the lowered result is walked by `expand()` itself (recursively) so any macro call sitting in an
  unquoted position still gets expanded, and the lowered form's synthesized `(QUOTE ...)` sub-forms
  automatically hit the pre-existing "never walk into QUOTE's argument" rule with no new logic needed for that
  half. A bare `,`/`,@` with no enclosing backquote is a diagnosed error, not a silent pass-through.
- **`append` joined the builtin set** (`closure::builtin_op::append`, `closure::list_op::append`,
  `detail::append_impl` in `pairs.hpp`, a `to_list_op` bridge case and dispatch-switch arm in `eval_direct.hpp`,
  an `"APPEND"` install in `env.hpp`'s `default_env`). **Per DIV-0007, `append` is two-argument only** — not
  ANSI CL's variadic form. If `defmacro`-defined macros end up wanting to call `append` with more than two
  arguments (unlikely, since backquote's own lowering never needs more than two), widen `apply_prim`'s `append`
  case to a `std::span` fold rather than assuming the two-argument signature is permanent.
- **Nested backquote is NOT supported** (DIV-0007): a `` ` `` template appearing inside another backquote
  template is treated as opaque literal data (wrapped in `QUOTE`), not re-expanded at its own comma-nesting
  level. If any `defmacro`-defined macro body needs a backquote template that itself contains another
  backquote, this will either silently do nothing useful (if the inner template has no `,`/`,@`) or produce a
  diagnosed `"quote: unsupported datum"` elaboration error (if it does) — not silently wrong evaluation, but
  also not supported. Revisit DIV-0007 if this bites.
- **No gensym / hygiene machinery was built this step**, despite L17's DIV-0005 explicitly flagging L18/L19 as
  the natural place for it (`or`/`case`'s `%OR-TMP`/`%CASE-KEY` fixed temp names are still not hygienic). L18's
  backquote lowering does not itself need fresh symbols (every synthesized name — `CONS`, `QUOTE`, `APPEND`,
  `NIL` — is a fixed, meaningful spelling, not a gensym'd temp), so building gensym infrastructure was not
  forced by this step and was left for whoever needs it first. **`defmacro`-defined macros are exactly where
  user code is most likely to need `gensym`** (a macro-writer avoiding variable capture in its own expansion,
  the same problem `or`/`case`'s host macros have but user-authored instead of host-authored) — if L19's own
  merge criteria don't strictly require it, consider whether to build it now (an uninterned-symbol kind, or
  just a counter-suffixed interned-symbol scheme as a documented scope cut) or file a divergence doc explaining
  why it's deferred again.
- **Symbol-matching pattern, still the same one to reuse:** compare `reader::folded_name::view()` against a
  `std::string_view` literal (`sym.name.view() == "DEFMACRO"`), exactly as `elaborate_list`/`expand()`/every
  host macro in `expander.hpp` already do.

## What L19 needs to build (plan section 9, step L19)

> Object-language macros: `defmacro` registers a macro whose expander is a compiled closure run by the
> **compile-time evaluator** during the expansion pass, receiving the macro call as list values (L8) and
> returning a value that is reified back into the datum arena. This is the "compiler running the language it
> compiles, at compile time" milestone; the datum⇄value reification pair is the new machinery.
> Merge criteria: a `defmacro`-defined `my-when` behaves identically to the host `when`; expansion-budget error
> test.
> Deliverable: blog phase 20 draft.
> Dependencies: L18 (and transitively L11).

Read this literally: it names a **new kind of bridge** that nothing before this step has needed —
**datum-to-value** (turning a macro call's argument list, currently a `reader::datum_list` of `arena_box`
handles, into a `closure::value<Core>` list built of `pair_ref`/`cons` cells, so `eval_direct` can pass it as an
ordinary argument to the macro's compiled closure) and **value-to-datum** (the reverse: taking whatever
`value<Core>` the macro's closure body returns — presumably itself a cons-cell list structure, since a macro
expander returns code-as-data — and "reifying" it back into a fresh `reader::datum_type` node in the datum
arena, so `expand()`'s ordinary walk can keep processing the result exactly like a host macro's return value).
Neither direction exists anywhere in the codebase yet:

- `elaborate_quoted_datum` (elaborator) goes datum → **core** (compile-time AST), not datum → **value**
  (runtime). A `defmacro` expansion needs to happen *before* elaboration (D9: macro expansion is a datum-to-
  datum pass), so going through the elaborator + `eval_direct` for every macro call would work but produces a
  `value<Core>`, which is the right target — this is likely the correct reuse (elaborate the call's argument
  forms as if they were quoted data, i.e. run them through something `elaborate_quoted_datum`-shaped, then
  `eval_direct` the result) rather than inventing a third lowering path.
- The reverse (value → datum) is genuinely new: nothing today ever turns a runtime `value<Core>` (an `int`,
  `symbol`, `keyword`, `pair_ref` chain, etc.) back into a `reader::datum_type` node. Sketch: `int` →
  `datum_integer`; `symbol`/`keyword` → `datum_symbol`/`datum_keyword` (careful with `folded_name` ownership —
  see the recurring lifetime lesson below); `nil_t` → the bare `NIL` symbol datum (matching how `NIL`
  round-trips as data elsewhere in this codebase, not a special "empty datum list" kind); `pair_ref` → walk the
  cons chain via the environment's `pair_heap` and build a `datum_list`, erroring on an improper (non-`nil`-
  terminated) list since the datum tree has no dotted-pair form (D6); `closure`/`builtin`/`foreign_function` →
  there is no sensible datum representation for these (a macro should never *return* a function value as
  literal code) — a diagnosed error is the right behavior, not a crash or silent nonsense.

**A durable lifetime lesson repeated at every layer so far (L10 for `core_symbol`/`core_keyword`, L11 for
`closure::symbol`/`closure::keyword`): watch where a reified `datum_symbol`/`datum_keyword`'s `folded_name`
comes from.** `closure::symbol`/`closure::keyword` hold a bare `std::string_view name`, not an owned spelling
(L11's handoff section explains why this was deliberately deferred). Building a `reader::folded_name` for a
reified datum node from that view is safe (folded_name owns its own storage — copying a view into it is fine),
but only if the `string_view` itself is still valid at the moment of the copy; if a macro's compiled closure
returns a symbol/keyword value whose `string_view` traces back to something that could have gone out of scope
(the same class of bug L10/L11 hit and fixed), the reification step needs to be the one that's careful, since
nothing upstream of it enforces this anymore.

## Standing constraints for L19

- `src/smd/smdscheme/**` is frozen for semantic changes (D1). Read-only reference, never edit.
- New/changed files land primarily in `src/smd/smdlisp/macroexpand/` (the `defmacro` registration mechanism,
  the datum⇄value reification pair, wiring into `expand()`/`expand_and_elaborate`) and likely
  `src/smd/smdlisp/closure/` (if the datum→value half needs a helper alongside `eval_direct.hpp`/`pairs.hpp`
  rather than being purely local to `macroexpand/`). Check whether L13 has landed before editing `closure/`;
  if it has, read its handoff section first so a new small helper doesn't collide with L13's own additions.
  Avoid touching `src/smd/smdlisp/elaborator/` unless something in `defmacro`'s design genuinely forces it —
  L17 and L18 both needed zero elaborator changes, and the plan's phrasing ("compile-time evaluator," "reified
  back into the datum arena") suggests L19 shouldn't either, but confirm rather than assume.
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- File a divergence doc under `docs/divergences/` (`TEMPLATE.md` skeleton) for anything done differently than
  the plan specifies, or any knowing ANSI CL deviation (e.g. if gensym/hygiene is deferred again, or if
  `defmacro`'s argument-destructuring lambda-list is narrower than ANSI's full macro lambda-list syntax — very
  likely, given `elaborate_lambda`'s own existing "flat list of symbols" restriction). **Next free number is
  DIV-0008** — re-check `docs/divergences/` immediately before filing.
- Before handoff: `make compile`, `make test`, `make lint`. **This step has a blog deliverable: phase 20**, per
  the plan's phase table — follow L6/L10/L11/L13's precedent for authoring `#+transclude:` orgit links against
  this step's own worktree path (`docs/divergences/DIV-0004-orgit-transclusion-worktree-path.md`'s now-standing
  convention); the orchestrator repoints them to `main` after merge. Also run `make blog-md` and confirm only
  phase 20's `.md` and `index.md`/`index.org` show real content diffs (revert any incidental org-export
  id-churn in unrelated phases, same check every prior blog-deliverable step has done).
- Do not continue past L19 into L20 (multiple values) unless blocked; if blocked, document the blocker here
  instead.

---

## Standing constraints (apply to L19 and everything after)

- `src/smd/smdscheme/**` is frozen for semantic changes (D1); blog phases 5-12 transclude live code from it by
  UUID anchor. Read-only reference, never edit.
- New work goes in `src/smd/smdlisp/`, namespace `smd::smdlisp`, one sub-namespace per directory.
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- Per `docs/divergences/DIV-0004-orgit-transclusion-worktree-path.md`'s now-standing convention (resolved
  2026-07-19): if your step has a blog deliverable, author its `#+transclude:` orgit links against your own
  worktree path (e.g. `orgit:~/src/compile-time-scheme/wt-l19::...`), not `main` — the orchestrator repoints
  them to `main` after merge. No fresh divergence doc needed per occurrence. Watch for two org-markup footguns
  L11 hit while drafting phase 17 (see `handoff.md`'s L11 entry for the full explanation): adjacent
  `~verbatim~/~verbatim~` spans with no space between them merge into one giant span in GFM export, and a
  `~word~` immediately followed by a bare letter (e.g. `~static_assert~s`) has the same problem — always leave
  a space, or plain punctuation, immediately after a closing `~`.
- Before handing off any step, verify `make blog-md` (if the step has a blog deliverable) leaves every *other*
  phase's `.md` unchanged — `git status --short docs/blog/` after the render should show only your new/updated
  phase and `index.md`/`index.org`. If other `.md` files show diffs, `git diff` them first: if it's just
  `id="orgXXXXXXX"` churn, `git checkout --` them before committing; if the diff has real content changes,
  investigate before reverting.
