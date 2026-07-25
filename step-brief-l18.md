# step-brief (Track C) — next step: L20 (multiple values)

Forward-only brief for the next clean agent on **Track C**. Bounded; not a log.
Read the L20 section of `docs/cl-pivot-plan.md` for the authoritative spec (the
orchestrator pastes the one step section you need) — this brief does not restate
it. Prior-step narrative lives in `git log`; divergences live under
`docs/divergences/`.

**Status:** L19 (`defmacro`, Track C) is merged/landed; L20 depends on L19
(satisfied). Likely cross-lane conflict surface: tracking docs (`checklist.md`,
this file) and any top-level `CMakeLists.txt` a new component needs.

## Goal (checklist entry: "Step L20: multiple values")

Not restated here (the L20 plan section was not available to this brief's
author). The checklist names "multiple values" — expect `values` /
`multiple-value-bind` / `multiple-value-call`-style facilities. Read the pasted
plan section for exact scope, file locations, and merge criterion before
editing.

## What L19 built, and what L20 can rely on

- **Object-language macros exist.** `src/smd/smdlisp/macroexpand/expander.hpp`
  now has `defmacro`: a user macro's expander is Lisp code, elaborated by
  `elaborator::elaborate` and run by `closure::eval_direct` during the
  expansion pass. The compile-time evaluator plumbing (a private pair heap,
  env, env-arena, and a compile-time-only core arena) is bundled in
  `macro_context`; `expand_and_elaborate` / the context-less `expand_datum`
  each construct one fresh per top-level form.
- **datum⇄value reification** is the reusable new machinery:
  `detail::datum_to_value` (unevaluated argument datum → `closure::value`) and
  `detail::value_to_datum` (evaluator result → datum). If L20 needs to move
  data across the datum/value boundary, reuse these rather than re-deriving.
- **CMakeLists:** `smdlisp.macroexpand` now links `smdlisp.closure` and
  `smd.fixpoint` (it calls the evaluator). Already committed.
- **Three org-transclusion UUID anchors** were added in `expander.hpp`
  (`macro_context`, `process_defmacro`, `invoke_user_macro`) for phase-20's
  blog. Do not delete/nest them.

## Open items L20 should be aware of (do NOT assume they are closed)

- **DIV-0006 (gensym) is still OPEN.** `or`/`cond`/`case` still use fixed
  reserved temp names (`%OR-TEMP` etc.), not fresh symbols. L19 supplied the
  prerequisite (a compile-time evaluator that runs macro-body code) but did
  **not** add a `gensym` builtin, because doing so would require editing the
  read-only `closure/` files (value variant, `eval_direct` dispatch,
  `default_env`). If a step legitimately owns `closure/`, that is where gensym
  belongs. Do not close DIV-0006 without adding a real `gensym`.
- **DIV-0008 (nested backquote) is still OPEN.** A macro body may use
  single-level backquote (tested), but a template nested inside another
  template is opaque literal data.
- **DIV-0010 (this step):** `defmacro` registrations are scoped to one
  top-level form, not the session; the macro lambda list is required params +
  one `&rest`/`&body` only (no `&optional`/`&key`/`&aux`/destructuring); a
  `defmacro` form is replaced by `(quote name)`; reification rejects raw
  backquote in argument position and dotted-list expansions. If L20 needs any
  of these lifted, that is new scope — read the plan section, do not assume.

## Standing constraints

- `src/smd/smdscheme/**` is frozen (D1) — read-only reference, never edit.
- Treat `src/smd/smdlisp/closure/{eval_direct,cps_code,closure_program,value,env,pairs}.hpp`
  and `src/smd/smdlisp/elaborator/**` as READ-ONLY unless the pasted L20
  section explicitly puts them in your lane — CALL them, do not modify them.
  If you find you genuinely must, STOP and report it as a blocker.
- `docs/compiler_architecture.org` is still smdscheme-only (L13/L17/L18/L19 all
  left it untouched); durable smdlisp facts live in code doc-comments + DIV
  docs. Opening an smdlisp section there is an orchestrator scope decision, not
  something to infer silently.
- Never edit inside existing UUID anchor blocks.
- C++26/GCC16 baseline; Catch2; mirror file prolog / include-guard /
  canonical-include conventions used throughout `src/smd/smdlisp/`.
- File a DIV under `docs/divergences/` for any knowing ANSI CL deviation.
  **Next free number is DIV-0011** as of L19's landing (DIV-0010 was L19's);
  re-check the directory before filing, since concurrent lanes may claim
  numbers.
- If your step has a blog deliverable, author `#+transclude:` orgit links
  against your own worktree path (DIV-0004); the orchestrator re-points them to
  `main` at merge. Generate only your phase's `.md` (`make
  docs/blog/phase-NN-*.md`) and hand-edit `index.org`/`index.md` — do not let
  `make blog-md` churn other phases' anchor ids.
- Before handoff: `make compile`, `make test`, `make lint`. Do not continue
  past L20 unless blocked; if blocked, document it here.
