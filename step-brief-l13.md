# step-brief-l13.md — Step L14: block / return-from (Track A)

Forward-only brief for the next clean agent on **Track A**. Bounded; not a log.
This file is still named `step-brief-l13.md` (Track A's lane-brief filename); it
now describes **L14**, per the step-brief contract in `AGENTS.md`. Prior-step
narrative lives in `git log`; architecture lives in
`docs/compiler_architecture.org`.

**Status:** L13 (CPS closure backend) is merged; L14 depends on L13
(satisfied). L18 (backquote, Track C) runs concurrently in a non-overlapping
lane — see `step-brief-l18.md`. Likely integration conflict surfaces are still
`checklist.md` and `src/smd/smdlisp/CMakeLists.txt`, shared with the L18 lane.

## Missing input: get the L14 section of docs/cl-pivot-plan.md

Unlike L13's brief, this one does **not** have the orchestrator's pasted L14
step section to work from — this handoff is being written between orchestrator
turns. `checklist.md` names the step only as "Step L14: block / return-from";
that is everything this brief can certify first-hand. Before starting, get the
actual L14 step section (plan §9) — either from the orchestrator's prompt, or,
if working un-orchestrated, by opening `docs/cl-pivot-plan.md` **at the L14
section only, by anchor/heading, not front-to-back** (the reading-contract
exception for "on demand, by anchor/named section only, never wholesale" is
exactly for this situation). Do not guess the merge criterion or the exact
file list from the one-line checklist description.

## What L13 landed, and what L14 can now rely on

- `src/smd/smdlisp/closure/cps_code.hpp` and `closure_program.hpp` (+ tests):
  a CPS-style evaluator for all fifteen `smdlisp` core node kinds, a drop-in
  alternative to `eval_direct.hpp` — every L11/L12 end-to-end program produces
  the same answer on both. `cps_dispatch`'s two-continuation (`cont`/`k`)
  shape is adapted from `smd::smdscheme::cps::detail::cps_dispatch`; see the
  doc comments on `cps_dispatch`/`cps_apply_function_value` in `cps_code.hpp`
  for the non-tail-position idiom (recurse with `identity_k<Core>{}` for both
  continuations) versus tail position (thread `cont`/`k` onward) — L14 will
  need the identical idiom for whatever new core node(s) it adds, in both
  `eval_direct.hpp` and `cps_code.hpp`.
- **The environment-mutation-across-a-sequence invariant now has two
  witnesses, not one.** `environment` (`env<Core,MaxBindings>&`) is threaded
  as the same mutable reference through every recursive call in *both*
  evaluators — never copied mid-sequence. Whatever L14 adds (a lexical
  `block`/`return-from` non-local exit, most likely a distinguished
  error/control value threaded up through the existing
  `result<value<Core>>` return channel rather than a new parameter) must
  preserve this in both places, or the merge criterion (parity between the
  two evaluators) breaks silently on exactly the kind of program that mixes a
  `progn`/lambda-body sequence with the new control form.
- **DIV-0007**
  (`docs/divergences/DIV-0007-closure-program-datum-arena-caller-owned.md`):
  `smd::smdlisp::closure::compile_to_closure` takes the *datum* arena as a
  caller-owned reference parameter — it is **not** signature-compatible with
  `smd::smdscheme::closure::compile_to_closure`'s single-string-argument form.
  Root cause: `smdlisp`'s elaborated core nodes hold `std::string_view`s into
  the datum arena for every list-element name (lambda params, `setq`/`defun`/
  `defvar` names); that view is safe only as long as the datum arena outlives
  the program. If L14 (or any later step) calls `compile_to_closure`, pass a
  caller-owned `tree_arena<reader::datum_type<...>, MaxNodes>` alongside the
  source string and keep it alive as long as the program is called — see
  `closure_program.hpp`'s doc comment on `compile_to_closure` for the full
  mechanism.
- `docs/blog/phase-18-setq-defun-progn.org` (+ `.md`) drafted, covering L12's
  material via L13's CPS tests, per the docs-lag-code-by-one-step pattern —
  L14's own blog deliverable (if it has one; the checklist line above doesn't
  say) will cover *L13's* material the same way, one step later.

## Standing constraints (unchanged from L13's brief)

- `src/smd/smdscheme/**` is frozen for semantic changes (D1) — read-only
  reference, never edit.
- C++26/GCC16 baseline; Catch2; mirror file prolog / include-guard /
  canonical-include conventions used throughout `src/smd/smdlisp/`.
- File a DIV under `docs/divergences/` for anything done differently or any
  knowing ANSI CL deviation — check `docs/divergences/` for the actual next
  free number first (both DIV-0007 and L18's concurrently-claimed DIV-0008 are
  now taken; the next Track-A DIV is **DIV-0009 or higher**, but re-check
  before filing since L18 may have claimed more in the meantime).
- Before handoff: `make compile`, `make test`, `make lint`; if the step has a
  blog deliverable, verify `make blog-md` leaves every *other* phase's `.md`
  unchanged. Regenerating `index.md` re-randomizes every heading's
  `orgXXXXXXX` anchor id on each Emacs export run — don't let that churn land;
  edit `docs/blog/index.md`/`index.org` by hand to add just the new entry
  instead of committing whatever `make blog-md` regenerates for that file,
  exactly as this step's own handoff had to do for the Phase 18 entry.
- Do not continue past L14 into L15 unless blocked; if blocked, document it
  here.
