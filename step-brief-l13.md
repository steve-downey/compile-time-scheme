# step-brief-l13.md — Step L13: CPS closure backend (Track A)

Forward-only brief for the next clean agent on **Track A**. Bounded; not a log.
Prior-step narrative lives in `git log` / `docs/history/handoff-archive.md`;
architecture lives in `docs/compiler_architecture.org`. Read the L13 section of
`docs/cl-pivot-plan.md` for the authoritative spec (orchestrator pastes it).

**Status:** L12 (spine) is merged; L13 depends on L12 (satisfied). L18 (backquote,
Track C) runs concurrently in a non-overlapping lane — see `step-brief-l18.md`. Likely
integration conflict between lanes: tracking docs (`checklist.md`, this file) and
`src/smd/smdlisp/CMakeLists.txt`.

## Goal

Build `src/smd/smdlisp/closure/{cps_code.hpp,closure_program.hpp}` (+ tests), adapted
by copy from the Scheme CPS backend `smd::smdscheme::closure::cps_code`/
`closure_program`, per `docs/cps-direction.md`: **structural recursion over the flat
arena (Option A)**, not `fix<F>` folding (Option B, rejected — `fix<F>` is not a
literal type). Read `docs/cps-direction.md` and the Scheme originals in full first;
this is "adapt then diverge," so the Scheme shape is the starting point. Name the
public compile entry point to mirror `smdscheme::closure::closure_program`'s.

## Merge criterion (plan §9, L13)

Every L11/L12 end-to-end test also passes through the CPS/closure path. Concretely:
- `(if nil 1 2)` ⇒ `2`
- `((lambda (x) (car (cdr x))) '(1 2 3))` ⇒ `2`
- `(funcall #'cons 1 nil)` ⇒ a `pair_ref` cell
- `(progn (defun twice (x) (+ x x)) (twice 4))` ⇒ `8`

Plus a representative sample of the fuller `eval_direct.test.cpp` suite (closures
capturing both namespaces; `apply`/`funcall`; `setq` shared-store-across-closures;
`defvar`/`defparameter` init-vs-no-reinit) — those expose CPS environment-threading
regressions a narrow "just the merge criteria" set would miss.

## What L13 must get right (from L12; detail in eval_direct.hpp / arch doc)

The CPS backend must **cover all fifteen** core kinds — the twelve from L10 plus
`core_setq`/`core_defun`/`core_defvar` (in scope, not deferred). The subtle,
forward-looking constraints:

- **Environment is a mutable reference** (`env<Core,MaxBindings>&`) threaded through
  `eval_direct`; `defun`/`defvar`/`setq` mutate it in place and later siblings in a
  `progn`/lambda-body sequence observe the mutation. The CPS threading must preserve
  this — mutate in place and share across the continuation chain, not copy fresh per
  step, or `(progn (defun f ...) (f ...))` regresses under CPS. **Read
  `smd::smdscheme::closure::cps_code.hpp`'s `begin`/`core_begin` wiring** — the plan's
  named pattern for `core_progn`, and the precedent for whether Scheme's CPS already
  solved mutation-visibility-across-a-sequence (it did for `set!`, the same problem).
- **`setq` returns the assigned value** (not `void`, no `unspecified` kind exists in
  `smdlisp` — don't add one); the `core_setq` continuation passes that value forward.
- **`store<Core,MaxStore>` backs `setq`** (mutable mode = shared store pointer across
  every `env` copy incl. closure captures). The CPS env representation needs the
  equivalent mutable-mode story, or `setq` is only visible within one call frame.
  Mirror the store-pointer-sharing by construction; don't rediscover via a failing
  shared-closure test.
- **`defun`/`defvar`/`defparameter` return the name symbol**, never the closure/init
  value (ANSI contract). `core_defvar` CPS only mirrors `eval_direct`'s
  `mark_special`/optional-`define_value`; **no dynamic-binding machinery** (that's
  L16).
- **`defun` reuses the lambda CPS lowering** — `core_defun.lambda_node` is a real
  `core_lambda`; don't build a second lambda-under-CPS path. Add only the
  function-namespace-definition side effect + name-as-return-value.

## Deliverable: blog phase 18 draft

`docs/blog/phase-18-setq-defun-progn.org` (working title "`setq`, `defun`, `progn`: a
programmable core") — covers **L12's** material (docs-lags-code-by-one-step), using
L13's CPS tests as live UUID-anchored code. Follow DIV-0004: author `#+transclude:`
links against your own worktree path; the orchestrator repoints after merge. Two
org-markup footguns: adjacent `~verbatim~` spans need a space/punctuation between
them, and `~word~` immediately followed by a bare letter has the same problem.

## Standing constraints

- `src/smd/smdscheme/**` is frozen for semantic changes (D1) — read-only reference,
  never edit; blog phases 5–12 transclude it live by UUID anchor.
- New/changed files land in `src/smd/smdlisp/closure/` only (two new files + tests).
  Do not touch `reader/`, `elaborator/`, or `macroexpand/` (L18's lane).
- C++26/GCC16 baseline; Catch2; mirror file prolog / include-guard / canonical-include
  conventions used throughout `src/smd/smdlisp/`.
- File a DIV under `docs/divergences/` for anything done differently or any knowing
  ANSI CL deviation — **check `docs/divergences/` for the actual next free number**
  before filing (the two lanes may claim numbers concurrently).
- Before handoff: `make compile`, `make test`, `make lint`; draft blog phase 18; if it
  has a blog deliverable, verify `make blog-md` leaves every *other* phase's `.md`
  unchanged (revert `id="orgXXXX"` churn per L6/L11 precedent). Do not continue past
  L13 into L14 unless blocked; if blocked, document it here.
