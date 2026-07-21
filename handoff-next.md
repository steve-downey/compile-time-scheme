# Next steps: Common Lisp pivot, Step L14 (block/return-from, spine) and Step L19 (`defmacro`, Track C)

> **2026-07-21 update (orchestrator):** L13 (`cl-pivot/l13-cps-closure`) and L18 (`cl-pivot/l18-backquote`)
> both landed on `main` this round, run in parallel in sibling worktrees — see `handoff.md`'s
> "Step L13: CPS closure backend landed" and "Step L18: backquote landed" sections. That satisfies the
> dependencies of the next two steps:
>
> - **Step L14 (`block`/`return-from`, spine / Track A) depends on L13** — now satisfied.
> - **Step L19 (`defmacro`, Track C) depends on L18 (and transitively L11)** — now satisfied.
>
> Per the plan's parallelism summary, **Track C (L19) is meant to run parallel to the spine (L14–L16)**, so
> these two are parallel-eligible, and can be dispatched one worker per step in separate worktrees off current
> `main`. **Unlike the last two rounds, the file lanes are NOT cleanly disjoint this time:** L14 works in
> `src/smd/smdlisp/elaborator/` + `src/smd/smdlisp/closure/` (new `eval_direct.hpp`/`cps_code.hpp` cases and an
> exit-table mechanism), and L19 works in `src/smd/smdlisp/macroexpand/` **and possibly**
> `src/smd/smdlisp/closure/` (if its datum→value reification needs a helper alongside `eval_direct.hpp`/
> `pairs.hpp`). If both run concurrently, the `closure/` lane is the collision risk — L19's worker is asked to
> keep any `closure/` reification helper in `macroexpand/` where possible, and additive if not, so the
> orchestrator's merge stays small (same story L18's `append` already went through cleanly against L13).
> This file has a section per step; read only your own Part plus "Standing constraints (apply to both)".
>
> **Divergence numbering:** DIV-0001..DIV-0008 are now taken (DIV-0007 = L18's nested-backquote/`append`-arity
> cuts; DIV-0008 = L13's `compile_to_closure` caller-owned datum arena). **Next free number is DIV-0009** —
> re-check `docs/divergences/` immediately before filing, since L14 and L19 may again run concurrently and
> either could claim DIV-0009 first; the second to land renumbers.

---

# Part 1 — Step L14: `block` / `return-from` (spine, Track A)

## What L13 landed, that L14 builds on

Read `handoff.md`'s "Step L13" section in full before starting. The essentials:

- **`src/smd/smdlisp/closure/{cps_code.hpp,closure_program.hpp}` exist now**, adapted from
  `smd::smdscheme::closure::{cps_code.hpp,closure_program.hpp}` (frozen, read-only reference) per
  `docs/cps-direction.md`'s structural-recursion-over-flat-arena architecture.
  `detail::cps_dispatch<MaxNodes,MaxList,MaxBindings,MaxEnvs,Cont,K>` pattern-matches all
  `elaborated_core.hpp` node kinds and threads a two-continuation `(cont, k)` pair: subexpressions needing
  an immediate value use `identity_k`; a node's own tail position (an `if` branch, a `progn`'s
  last expression, a called closure's last body expression) tail-chains the CALLER's own `(cont, k)`
  straight through. `detail::cps_apply` is the CPS counterpart of `eval_direct.hpp`'s
  `apply_function_value` — the single call-dispatch point for ordinary application, `funcall`, and `apply`.
- **This is exactly the hook L14 needs, and it is genuinely new plumbing, not a rename of something
  `eval_direct` already had.** `eval_direct.hpp` is a plain recursive-descent tree-walker: C++'s own call
  stack IS its continuation, and there is no way to reify "the rest of the computation" as a value you can
  stash somewhere and invoke later, skipping intervening frames. `cps_code.hpp`'s `k` (and `cont`) are real
  C++ callables threaded explicitly through every dispatch call — that is what makes it possible, for the
  first time in this project, to capture "what happens after this `block` finishes" as a value a
  `return-from` deep inside the block's body can invoke directly. **However:** as landed, L13 never actually
  builds a NEW `k`/`cont` value anywhere — every call site either forwards `cont`/`k` verbatim (tail
  position) or passes `identity_k` (an immediate, non-escaping evaluation). Read `cps_code.hpp`'s
  `cps_dispatch`/`cps_apply` before assuming continuation *construction* is a solved problem: L14 is where a
  genuinely new kind of continuation (one that skips frames, tied to a live/dead exit record) first has to
  be built.
- **`environment` is threaded as a mutable reference (`env<Core,MaxBindings>&`) throughout CPS**, a
  deliberate departure from the frozen Scheme original's `Env const&` — see `handoff.md`'s L13 section for
  why (`defun`'s `env::define_function` mutates the environment object itself, not just a store cell).
  Whatever exit-table mechanism L14 builds should decide up front whether it threads the SAME way (a mutable
  reference alongside `environment`/`envs`) or as part of the continuation values themselves; either is
  plausible, but check what's simplest against the actual `cps_dispatch` signature before choosing.
- **`setq`/`defun`/`defvar`/`defparameter` all work through CPS now**, including `defun` self-recursion
  (reserve-then-patch, identical mechanism to `eval_direct`'s). The frozen Scheme CPS backend's own
  `core_define` case is an unconditional error ("define not supported in expression context") — it was
  never a working pattern to adapt from for these four forms, and L13 had to work out the CPS story
  directly against `eval_direct.hpp`'s already-landed L12 semantics. **The same caution applies to L14**:
  do not assume the frozen Scheme backend has ANY escape-machinery pattern to adapt from — `smd::smdscheme`
  never had `block`/`return-from`/`catch`/`throw`/`unwind-protect` at all (D1's freeze predates the whole
  CL-specific control-flow story). This is new machinery from scratch, exactly as the plan's own section 9
  sketch for L14 says.
- **`compile_to_closure<MaxNodes,MaxList>(src, datum_arena)` takes the datum arena as a caller-owned,
  mutable out-parameter** — NOT a single `std::string_view` argument like the Scheme original. This is a
  forced divergence (DIV-0008), not a style choice: `smdlisp`'s case-folded symbol/keyword storage (D2)
  means `core_lambda::params`/`core_function::target`'s bare-symbol `std::string_view`s view into the datum
  arena's own memory, and that memory has to survive for as long as the compiled program is called, not just
  while it's being elaborated. If L14 needs to compile a whole program (e.g. for an end-to-end
  `block`/`return-from` test through `compile_to_closure`), read `closure_program.hpp`'s doc comment on
  `compile_to_closure` and `docs/divergences/DIV-0008-...md` before writing test helpers — the datum arena
  must be declared in the SAME scope that later inspects a `symbol`/`keyword` result, exactly like the core
  root already had to be in `eval_direct.test.cpp`.
- **A durable, now-three-times-repeated lifetime lesson (L10, L11, L13): never return a `value<Core>` out of
  a function whose only reference to whatever produced it (an elaborated root, a compiled `closure_program`,
  a compiled `cps_code`) was local to that function.** `value<Core>`'s `symbol`/`keyword` are bare
  `std::string_view`s, not owned spellings; the thing they view into (a core node, a datum arena, a compiled
  program's captured copy of either) has to outlive every inspection of the value. `closure_program.test.cpp`
  and `cps_code.test.cpp`'s test helpers were rewritten mid-step specifically because of this — read both
  files' header comments for the exact failure mode. Any L14 test helper that builds a `block`/`return-from`
  program and inspects its result needs to follow the same "compile inline, only factor out the
  'evaluate an already-compiled thing' step" pattern.

## What L14 needs to build (plan section 9, step L14)

> ```cpp
> // One-shot lexical exit, per D5.
> // block establishes an exit record; return-from completes to the
> // block's continuation and kills the record; a dead exit is a
> // diagnosed error, not UB.
> using exit_id = int;
>
> struct exit_record {
>     symbol   name{};          // block name, function namespace not involved
>     exit_id  id{invalid_exit};
>     bool     live{false};
> };
> ```
>
> Elaborator: `(block name body…)` and `(return-from name expr)` core kinds; `return-from` resolves its
> block lexically at elaboration time (unknown block name is an elaboration error).
> CPS: dispatch threads an exit table; `block` registers a live record whose continuation is the block's
> `k`; `return-from` evaluates its value, checks liveness, marks the record dead, and invokes the block
> continuation, skipping intervening frames; falling off the block end also kills the record.
> `defun` bodies get an implicit `(block function-name …)` per CL.
>
> Merge criteria:
>
> ```lisp
> (block b 1 (return-from b 42) 3)   ; => 42
> (defun f (x) (if x (return-from f 0) 1))  ; (f t) => 0
> ```
>
> plus a diagnosed-error test for a dead exit (closure smuggling a `return-from` out of its extent).
>
> Dependencies: L13.

Read this literally — it names work in **both** the elaborator and the CPS/closure backend:

1. **Elaborator (`src/smd/smdlisp/elaborator/elaborated_core.hpp`/`elaborate.hpp`):** two new core kinds,
   `core_block`/`core_return_from` (or whatever naming keeps with the file's `core_*` convention — `defun`,
   `setq`, etc. are the precedent). `block`/`return-from` join the special-form set `elaborate_list`
   recognizes (`QUOTE`, `IF`, `PROGN`, `LET`, `LET*`, `LAMBDA`, `FUNCTION`, `SETQ`, `DEFUN`, `DEFVAR`,
   `DEFPARAMETER`). **`return-from`'s block-name resolution happens at ELABORATION time, lexically** — the
   plan is explicit that an unknown block name is an elaboration error, not a runtime one. This means the
   elaborator needs to thread a "currently visible block names" scope (a stack/list of in-scope block names)
   through `elaborate_list`/`elaborate_lambda`/etc., analogous to how `let`/`let*` desugar but tracking NAMES
   rather than values — worth checking whether the existing elaborator has any scope-tracking machinery to
   extend, or whether this is the first thing that needs one. **`defun` bodies get an implicit
   `(block function-name ...)` wrapper per ANSI CL** — this is an elaborator-level change to `elaborate.hpp`'s
   existing `DEFUN` case (which currently calls `elaborate_lambda` directly on a synthesized formals/body
   list; the body now needs to be wrapped in an implicit block first, or the block-name-in-scope tracking
   needs to know `defun`'s own name is implicitly in scope for its body).
2. **CPS/closure backend (`src/smd/smdlisp/closure/cps_code.hpp`):** this is the part that actually needs a
   new kind of continuation value, not just two more `cps_dispatch`/`eval_direct` cases. Read the plan's
   sketch closely: "`block` registers a live record whose continuation is the block's `k`" — i.e., when
   `cps_dispatch` hits a `core_block` node, it needs to capture the CURRENT `k` (or `cont`, or some
   combination — work out which) as "what running past the end of this block resolves to," store it
   somewhere addressable by the block's name/an exit id, evaluate the body, and have `return-from` (found
   arbitrarily deep inside that body, not necessarily at the top of it) able to fetch that stored
   continuation and CALL IT DIRECTLY instead of returning a value up through the normal
   `cps_dispatch`/`cps_apply` recursion. This is a real design question: does the exit table live in a new
   parameter threaded alongside `environment`/`envs` (mutable, like environment), or does it need arena-style
   stable storage (like `env_arena`/`pair_heap`) because a live block's continuation must survive being
   captured by a NESTED closure that later tries to `return-from` through a dead exit (the "closure smuggling
   a `return-from` out of its extent" merge criterion implies exactly this scenario — a closure created
   inside the block, called after the block has already returned, whose body tries to `return-from` the
   now-dead block)? The `bool live` field in the plan's own `exit_record` sketch is the mechanism for
   detecting exactly that case as a diagnosed error rather than UB (D5's whole thesis). Read `env_arena`'s
   docs in `env.hpp` for the established "stable, arena-owned storage a closure can safely hold a pointer
   into" pattern before inventing a new one — an "exit arena" alongside `env_arena` is a plausible fit.
3. **`eval_direct.hpp`** also needs `core_block`/`core_return_from` cases: the STANDING expectation is that
   `eval_direct` stays a complete reference implementation of every landed core kind. A tree-walking
   interpreter's version of "invoke this block's continuation from deep inside a nested call" is C++
   exceptions or `longjmp`-style transfer, or (more in keeping with this project's `result<T>`-based error
   handling) a distinguished "escaping" `result` value that every frame between the `return-from` and the
   `block` propagates upward until the matching `block` catches it by id. Whichever mechanism you pick for
   `eval_direct`, it does NOT have to be the same mechanism the CPS backend uses — they are different
   architectures solving the same problem, exactly as `eval_direct` and `cps_dispatch` already solve every
   other form differently (recursion-with-return vs. explicit continuation-passing).

## Standing constraints for L14

- `src/smd/smdscheme/**` is frozen for semantic changes (D1). Read-only reference, never edit — and note
  again that it has NOTHING to adapt from for this step specifically (no `block`/`return-from`/`call/cc`
  analog exists there at all).
- New/changed files land in `src/smd/smdlisp/elaborator/` (new core kinds, special-form recognition) and
  `src/smd/smdlisp/closure/` (new `eval_direct.hpp` cases, new `cps_code.hpp` cases, the exit-table
  mechanism, possibly a new `env.hpp`-adjacent file for exit-record storage if an "exit arena" is the right
  shape). **Do not touch `src/smd/smdlisp/reader/` or `src/smd/smdlisp/macroexpand/`** — the latter is L19's
  lane, running concurrently. Keep your `closure/` edits confined to the escape machinery so the L19 merge
  (which may also touch `closure/` for its datum→value bridge) stays small.
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- File a divergence doc under `docs/divergences/` (`TEMPLATE.md` skeleton) for anything done differently
  than the plan specifies, or any knowing ANSI CL deviation. **Next free number is DIV-0009** — re-check
  `docs/divergences/` immediately before filing, since L19 may claim one concurrently.
- **This step has NO blog deliverable of its own** — phase 19 arrives at L15 (`catch`/`throw`/
  `unwind-protect`), per the plan's phase table. Don't draft one for L14.
- Before handoff: `make compile`, `make test`, `make lint`. No `make blog-md` needed (no blog deliverable).
- Do not continue past L14 into L15 (`catch`/`throw`/`unwind-protect`) unless blocked; if blocked, document
  the blocker here instead.

---

# Part 2 — Step L19: `defmacro` (Track C)

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
- **No gensym / hygiene machinery was built yet**, despite L17's DIV-0005 explicitly flagging L18/L19 as
  the natural place for it (`or`/`case`'s `%OR-TMP`/`%CASE-KEY` fixed temp names are still not hygienic). L18's
  backquote lowering does not itself need fresh symbols. **`defmacro`-defined macros are exactly where
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
`closure::symbol`/`closure::keyword`, L13 for compiled programs): watch where a reified
`datum_symbol`/`datum_keyword`'s `folded_name` comes from.** `closure::symbol`/`closure::keyword` hold a bare
`std::string_view name`, not an owned spelling (L11's handoff section explains why this was deliberately
deferred). Building a `reader::folded_name` for a reified datum node from that view is safe (folded_name owns
its own storage — copying a view into it is fine), but only if the `string_view` itself is still valid at the
moment of the copy; if a macro's compiled closure returns a symbol/keyword value whose `string_view` traces
back to something that could have gone out of scope (the same class of bug L10/L11/L13 hit and fixed), the
reification step needs to be the one that's careful, since nothing upstream of it enforces this anymore.

## Standing constraints for L19

- `src/smd/smdscheme/**` is frozen for semantic changes (D1). Read-only reference, never edit.
- New/changed files land primarily in `src/smd/smdlisp/macroexpand/` (the `defmacro` registration mechanism,
  the datum⇄value reification pair, wiring into `expand()`/`expand_and_elaborate`). **Prefer keeping the
  datum⇄value reification helpers inside `macroexpand/`** (reading `closure/` types via ordinary includes)
  rather than adding to `closure/` files — L14 (spine) is running concurrently and also touches `closure/`
  (`eval_direct.hpp`/`cps_code.hpp` + an exit-table mechanism), so anything you must add to `closure/` should
  be purely additive and minimal to keep the orchestrator's merge trivial. **Avoid touching
  `src/smd/smdlisp/elaborator/`** unless `defmacro`'s design genuinely forces it — L17 and L18 both needed zero
  elaborator changes, and the plan's phrasing ("compile-time evaluator," "reified back into the datum arena")
  suggests L19 shouldn't either, but confirm rather than assume; if it does, coordinate with L14, which owns
  the elaborator lane this round.
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- File a divergence doc under `docs/divergences/` (`TEMPLATE.md` skeleton) for anything done differently than
  the plan specifies, or any knowing ANSI CL deviation (e.g. if gensym/hygiene is deferred again, or if
  `defmacro`'s argument-destructuring lambda-list is narrower than ANSI's full macro lambda-list syntax — very
  likely, given `elaborate_lambda`'s own existing "flat list of symbols" restriction). **Next free number is
  DIV-0009** — re-check `docs/divergences/` immediately before filing, since L14 may claim one concurrently.
- Before handoff: `make compile`, `make test`, `make lint`. **This step has a blog deliverable: phase 20**, per
  the plan's phase table — follow L6/L10/L11/L13's precedent for authoring `#+transclude:` orgit links against
  this step's own worktree path (`docs/divergences/DIV-0004-orgit-transclusion-worktree-path.md`'s now-standing
  convention); the orchestrator repoints them to `main` after merge. Also run `make blog-md` and confirm only
  phase 20's `.md` and `index.md`/`index.org` show real content diffs (revert any incidental org-export
  id-churn in unrelated phases, same check every prior blog-deliverable step has done).
- Do not continue past L19 into L20 (multiple values) unless blocked; if blocked, document the blocker here
  instead.

---

## Standing constraints (apply to both L14 and L19, and everything after)

- `src/smd/smdscheme/**` is frozen for semantic changes (D1); blog phases 5-12 transclude live code from it
  by UUID anchor. Read-only reference, never edit.
- New work goes in `src/smd/smdlisp/`, namespace `smd::smdlisp`, one sub-namespace per directory.
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- Per `docs/divergences/DIV-0004-orgit-transclusion-worktree-path.md`'s now-standing convention: if your
  step has a blog deliverable, author its `#+transclude:` orgit links against your own worktree path, not
  `main` — the orchestrator repoints them after merge. Watch for the two org-markup footguns L11 and L13
  both hit while drafting (see `handoff.md`'s L11 and L13 entries): adjacent `~verbatim~`/`~verbatim~` spans
  with no space between them (including a bare `/` directly between two spans, e.g. `~foo~/~bar~`) merge
  into one giant span in GFM export, and a `~word~` immediately followed by a bare letter has the same
  problem — always leave a space or ordinary punctuation immediately after a closing `~`, on BOTH sides if
  joining two spans with a slash. (Of the two next steps, only L19 has a blog deliverable, phase 20; L14 does
  not.)
- Before handing off any step with a blog deliverable, verify `make blog-md` leaves every *other* phase's
  `.md` unchanged — `git status --short docs/blog/` after the render should show only your new/updated phase
  and `index.md`/`index.org`. If other `.md` files show diffs, `git diff` them first: if it's just
  `id="orgXXXXXXX"` churn, `git checkout --` them before committing; if the diff has real content changes,
  investigate before reverting.
- **The `value<Core>` string_view lifetime lesson is now a three-time-repeated pattern (L10, L11, L13).**
  Before writing ANY test helper that calls `read_datum`/`elaborate`/`eval_direct`/`compile_cps`/
  `compile_to_closure` and returns something derived from the result, check whether the thing you're
  returning (a `value<Core>` with a `symbol`/`keyword` alternative) could be viewing into memory owned by a
  LOCAL variable in your helper (an elaborated root, a compiled program, a datum/core arena). If so, either
  make that local a caller-owned parameter (like `eval_direct.test.cpp`'s `Core &root`) or inline the whole
  compile+evaluate sequence directly in the test body, factoring out only the final "evaluate an
  already-built thing" step.
