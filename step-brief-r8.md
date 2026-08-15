# Step brief: R8 (extract the kit)

Forward-only handoff for the next agent.
Read `docs/codestyle.org`, `AGENTS.md`, `docs/CODING_RULES.md`, `CLAUDE.md`, this file, and — last, immediately before writing code — `docs/cpp-rules.md`.
Nothing else, except the anchored sections named under "Dependencies" below.

## Next step's goal

`docs/cl-rebuild-plan.md` §5 ("The kit") and §6's `R8 — extract the kit` entry: `src/smd/cl` is now the third working client (after `smdscheme` and `compile-time-forth`) of a constexpr computation substrate that has so far been developed as two independently-drifted copies.
§5 names the actual contents already: `foundation/` — `static_vector`, `result`, `parse_error`, `source_pos`, `source_span`, `arena_box`, `functor`, `applicative`, `alternative` — plus `parser/` — `cursor`, `parser`, `alt`, `parser_ops`.
Diffing the `smdscheme` and `forth` copies of these was 7–15 changed lines per file at the time §5 was written, mostly include guard, namespace, and an "adapted by copy" comment — that is the signal the plan is reading as "this wants to be a library."
The orchestrator pastes the plan's full §5 and the R8 entry of §6 alongside this brief; this file does not restate them at length.

**What is explicitly not in scope.** §5 is emphatic: AST, `fix`, recursion schemes, and the elaborator are not shared, because `compile-time-forth` has no AST at all — it has `machine/` and `interpreter/` over a VM, not a compiler front end. Do not fold `src/smd/cl/core`, `src/smd/cl/elaborator`, or anything Lisp-family-specific into the kit. The kit is the substrate three different *kinds* of language front end all needed independently, not a Lisp toolkit.

**Where `compile-time-forth` lives is not something R7 could verify.** This worktree is `compile-time-scheme`; `compile-time-forth` is a sibling project the rebuild plan refers to by name and by the shape of its `foundation/`/`parser/` copies, but no path into it was available from inside this repository's tooling. Confirm where it actually lives (a sibling checkout, a separate remote, or something else) before assuming a three-way diff is mechanically available the way it was when §5 was written. If it is not reachable, the honest move is to extract the kit from the two copies this repository *does* contain (`smdscheme/foundation`+`parser` and `cl/foundation`) and record in a divergence note (or a correction to §5's own claim) that the third comparison point could not be re-verified from here, rather than asserting parity with a copy nobody re-read.

## Merge criterion

Whatever the orchestrator's pasted §5/§6 text states as R8's own criterion, plus the standing ones this repo always asks: `make compile`, `make test-matrix` (both `Debug` and `Asan` legs — report which you ran), `make lint`, `make compile-headers`, `scripts/verify-transclusions.sh`, all green.
If the kit becomes a real extracted target (its own `CMakeLists.txt`, its own namespace), every existing caller (`smdscheme.foundation`, `smdscheme.parser`, `cl.foundation`, and anything under `src/smd/smdlisp` that reaches `smdscheme.foundation`) must still build and pass unchanged — this is a refactor with three tenants, not a rewrite with one.

## Files this step owns

```txt
src/smd/cl/foundation/*          (existing; becomes a caller of the kit, or is superseded by it)
src/smd/<kit-name>/*             (new location, name and placement is this step's call — see below)
checklist.md                     (tick R8)
step-brief-r8.md                 (retire), step-brief-r9.md or the rebuild's closing note (write)
```

`src/smd/smdscheme/**` stays dead-ended: read it, diff it, copy from its current state into the kit — never edit it in place, even to converge it onto the kit's shape. If `smdscheme.foundation`/`smdscheme.parser` should become thin wrappers over the kit rather than staying independent copies, that is a judgement call this step gets to make and record, not a mandate — re-read `AGENTS.md`'s "Which trees you may edit" before deciding, since "never edit in place" is the one rule among the three, not a preference.
`src/smd/smdlisp/**` is the behavioural oracle and is never edited, from R1 onward — unaffected either way, since it depends on `smdscheme.foundation`, not `cl.foundation`.
`src/smd/cl/sender/**`, `src/smd/cl/eval/**`, `src/smd/cl/conformance/**` are R5/R6/R7's and should not need touching: nothing in the kit's proposed contents (`static_vector`, `result`, `parse_error`, `source_pos`, `source_span`, `arena_box`, `functor`, `applicative`, `alternative`, `parser/*`) is a type this step's own `eval::limits`/`eval::machine`/`sender::machine_sender` would need to change shape for — they consume `foundation::static_vector`, `foundation::result`, `foundation::parse_error` by name, and an extraction that preserves those names' behaviour needs no caller-side change beyond, at most, an include path.

## Dependencies already satisfied

- `docs/compiler_architecture.org` § "The sender backend drives the machine; it does not fork it" (R7) — `smd::cl::sender::machine_sender`, `smd::cl::sender::run_sender`, and the D13 channel mapping they spend a second time. Nothing here is a kit candidate: it is Lisp-family evaluator machinery, layered above the substrate `docs/cl-rebuild-plan.md` §5 is asking R8 to extract, not inside it.
- `docs/compiler_architecture.org` § "The evaluator is a machine, not a fold" and § "D13's three channels are now a type, not a discriminator convention" (R5) — `eval::machine`, `eval::outcome`; both consume `foundation::result`/`foundation::parse_error`/`foundation::static_vector` as ordinary dependencies, so an extraction that keeps those three call-compatible is transparent to `eval/**`.
- R1–R4 substrate as it stands today: `src/smd/cl/foundation/` is "the reviewed union of the `smdscheme` and `forth` copies" per §6's own R1 entry — meaning R1 already did a two-way merge once. R8 is not that merge again; it is deciding where the *shared* result of that kind of merge should physically live once a third client exists, and updating however many `CMakeLists.txt`/include paths that implies.

## What R7 found that this step needs and cannot get elsewhere

- **`eval::limits` and `eval::machine`'s NTTP-configured capacities are the existing precedent for "capacity is storage, not identity" (D14), and the kit's `static_vector`/`arena_box` are exactly that pattern one layer down.** Nothing about extracting them should reopen D14; if a kit-shaped `static_vector` shows up parameterised on anything but its own capacity, that is a regression to flag, not a new design question.
- **The sender backend added a third area (`src/smd/cl/sender/`) with its own `CMakeLists.txt`, following the same `add_library(cl.<area> INTERFACE)` + `FILE_SET HEADERS` shape every other `cl` area uses, but it links `cl.eval` rather than `cl.foundation` directly** (`grep -rl cl\.foundation src/smd/cl --include=CMakeLists.txt` finds `foundation`, `symbol`, `reader`, `core`, `elaborator`, `eval` and `conformance` — not `sender`). If the kit extraction changes how `cl.foundation` is declared (e.g. a shared `target_link_libraries` onto a new kit target rather than owning its own headers), the direct linkers need the primary audit, but `cl.sender` still needs checking transitively through `cl.eval` — a link-graph walk, not a plain-text grep, is what actually enumerates every affected target.
- **No `cl` target is in the installed package's `TARGETS` list yet** (`beman_install_library(schemepoc.schemepoc TARGETS ...)` in the root `CMakeLists.txt` lists only `smdscheme.*` and `smd.fixpoint`). If the kit extraction is the first time a `cl.*` target needs installing (because something outside this repo depends on the extracted kit), that is new scope for this step to size, not an oversight R7 left behind — R7 did not need it and did not add it.

## UUID anchors landed by R7

- `src/smd/cl/sender/machine_sender.hpp` `0c10d654-ec97-4580-9c14-aed654701bbf` — `machine_completions` and `machine_operation_state`: D13's channel mapping spent a second time, and the argument for why the machine is driven whole rather than forked.
- `src/smd/cl/sender/machine_sender.hpp` `4087241f-a4fb-4e3f-bc87-59f3973fc026` — `machine_sender` and the `evaluate` factory functions: the sender backend's public surface.
- `src/smd/cl/sender/run_sender.hpp` `bc5a6bf9-8aa1-45f2-bcbf-39c3ff2966b2` — `outcome_receiver` and `run_sender`: the inverse direction, a completion turned back into an `eval::outcome`.
- `src/smd/cl/sender/sender_v.hpp` `2cae776b-38af-41b9-b878-2cca67ac1295` — the whole vocabulary-alias block.
- `src/smd/cl/sender/machine_sender.test.cpp` `84e9ce88-8829-4620-83f2-0b259fca7f3f` — `WhenAllJoinsTwoIndependentPrograms`: the honestly-scoped structural-independence demonstration (two whole programs, not one program's arguments).
