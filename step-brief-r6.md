# Step brief: R6 (conformance corpus and differential oracle)

Forward-only handoff for the next agent.
Read `docs/codestyle.org`, `AGENTS.md`, `docs/CODING_RULES.md`, `CLAUDE.md`, this file, and — last, immediately before writing code — `docs/cpp-rules.md`.
Nothing else, except the anchored sections named under "Dependencies" below.

## Next step's goal

Build the conformance corpus and the differential oracle of decision D16: one place where a program's spelling, its expected outcome, and the classification of any divergence live together, run against `src/smd/cl/eval`.
The orchestrator pastes the plan's R6 phase section (`docs/cl-rebuild-plan.md` §6) alongside this brief; this file does not restate it.

D16's rule is the sharp edge: **a test may pin a `scope-decision`; a test must never pin a `defect`.**
The old tree (`src/smd/smdlisp/**`) is a behavioural oracle, not a correctness oracle — where `docs/divergences/README.md` classifies its behaviour as a `defect`, it is a *wrong* answer on purpose and must not be copied.

## Merge criterion

`make compile`, `make test`, `make lint`, `make compile-headers` green, and `scripts/verify-transclusions.sh` green.
Every corpus entry states its expected outcome by channel — value, diagnosed error, or unwind — because the evaluator has three and collapsing them in the corpus would give back exactly what R5 spent the phase separating.
Everything meaningfully constant-evaluable is `constexpr` with compile-time twins.
Law tests come before substantive tests for any new instance.
UUID anchors landed around this step's centre and named at the end of the successor brief — phase 29's post pins to this step's merge (D20; `AGENTS.md`, "The blog deliverable").
You do not write the post.

## Files this step owns

```txt
src/smd/cl/conformance/*            (new area, with its own CMakeLists.txt)
src/smd/cl/CMakeLists.txt           (one add_subdirectory line)
src/smd/cl/eval/*                   (as the corpus finds gaps)
src/smd/cl/elaborator/elaborate.*   (as the corpus finds gaps)
docs/divergences/*                  (append a dated note; never edit in place)
checklist.md                        (tick R6)
step-brief-r6.md                    (retire), step-brief-r7.md (write)
```

`src/smd/smdlisp/**` is the behavioural oracle and is never edited.

## Dependencies already satisfied

- `docs/compiler_architecture.org` § "A sender has three completion channels, and that is a semantic resource" — R5 generalised what that section records; R7's sender backend spends it a third time.
- R1–R4 substrate: `foundation/` (`cl.foundation`), `symbol/` (`cl.symbol`), `reader/` (`cl.reader`), `core/` (`cl.core`), `elaborator/` (`cl.elaborator`).
- R5 landed `src/smd/cl/eval/` (target `cl.eval`), `foundation/trampoline.hpp`, and `symbol::intern_checked`.

## What R5 found that this step needs and cannot get elsewhere

- **Driving a program is four steps, and there is no one-shot entry point yet.**
  `install_builtins(symbols)` → `reader::read(source, symbols)` → `elaborator::elaborate(form, symbols)` → `eval::machine{program, symbols, known}.run()`.
  `install_builtins` must run **first**: it interns `NIL` and `T`, which the elaborator asks the table for when deciding that a bare `nil` is a constant rather than a variable, and it fills the function slots a call resolves through.
  `src/smd/cl/eval/machine.test.cpp` has this chained through `foundation::and_then` and returning a small `report`; a corpus wants it as a real component, not copied a third time.
  That component is R6's natural first file.
- **A program of several top-level forms is one core arena, not several.**
  `elaborator::elaborate_into(form, symbols, out)` is new and public: it emits into an arena that may already hold earlier forms and *returns* the root rather than setting it.
  This exists because a closure carries the index of the `core_lambda` node its body lives at, so a function defined by one form and called by a later one needs both forms' nodes in one tree that outlives both.
  Reading a *sequence* of top-level forms is still not in the library — `reader::read_datum` returns the cursor after the datum, so the unfold is available but unwritten.
  Write it once, in the corpus driver, rather than in each test.
- **The five new `core_tag` alternatives all survived D18's bar, and the reasoning is worth reusing rather than reopening.**
  `core_defun` — "put this function in that symbol's function slot" is what D12's slot is for and no other form says it.
  `core_lambda` — parameter binding; kept separate from `core_defun` so that `let` and `multiple-value-bind` can later lower to a lambda application, which is the pivot's proven strategy (DIV-0020) and D18's own stated mitigation.
  `core_block` and `core_return_from` — the only producer of the unwind channel; without them the third channel has no source and D13 is untestable.
  `core_unwind_protect` — the one form whose meaning is stated in terms of all three channels at once.
  Apply the same bar before adding a sixth.
- **`lambda` is not yet an expression, and `value` has no function alternative.**
  Nothing in the R5 object language yields a function object: `defun` names one, a call resolves one through a slot, and `#'f`, `funcall`, `apply` and `lambda`-as-expression are all still diagnosed.
  Adding the value alternative is one step's work and belongs with whichever step adds a producer; do not add a channel with no producer first.
  Note the current diagnostic for `(lambda (x) x)` in expression position is "undefined function", because `LAMBDA` falls through to an ordinary call — poor, and worth fixing when `lambda` lands.
- **Entering a function costs no continuation frame, but the implicit `block` does.**
  A `defun` body sits inside a `core_block` named for the function (ANSI 3.1.2.1.2.2), and that block's frame cannot be popped until the body finishes, so a self-call in tail position costs one frame per call rather than none.
  Recursion depth is therefore a diagnosed capacity (`limits::frames`), not the C++ stack.
  Omitting the block when a body contains no `return-from` is a real optimisation and is *not* done.
- **A budget is part of the contract, not a safety net.**
  `foundation::trampoline` takes a step budget and diagnoses exhaustion, because a non-terminating object program must not become a non-terminating *translation* — an unbounded loop under constant evaluation is a compiler hang, and a corpus is exactly where one will first be written by accident.
  `limits::steps` and `limits::frames` both surface as ordinary diagnosed errors.
- **`intern_checked` was promoted to `symbol/`, and all three copies collapsed.**
  It is now `symbol::intern_checked(table, name, where = {})` in `symbol/symbol_table.hpp`, a free function beside the table because only the table knows what "full" means.
  The reader's and the elaborator's private copies are gone, replaced by `using symbol::intern_checked;`.
  Note that the copies could not have coexisted: a symbol table's own type puts `smd::cl::symbol` among the associated namespaces, so every unqualified call was ambiguous the moment the second definition existed — the same collision R4 hit promoting `and_then`.
- **`foundation/` grew two things, both minimal and both justified.**
  `static_vector::pop_back`, because the evaluator's continuation stack is the first client that shrinks; and `foundation/trampoline.hpp`, the driver loop for a small-step machine, put in the substrate so no evaluator writes a raw loop of its own.
  A second backend (R7) is the test of whether `trampoline` was the right shape.
- **Diagnostics still carry no source position, and it is now worse than it was.**
  R4 recorded that every elaborator-level diagnostic has a default `source_pos` because the datum tree carries no positions.
  Every *evaluator* diagnostic inherits that, so "unbound variable" and "wrong number of arguments" arrive positionless.
  A conformance corpus is where that hurts first. Fixing it means putting a position or a span on `tagged_tree` nodes, which is reader and foundation work.
- **`(block nil ...)` is diagnosed, not accepted.**
  The atom pass (`traverse`) turns `NIL` and `T` into constants before the role scan (`scan_down`) runs, so neither is available as a block name. ANSI uses `(block nil ...)` for `loop`'s implicit block, so this will need answering before iteration macros land. Classify it before pinning it.
- **The informal SBCL differential check is still pending.**
  No CL implementation is installed here, so R3's reader syntax, R4's elaboration and R5's evaluation semantics were all derived from the specification and pinned by tests. Run the cross-check when sbcl arrives (plan §4, tier 3).

## Flag for the next docs-owning step

None of these were edited by R5, per its own file list.

- Still pending from R3: a toolchain divergence record for the GCC trunk r16-8246 memchr misfold, still worked around in `reader/number.hpp`.
- Still pending from R2: the plan §8 answer that the symbol table survives into the runtime program, and the DIV-0013 dated note that it still reproduces at r16-8246.
- Still pending from R4: `core_cons` as a deliberate D18 exception, and elaborator diagnostics carrying no source position.
- New from R5, for `docs/compiler_architecture.org`: that D13's three-channel outcome is now a *type*, `eval::outcome`, rather than a discriminator convention, and that the pivot's sentinel `parse_error` messages compared by pointer identity are not ported; that `unwind-protect`'s cleanup-on-every-path property is reconstructed by a continuation frame (`eval::detail::k_protect`) rather than by a merged exit path; and that the evaluator is a defunctionalised-CPS abstract machine over the same core tree the elaborator's `para_short` emits — a machine and not a fold, because evaluation visits one arm of an `if` and nothing past an unwind, so evaluation depth is a diagnosed capacity rather than C++ stack depth.
- New from R5, for `docs/divergences/`: `(block nil ...)` rejected; lambda list keywords (`&optional` and friends) diagnosed rather than bound; `lambda`, `#'f`, `funcall` and `apply` still not executable; `eq` and `eql` still coincide because every number that exists is an immediate.

## UUID anchors landed by R5

- `src/smd/cl/eval/outcome.hpp` `8f5b0a1c-3f0e-4f6f-95b0-2a0b4a5f6c31` — `unwind` and `outcome`: D13's three channels as one type.
- `src/smd/cl/eval/value.hpp` `5d5f74e8-b4a3-4d0e-9b1f-0f0e3e7f7a11` — the runtime value model, capacity-free (D14).
- `src/smd/cl/eval/heap.hpp` `6a2fbb0b-6b7b-4b0e-9cb9-1a25f9f6a3d2` — the `heap` class: where every capacity lives.
- `src/smd/cl/eval/heap.hpp` `b5e6c9e7-7f4e-4a52-8a3a-2f1d0c7a4b90` — `list_view`, the only way the evaluator walks a chain.
- `src/smd/cl/eval/environment.hpp` `8a26a939-6938-4e23-a016-5328e9288d5b` — `lookup` and `extend`: the environment is an ordinary Lisp value.
- `src/smd/cl/eval/builtin.hpp` `9c1c6a0d-3a34-4b0f-9b6a-0a4a9f2d5e10` — the builtin name/operation table.
- `src/smd/cl/eval/builtin.hpp` `feb024b9-0150-4cda-a395-00cdf2d2474c` — `install_builtins`: a builtin is an ordinary function slot.
- `src/smd/cl/eval/builtin.hpp` `1f9a4b57-2c1d-4a75-9a17-6b8b4b0f2a63` — `apply_builtin`, whose third channel stays empty.
- `src/smd/cl/eval/machine.hpp` `0b7a5e42-9d3c-4b19-83f4-6e5c2a1b7d04` — the `machine` class and why it is a machine and not a fold.
- `src/smd/cl/eval/machine.hpp` `2e6f8b3a-51d7-4a0c-9f6e-7c2b1d0a4e58` — `k_protect`, where cleanup-on-all-three-channels is reconstructed.
- `src/smd/cl/eval/machine.hpp` `7fb628ca-8a1d-4af4-ac88-3a05726e237f` — `call_function` and `enter_closure`: the slot lookup that closes DIV-0009.
- `src/smd/cl/foundation/trampoline.hpp` `4b3c37ec-1a54-4b6b-a10c-4d20ff2e3427` — the step budget as a contract.
- `src/smd/cl/core/ast.hpp` `7d0ac0a1-9c4e-4f2b-8a2e-3f4b6d1e5c07` — the five control tags and the `core_tag` variant.
- `src/smd/cl/elaborator/elaborate.hpp` `20b081ea-5806-495c-8476-d89fee98acdc` — `child_roles`: which children of a form are names rather than subforms.
- `src/smd/cl/elaborator/elaborate.hpp` `d398d3f2-8862-4663-9e03-28262372feae` — `emit_definition`: defun over lambda over block.
- `src/smd/cl/elaborator/elaborate.hpp` `53eabd7c-d0c2-4f0e-a63b-3260a0151044` — `elaborate_into` and `elaborate`.
- `src/smd/cl/symbol/symbol_table.hpp` `c589354c-b5fe-4109-9eb9-31c9929ae887` — `intern_checked`, promoted from two private copies.
