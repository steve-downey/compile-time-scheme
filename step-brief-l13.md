# step-brief-l13.md — Step L24: documentation consolidation (Track A)

Forward-only brief for the next clean agent on **Track A**. Bounded; not a log.
This file keeps Track A's lane-brief filename (`step-brief-l13.md`); it now
describes **L24**, per the step-brief contract in `AGENTS.md`.
Prior-step narrative lives in `git log`; deliberate deviations live in
`docs/divergences/`.

**Status:** L21 (sender backend for the CL core) is merged into this lane's
branch. L11–L19 and L21 are satisfied; the three backends —
`closure/eval_direct.hpp`, `closure/cps_code.hpp`, `sender/sender_eval.hpp` —
now run the same L11–L16 test set. L20 (multiple values) is being built in a
sibling lane and is **not** on this branch; L23 (`tagbody`/`go`) is optional
and unstarted.

## The step (plan §L24)

Consolidate the Common Lisp layer into `docs/compiler_architecture.org`.

That document has deliberately had **no `smdlisp` section** for the whole
pivot: its own introduction says the CL layer "is consolidated into this
document at step L24". Five workers re-decided the deferral rather than
breaking it. **L24 is the step that is finally allowed to add one**, and it is
the only step that should.

Everything transcluded in that document today points into
`src/smd/smdscheme/**`, the frozen reference tree. Adding the CL layer means
adding transclusions into `src/smd/smdlisp/**` alongside them, by UUID anchor.

## What exists to transclude, and where the anchors are

L21 added anchors specifically so this step would have regions to quote. In
`src/smd/smdlisp/sender/`:

- `outcome.hpp` — `5c044086-…` (the three-channel mapping, and why the closure
  backends' unwind sentinels are deleted rather than ported),
  `6ffe3840-…` (`outcome_receiver` / `run_sender`, and why not `sync_wait`).
- `defer_outcome.hpp` — `e64f025f-…` (the one leaf sender; laziness is
  load-bearing).
- `unwind_protect.hpp` — `cae50b27-…` (the adapter whose three completion
  functions funnel into one — decision D5 made structural).
- `sender_eval.hpp` — `59265482-…` (the `foundation::fix` Mendler bridge),
  `44233d7a-…` (`core_cons`, the only honest `when_all`),
  `02f32bd6-…` (`block`/`return-from`/`catch`/`throw`: the escape machinery),
  `06514953-…` (`core_unwind_protect` delegating to the adapter).
- `build_lisp_tree.hpp` — `45796b9c-…` (the reflection classifier).

Older anchors on the closure side: `closure/env.hpp` (`1232b7f6-…` the unwind
markers, `a5b4dac9-…` `catch_record`, `d5b819ef-…` `dynamic_binding`,
`107e5185-…`/`13f2e157-…` the arena stacks) and `closure/eval_direct.hpp`
(`e0468b60-…` the shared dynamic-binding helpers, `1d30e953-…` call dispatch).

**Merge criteria:** `docs/compiler_architecture.org` describes the CL layer
with live transclusions, and the org toolchain still resolves every link.

## Durable facts already recorded, do not re-derive

- `docs/compiler_architecture.org` Phase 6 now carries a subsection on the
  three completion channels as a semantic resource — written to be
  backend-general, not CL-specific. Build on it rather than restate it.
- The one thing that does **not** move to a receiver is stated there too:
  dynamic-binding restore stays in ordinary C++ control flow, because it
  brackets a callee's whole extent rather than a single sender's completion.

## Gotchas that will bite you

- **`src/smd/smdscheme/**` is frozen (D1).** Read it, copy from it, link it;
  do not edit it. Every existing transclusion in the architecture document
  targets it, so those links must keep resolving unchanged.
- **The anchor rule is narrower than a previous version of this brief
  claimed.** See `AGENTS.md`, "UUID anchors and the frozen Scheme tree":
  inside `src/smd/smdlisp/**` you **may** edit the contents of an anchored
  region; only *deleting or nesting* an anchor pair is forbidden. An
  over-broad "never edit inside a UUID anchor block" previously cost two
  workers real design work. Do not restate it in stronger form than
  `AGENTS.md` puts it.
- **DIV-0016 asks L24 for something specific:** record the two fixed-point
  types (`smd::fixpoint::Fix` and `smdscheme::foundation::fix`) as a known
  duplication, since it reads as an accident when found later.
- **DIV-0017 asks L24 to re-check itself:** the sender backend does not cover
  multiple values. L20 is expected to close that gap; verify the current state
  before describing the sender backend's coverage, rather than copying the
  divergence's wording.
- **DIV-0015 is a real limit on the parity claim:** the sender backend has
  runtime evidence only, no `static_assert` twins, because Beman's
  `connect`/`start` is not constant-evaluable. Do not describe all three
  backends as compile-time evaluators.
- **Recursive `defun` is unsupported (DIV-0009)**, so no worked example in the
  prose may rely on self-recursion. L14, L15, L16 and L21 all worked around it.
- **`MaxBindings` is still hard-wired to 16** in all three backends
  (`static_assert`ed in each). A real fix threads it through `value`, `store`,
  `pair_cell`, `pair_heap`, `apply_prim`, `env`, `env_arena` and three
  evaluators; L16 and L21 both declined it as its own step. Do not half-do it
  in a documentation step either.

## Standing constraints (unchanged)

- Lane = `elaborator/**` + `closure/**` + `sender/**` + `docs/**`. Do NOT
  touch `macroexpand/**` or `reader/**`.
- Vendor Beman Execution as the git submodule at `vendor/execution`,
  integrated with `add_subdirectory`. No FetchContent, no vcpkg, no
  `find_package` for it.
- New anchors must be real `uuidgen` output and must exist at the step's merge
  commit — a post pinned to that tag can only transclude anchors present there.
- C++26/GCC16; Catch2; component header included first and twice; file prolog
  with repo-relative path + Emacs mode line, then SPDX; classical include
  guards; canonical angle includes; co-located tests; constexpr-first.
- File a DIV for any knowing ANSI CL deviation. **DIV-0017 is the highest used
  by this lane**; the L20 lane takes numbers above it, so re-check
  `docs/divergences/` and take the next unused number.
- Before handoff: `make compile`, `make test`, `make lint` all green, and
  `make compile` warning-free — the smdlisp tree currently has zero warnings.
- Do not write the blog draft and do not tick your `checklist.md` box until the
  orchestrator says so: blog posts are dispatched separately *after* the step's
  `--no-ff` merge, so transclusions can be pinned to a tag that exists.
- Do not continue past L24. Document blockers here.
