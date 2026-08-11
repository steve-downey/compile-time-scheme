# Step brief: R7 (sender backend)

Forward-only handoff for the next agent.
Read `docs/codestyle.org`, `AGENTS.md`, `docs/CODING_RULES.md`, `CLAUDE.md`, this file, and — last, immediately before writing code — `docs/cpp-rules.md`.
Nothing else, except the anchored sections named under "Dependencies" below.

## Next step's goal

Build the sender backend of decision D17: a second backend over `src/smd/cl`'s core tree, using Beman Execution, alongside the `eval::machine` step-based abstract machine R5 built and R6's conformance corpus now exercises.
The orchestrator pastes the plan's R7 phase section (`docs/cl-rebuild-plan.md` §7) alongside this brief; this file does not restate it.

**The sender backend's shape is a choice, not an inheritance — open the step by making it, not by porting the pivot's design.**

Rule out the obvious wrong answer first. Porting the pivot's Mendler sender interpreter (`src/smd/smdlisp/sender/sender_eval.hpp`, and the Scheme `sender_mendler_eval.hpp` behind it) is not an option, and treating it as a starting point will get it built by default rather than by decision.
It needs a pointer-shaped tree — `Fix`, `Box`, the `Comp` tree of `smdscheme/sender/comp_tree.hpp` — which is a second full spelling of every core node kind beside the columnar one (DIV-0016 priced this at "a step's worth of work on its own and buys nothing"), and `Comp` puts capacities inside types (`comp_lambda<A, MaxList>`, `env<CompT, 16>`), which is the D14 violation the rebuild exists to remove.
`src/smd/cl/foundation/` has no Mendler scheme and needs none: `para` already carries the node itself, and over a materialized column there is no descent for a recurse-knob to choose (DIV-0016, 2026-08-01 note).

- **Shape one: drive the machine.**
  A sender or scheduler loop around `machine::step` — `foundation::trampoline` generalised, which is the question R5 left it holding.
  Keeps `limits::steps` and `limits::frames` exactly as R5 diagnosed them, and reuses `eval::outcome` unchanged.
- **Shape two: the machine with a forking argument frame.**
  `k_arguments` walks `branch.children` left to right; the independent set is *already in hand* at `machine::evaluate_subforms`, so exposing it to `when_all` is a change to one frame's scheduling, not a change of representation or of scheme.
  This is what the pivot's `when_all` demonstration bought, without the pointer tree.
- **The standing prediction, and why it has expired.**
  `docs/fixpoint-mendler-reference.md` § "Applicative Parallelism Preserved" predicts that a CPS trampoline linearizes the argument structure `Fix<CompF>` preserves, and R5's evaluator is that trampoline.
  That claim was about the pointer tree, where independence is only visible to whoever holds the sub-`Box`es; on a columnar tree it inverts, since the whole child list is one `static_vector` on the branch.
  Record this when you land the shape decision — a dated note on DIV-0016, or a correction at the head of the reference doc, which currently reads as live guidance for a foundation that no longer exists.
- **What favours senders regardless of shape.**
  D13's `outcome` maps one-to-one onto the completion channels — value/error/unwind against `set_value`/`set_error`/`set_stopped` (`docs/compiler_architecture.org` § "D13's three channels are now a type, not a discriminator convention"), which is the observation that produced D13 in the first place.
- **Do not reuse R5's depth argument here.**
  Phase 28 justifies the machine partly because each recursion would be a C++ activation record under `-fconstexpr-depth`.
  That argument does not transfer: DIV-0015 is accepted-permanent, `connect`/`start`/`sync_wait` are not constant-evaluable, so this backend has no compile-time evidence to protect.
  What survives is ordinary runtime stack depth, which is a weaker reason and should be stated as the weaker one.
- **Scope any `when_all` demonstration honestly.**
  Left-to-right argument evaluation is the conforming order (ANSI 3.1.2.1.2.3) as soon as an argument can have effects, and D19 aims at full ANSI.
  Whatever R7 shows must be scoped to the effect-free fragment or presented as visible graph structure, never as a claim about evaluation order.

## Merge criterion

`make compile`, `make test-matrix`, `make lint`, `make compile-headers` green, and `scripts/verify-transclusions.sh` green.
Report which `test-matrix` legs you ran (Debug -O0 and Asan -O3 both witness real defects here; do not settle for `make test` alone).
DIV-0015 (no constexpr twin for a sender backend) is accepted-permanent — do not attempt to force one; a runtime `TEST_CASE` is the whole of this step's verification for the sender path itself.
UUID anchors landed around this step's centre and named at the end of the successor brief — phase 31's post pins to this step's merge (D20; `AGENTS.md`, "The blog deliverable"). You do not write the post.

## Files this step owns

```txt
src/smd/cl/sender/*                 (new area, with its own CMakeLists.txt)
src/smd/cl/CMakeLists.txt           (one add_subdirectory line)
src/smd/cl/eval/*                   (only if the chosen shape needs a seam exposed, e.g. making evaluate_subforms's independent set visible)
docs/divergences/*                  (append a dated note; never edit in place)
checklist.md                        (tick R7)
step-brief-r7.md                    (retire), step-brief-r8.md (write)
```

`src/smd/smdlisp/**` is the behavioural oracle and is never edited. `src/smd/cl/conformance/**` is R6's; do not extend the corpus in this step unless the sender backend finds a genuine gap the corpus should have caught — record such a gap in a divergence note rather than silently growing the corpus.

## Dependencies already satisfied

- `docs/compiler_architecture.org` § "D13's three channels are now a type, not a discriminator convention" and § "The evaluator is a machine, not a fold" — R5's `eval::outcome` and `eval::machine`, which this step drives or wraps rather than replaces.
- R1–R4 substrate: `foundation/` (`cl.foundation`), `symbol/` (`cl.symbol`), `reader/` (`cl.reader`), `core/` (`cl.core`), `elaborator/` (`cl.elaborator`).
- R5: `src/smd/cl/eval/` (target `cl.eval`) — `machine`, `outcome`, `value`, `heap`, `builtin`, `environment`, `foundation/trampoline.hpp`.
- R6: `src/smd/cl/conformance/` (target `cl.conformance`) — the one-shot driver (`evaluate_program`, `read_and_elaborate_all`) and the corpus (`ansi_test_adapted`, `spec_derived`), both usable as-is for any differential or regression check this step wants; the guarded-subprocess pattern in `sbcl_oracle.hpp` if this step ever needs to shell out to anything (it should not need to).

## What R6 found that this step needs and cannot get elsewhere

- **The driver is a reusable component now, not a test-local helper.** `smd::cl::conformance::evaluate_program` (`src/smd/cl/conformance/driver.hpp`) runs `install_builtins` → read every top-level form → elaborate into one arena → wrap in `progn` if more than one → `machine::run()`, returning a `program_report{outcome, standard_symbols, witness_defined}`. If this step wants a quick differential check between the machine-driven path and whatever the sender backend produces for the same source, this is the machine-side half already built; write the sender-side counterpart next to it, or in `src/smd/cl/sender/`, not a fourth copy of the chain.
- **The corpus is fixnum/symbol/error/unwind only, deliberately.** `src/smd/cl/conformance/corpus.hpp`'s 75 entries never exercise the numeric tower, strings, or characters (D10), and never pin a `defect` (`docs/divergences/README.md`). If this step's `when_all` demonstration needs example programs, the corpus's `spec_derived` table is a source of already-verified, already-in-scope forms — reuse rather than inventing parallel examples with unverified expected outcomes.
- **The reader has no dotted-pair syntax.** `(a . b)` reads as the three-element list `(a |.| b)`, not a pair (DIV-0022, new this step, `scope-decision` under the pivot's D6). This does not affect the sender backend directly — nothing in the core tree cares how a pair's syntax was spelled — but it means any sender-backend example built from source text (rather than from `cons` calls) cannot use consing-dot notation either.
- **`program_report` does not expose the symbol table**, by design (a `program_report` outlives the local `symbol_table` its `evaluate_program` call built). If this step's differential check needs to compare *names* rather than structural value shape, it will need its own access to a symbol table, not an extension to `program_report` — extending it defeats the reason it does not already expose one (see `driver.hpp`'s own comment on `witness_defined` for the shape of probe this project reaches for instead: ask one narrow, named question, not "give me the table").
- **The differential-oracle pattern (guarded skip on a missing external tool) is now precedented.** `src/smd/cl/conformance/sbcl_differential.test.cpp` probes for `sbcl` at runtime and calls Catch2's `SKIP(...)` rather than failing when it is absent. If this step ever needs an external tool gate (it should not — Beman Execution is a vendored submodule, not a runtime probe), this is the house shape to copy rather than reinventing a build-time `find_package`-based gate.

## UUID anchors landed by R6

- `src/smd/cl/conformance/driver.hpp` `74829ac7-6ad9-4158-b990-7c59ffa5b40b` — `read_and_elaborate_all`: the sequence unfold, reading any number of top-level forms into one arena.
- `src/smd/cl/conformance/driver.hpp` `e0cc2579-ecbe-41f7-973e-5649f0ba5bc6` — `evaluate_program`: the one-shot install/read/elaborate/run chain, generalized from `machine.test.cpp`'s local helpers.
- `src/smd/cl/conformance/corpus.hpp` `6bafb7e1-11a8-48bd-b0b2-341a546df04d` — `ansi_test_adapted`: the corpus shape (provenance-tagged entries) and the `ansi-test`-derived table itself.
- `src/smd/cl/conformance/sbcl_oracle.hpp` `29a14fe5-7268-446e-9489-7ec8df52494c` — `find_sbcl_version`: the pinned-oracle-version probe.
- `src/smd/cl/conformance/sbcl_oracle.hpp` `1e9c6a05-3f50-4a61-8552-e879e2053d2d` — `sbcl_prin1`: the differential comparison's one subprocess call.
