# Next step: Common Lisp pivot, Step L14 (block / return-from)

> **2026-07-21 update:** Step L13 (CPS closure backend, `cl-pivot/l13-cps-closure`) just landed in this
> worktree — see `handoff.md`'s "Step L13: CPS closure backend landed" section for the full story. That
> satisfies L14's only dependency.
>
> **Concurrency note:** Step L18 (backquote) may be landing in a sibling worktree around the same time,
> in the disjoint `src/smd/smdlisp/reader/` + `src/smd/smdlisp/macroexpand/` lanes (plus a small, additive
> `append`-builtin touch to `src/smd/smdlisp/closure/`). This file was written without knowledge of L18's
> status — if you are the L14 worker and `docs/divergences/`'s highest number or `checklist.md`'s L18 line
> looks inconsistent with what's written here, the orchestrator has already reconciled two independently
> rewritten `handoff-next.md`s and you're reading the merged result; trust `checklist.md` and
> `docs/divergences/` over this file's own numbering claims.
>
> **Divergence numbering:** DIV-0001..DIV-0007 are taken as of this step landing. DIV-0007 is L13's own
> (`compile_to_closure` needs a caller-owned datum arena). **Next free number is DIV-0008** — re-check
> `docs/divergences/` immediately before filing, since L18 may claim a number concurrently.

---

## What L13 landed, that L14 builds on

Read `handoff.md`'s "Step L13" section in full before starting. The essentials:

- **`src/smd/smdlisp/closure/{cps_code.hpp,closure_program.hpp}` exist now**, adapted from
  `smd::smdscheme::closure::{cps_code.hpp,closure_program.hpp}` (frozen, read-only reference) per
  `docs/cps-direction.md`'s structural-recursion-over-flat-arena architecture.
  `detail::cps_dispatch<MaxNodes,MaxList,MaxBindings,MaxEnvs,Cont,K>` pattern-matches all sixteen
  `elaborated_core.hpp` node kinds and threads a two-continuation `(cont, k)` pair: subexpressions needing
  an immediate value use `identity_k`/`identity_k`; a node's own tail position (an `if` branch, a `progn`'s
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
  forced divergence (DIV-0007), not a style choice: `smdlisp`'s case-folded symbol/keyword storage (D2)
  means `core_lambda::params`/`core_function::target`'s bare-symbol `std::string_view`s view into the datum
  arena's own memory, and that memory has to survive for as long as the compiled program is called, not just
  while it's being elaborated. If L14 needs to compile a whole program (e.g. for an end-to-end
  `block`/`return-from` test through `compile_to_closure`), read `closure_program.hpp`'s doc comment on
  `compile_to_closure` and `docs/divergences/DIV-0007-...md` before writing test helpers — the datum arena
  must be declared in the SAME scope that later inspects a `symbol`/`keyword` result, exactly like the core
  root already had to be in `eval_direct.test.cpp`.
- **A durable, now-three-times-repeated lifetime lesson (L10, L11, L13): never return a `value<Core>` out of
  a function whose only reference to whatever produced it (an elaborated root, a compiled `closure_program`,
  a compiled `cps_code`) was local to that function.** `value<Core>`'s `symbol`/`keyword` are bare
  `std::string_view`s, not owned spellings; the thing they view into (a core node, a datum arena, a compiled
  program's captured copy of either) has to outlive every inspection of the value. `closure_program.test.cpp`
  and `cps_code.test.cpp`'s test helpers were rewritten mid-step specifically because of this — read both
  files' header comments for the exact failure mode (one caught by AddressSanitizer, one caught only by
  applying the same discipline preemptively after the first bug was found, since the specific test that would
  have caught it happened to get inlined away). Any L14 test helper that builds a `block`/`return-from`
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
3. **`eval_direct.hpp`** also needs `core_block`/`core_return_from` cases for the merge criterion's own
   language ("passes through `eval_direct`" is implicit in every prior step's own merge criteria, and L13's
   own merge criterion was "every L11/L12 eval_direct scenario also passes through compile_to_closure" —
   the STANDING expectation is that `eval_direct` stays a complete reference implementation of every landed
   core kind). A tree-walking interpreter's version of "invoke this block's continuation from deep inside a
   nested call" is C++ exceptions or `longjmp`/`std::exit`-style control transfer, or (more in keeping with
   this project's `result<T>`-based error handling) a distinguished "escaping" `result` value that every
   frame between the `return-from` and the `block` propagates upward until the matching `block` catches it
   by id. Whichever mechanism you pick for `eval_direct`, it does NOT have to be the same mechanism the CPS
   backend uses — they are different architectures solving the same problem, exactly as `eval_direct` and
   `cps_dispatch` already solve every other form differently (recursion-with-return vs. explicit
   continuation-passing).

## Standing constraints for L14

- `src/smd/smdscheme/**` is frozen for semantic changes (D1). Read-only reference, never edit — and note
  again that it has NOTHING to adapt from for this step specifically (no `block`/`return-from`/`call/cc`
  analog exists there at all).
- New/changed files land in `src/smd/smdlisp/elaborator/` (new core kinds, special-form recognition) and
  `src/smd/smdlisp/closure/` (new `eval_direct.hpp` cases, new `cps_code.hpp` cases, the exit-table
  mechanism, possibly a new `env.hpp`-adjacent file for exit-record storage if an "exit arena" is the right
  shape). Do not touch `src/smd/smdlisp/reader/` or `src/smd/smdlisp/macroexpand/` (L18's lane, if running
  concurrently).
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- File a divergence doc under `docs/divergences/` (`TEMPLATE.md` skeleton) for anything done differently
  than the plan specifies, or any knowing ANSI CL deviation. **Next free number is DIV-0008** — re-check
  `docs/divergences/` immediately before filing.
- **This step has NO blog deliverable of its own** — phase 19 arrives at L15 (`catch`/`throw`/
  `unwind-protect`), per the plan's phase table. Don't draft one for L14.
- Before handoff: `make compile`, `make test`, `make lint`. No `make blog-md` needed (no blog deliverable).
- Do not continue past L14 into L15 (`catch`/`throw`/`unwind-protect`) unless blocked; if blocked, document
  the blocker here instead.

---

## Standing constraints (apply to L14 and everything after)

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
  joining two spans with a slash.
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
