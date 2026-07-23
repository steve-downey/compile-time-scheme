# step-brief-l18.md — Step L19: defmacro (Track C)

Forward-only brief for the next clean agent on **Track C**. Bounded; not a log.
Prior-step narrative lives in `git log` / `docs/history/handoff-archive.md`;
architecture lives in `docs/compiler_architecture.org` (not yet extended for
`smdlisp`; see the note below). Read the L19 section of `docs/cl-pivot-plan.md`
for the authoritative spec (orchestrator pastes it) — this brief does not
restate it because it was not available to the agent that wrote this file.

**Status:** L18 (backquote, Track C) is merged; L19 depends on L18 (satisfied).
L13 (CPS closure backend, Track A) may still be running concurrently in a
non-overlapping lane — check `step-brief-l13.md`'s status line. Likely
cross-lane conflict: tracking docs (`checklist.md`, this file) and top-level
`src/smd/smdlisp/CMakeLists.txt`.

## Goal (plan §9, L19 — checklist says "defmacro (+ phase 20 draft)")

Not restated in detail here (unavailable to this brief's author); the
checklist entry names `defmacro` plus a phase-20 blog draft. Read the pasted
plan section for the exact scope, file locations, and merge criterion.

## What L18 built, and what L19 can rely on

- **Reader** (`src/smd/smdlisp/reader/`): `` ` ``, `,`, `,@` now read to three
  new datum kinds — `datum_backquote`, `datum_unquote`, `datum_unquote_splice`
  (`datum_type.hpp`; the datum variant is now 9-wide, was 6). `` ` ``/`,`/`,@`
  are terminating macro characters (`cl_chars.hpp`).
- **Expander** (`src/smd/smdlisp/macroexpand/expander.hpp`): `expand_datum` has
  a dedicated case for `datum_backquote` (not a `host_macro` entry — `` ` `` is
  reader syntax, never a symbol-headed call form). `detail::expand_backquote_template`
  / `detail::expand_backquote_list_elems` lower a template to `CONS`/`APPEND`
  call-form code (literal spans wrapped in a native `datum_quote` via
  `detail::make_quote`), then the lowered form is recursively `expand_datum`'d
  — so a backquote template written **inside** a macro's body (the standard
  CL macro-writing idiom: `` `(defun ,name () ,@body) ``-style expansions) gets
  lowered and macro-expanded automatically wherever it appears in the tree,
  with no special wiring `defmacro` needs to add for that interaction.
- **`append` joined the builtin set**, as the plan's L18 line required:
  `closure::builtin_op::append` / `closure::list_op::append`
  (`value.hpp`/`pairs.hpp`), bridged in `eval_direct.hpp`'s `to_list_op`, and
  registered as the FUNCTION-namespace `APPEND` in all three
  `closure::default_env` overloads (`env.hpp`). `pairs.hpp::detail::append_two`
  is the recursive copy-onto-shared-tail implementation if L19 needs list
  utilities of its own.
- **`docs/compiler_architecture.org` was not touched.** That doc is still
  scoped entirely to the pre-pivot `smdscheme` presentation pipeline (phases
  1–7, UUID-anchored for blog/slide transclusion) and has no `smdlisp` section
  or anchors to extend in place; the immediately-prior L17 step made the same
  call. Durable `smdlisp` facts currently live in the code's own doc comments
  (extensive throughout `reader/`, `macroexpand/`, `closure/`) plus the DIV
  docs. If L19 is the step that should finally open an `smdlisp` section
  there, that is a scope decision for the orchestrator, not something to
  infer silently.

## Open items L19 should look at

- **DIV-0006** (`or`/`cond`/`case` use fixed reserved binding names, not
  `gensym`) is **still open**. L18 added template *construction* syntax but no
  fresh-symbol generation — L17's own note named L18/L19 together as "when
  real `gensym` machinery arrives." Check whether L19's plan section expects
  `defmacro` (needing a compile-time macro-body evaluator regardless) to also
  supply `gensym` and close DIV-0006, or whether that is deferred further.
- **DIV-0008** (nested backquote, e.g. `` `(a `(b ,c)) ``, has no depth
  tracking — a nested template is opaque literal data, escapes and all) is
  open. Real CL macros commonly nest backquote (a macro whose body constructs
  another macro's template). If `defmacro` test cases need this, L19 must
  either implement depth tracking in
  `detail::expand_backquote_template`/`expand_backquote_list_elems` or file a
  narrower divergence describing the gap for the macros actually implemented.
- `host_macro` (`macroexpand/expander.hpp`) is a **C++-function** registry —
  it has no notion of a Lisp-defined transformer. `defmacro` almost certainly
  needs a second, runtime-populated table (name -> something that can be
  invoked to produce a replacement datum from a call-form datum) rather than
  reusing `host_macro` as-is; how that invocation runs the macro body (through
  `eval_direct` against a value that isn't a `core_type` node, or some other
  path) is exactly the kind of design decision the pasted L19 section should
  settle — do not guess at it from this brief.

## Standing constraints

- `src/smd/smdscheme/**` is frozen (D1) — read-only reference, never edit.
- Your lane's prior surface is `src/smd/smdlisp/reader/` and
  `src/smd/smdlisp/macroexpand/`; L19 may need `closure/` too (a macro-body
  evaluator likely lives there or in `elaborator/`) — confirm against the
  pasted plan section rather than assuming L18's file list still applies.
- Never edit inside existing UUID anchor blocks; do not touch
  `closure/cps_code.hpp`/`closure_program.hpp` (Track A's files, per L13's
  brief) unless the orchestrator's L19 instructions explicitly say otherwise.
- C++26/GCC16 baseline; Catch2; mirror file prolog / include-guard /
  canonical-include conventions used throughout `src/smd/smdlisp/`.
- File a DIV under `docs/divergences/` for anything done differently or any
  knowing ANSI CL deviation — **check `docs/divergences/` for the actual next
  free number** before filing (concurrent lanes may claim numbers too; next
  free as of L18's merge is DIV-0009).
- Before handoff: `make compile`, `make test`, `make lint`. Do not continue
  past L19 into L20 unless blocked; if blocked, document it here.
