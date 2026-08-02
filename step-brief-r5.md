# Step brief: R5 (one evaluator, three channels)

Forward-only handoff for the next agent.
Read `docs/codestyle.org`, `AGENTS.md`, `docs/CODING_RULES.md`, `CLAUDE.md`, this file, and — last, immediately before writing code — `docs/cpp-rules.md`.
Nothing else, except the anchored sections named under "Dependencies" below.

## Next step's goal

Build the evaluator in `src/smd/cl/eval/`: a CPS backend over `core::core_tree`, on a result type with **three** alternatives — a value, a diagnosed error, and an unwind in flight — per decision D13.
Control flow does not travel in the error channel.
The two sentinel `parse_error` messages compared by pointer identity that the old tree uses as an out-of-band discriminator are not ported.
The orchestrator pastes the plan's R5 phase section (`docs/cl-rebuild-plan.md` §6) alongside this brief; this file does not restate it.

## Merge criterion

`make compile`, `make test`, `make lint`, `make compile-headers` green.
`(defun len (l) (if (null l) 0 (+ 1 (len (cdr l)))))` followed by `(len '(a b c))` evaluates to `3`.
Recursive `defun` works at the end of this phase or the phase is not done — it is DIV-0009, the headline divergence D12 exists to close.
Everything meaningfully constant-evaluable is constexpr with compile-time twins.
Law tests come before substantive tests for any new instance.
UUID anchors are landed around the regions that are this step's centre, and named at the end of the successor brief — phase 28's post pins to this step's merge and transcludes what is anchored there (D20; `AGENTS.md`, "The blog deliverable").
You do not write the post.

## Files this step owns

```txt
src/smd/cl/eval/*.hpp
src/smd/cl/eval/*.test.cpp
src/smd/cl/eval/CMakeLists.txt
src/smd/cl/CMakeLists.txt                (one add_subdirectory line)
src/smd/cl/core/ast.hpp                  (grow leaves/tags as forms demand)
src/smd/cl/core/ast.test.cpp
src/smd/cl/elaborator/elaborate.hpp      (lower the forms the evaluator needs)
src/smd/cl/elaborator/elaborate.test.cpp
checklist.md                             (tick R5)
step-brief-r5.md                         (retire), step-brief-r6.md (write)
```

Do not touch anything else.
`src/smd/smdlisp/**` is the behavioural oracle and is never edited.
`foundation/`, `symbol/` and `reader/` are finished substrate; growing any is a discovered deviation to record in this brief's successor.

## Dependencies already satisfied

- `docs/cl-rebuild-plan.md` §2 — decision records; D13, D17 and D18 bind this step.
- `docs/compiler_architecture.org` § "A sender has three completion channels, and that is a semantic resource".
  **Read this before designing the result type.**
  D13 generalises what that section records, and it already names the two consequences this step must plan for rather than discover: `unwind-protect`'s run-cleanup-on-every-exit-path property has to be *reconstructed* once the channels are split, and restoring a dynamic binding stays in ordinary C++ control flow because it brackets a callee's whole extent rather than a single completion.
- R1–R3 substrate as the R3 brief recorded: `foundation/` (`cl.foundation`), `symbol/` (`cl.symbol`, with value, function and macro slots), `reader/` (`cl.reader`), `core/` (`cl.core`).
- R4 landed `src/smd/cl/elaborator/` (target `cl.elaborator`), entry point `elaborate<MaxNodes, MaxChildren>(datum_tree, symbol_table&) -> result<core_tree>`.

## What R4 found that this step needs and cannot get elsewhere

- **The elaborator is three named schemes, not a recursive descent, and R5 must decide whether evaluation is a fourth.**
  A `tagged_tree` stores its own base functor (`node_f` at `R = int`), so node indices are a topological order: `cata`/`para` *are* the bottom-up catamorphism as one linear pass, and `scan_down` *is* the top-down propagation through the link column.
  `elaborate.hpp` runs atoms (ascending `traverse`), then roles (`scan_down`), then emission (`para_short`, carrier `int`: tree indices in, core indices out).
  No scheme needs a stack, and none bounds nesting depth.
  Before writing the evaluator, answer the scheme question honestly: a fold visits every node in a fixed order, and evaluation visits one arm of an `if`, a body once per call, and nothing past an unwind.
  If the answer is "not a fold", say so in the code and the post, and name the fold it isn't.
- **Four tools, and which job each one actually wants.**
  `traverse` where a structure is threaded through the effect and every element should be visited.
  `cata`/`para` where a value is synthesized bottom-up — `para` exactly when the algebra must read the node as syntax (a name in head position, a binding list), and their `_short` forms where later work must be *skipped*: in R4 that is emission, because the core tree is a shared arena and continuing past a failure spends its capacity on nodes that are already discarded.
  `scan_down` where a value is inherited top-down (a role, a depth, an environment index).
  `foundation::and_then` for a step whose input is the previous step's output, with no structure and no sequence.
  Do not reach for `traverse` where the shape is a bind; that produces decorative machinery, which is worse than the ladder D15 rules out.
- **`foundation::and_then` is new in `foundation/result.hpp`**, with monad-law tests in `result.test.cpp`; the R3 brief pre-authorised this growth.
  `reader::detail::and_then` was deleted and replaced by a `using foundation::and_then;` in `reader::detail`.
  The using-declaration is there for the two calls in `read.hpp`'s public entry points that spell `detail::and_then`; the calls inside the namespace find it by ADL regardless, since `result` is a `foundation` type.
  Note that the two definitions cannot coexist — while both existed, every unqualified call in `reader::detail` was ambiguous.
- **The core AST grew four node kinds**, all inside the baseline scope.
  Leaves: `core_character`; `core_string`, which owns its characters, where `core::max_string_chars` is deliberately *not* shared with the reader's constant and the elaborator diagnoses a literal that does not fit rather than truncating it; and `core_quoted_symbol`, a symbol as data, distinct from `core_variable` because the position picks the namespace, not the atom.
  Tag: `core_cons`.
- **`core_cons` is the one new tag, and D18's own stated exception is what licenses it.**
  Lowering onto an existing form would make `'(1 2)` a call to `cons`, and ANSI quoted data is a literal, so it must not depend on what the function slot of `CONS` holds.
  That is a semantics no existing tag expresses, which is the exception D18 already carves out — not an override of it.
  Reuse that reasoning before adding a tag; do not treat it as precedent for convenience.
- **Elaborated scope is literals, variables, keywords, calls, `if`, `progn`, `quote` in both spellings, and `()` and `(progn)` as `NIL`.**
  Everything else is a *diagnosed* error and never silence: numeric-tower literals (D19 — readable, not yet executable), `#(...)`, `#'f`, and the backquote family, both as expressions and, where they differ, as quoted data.
  R5 grows this list; `defun`, `lambda`, `setq`, `let` and function-namespace access are the obvious next lowerings, and each is a change to `elaborate.hpp` plus a `core_tag`, not a new pipeline stage.
- **`''x` and `'#'f` materialise as the two-element lists `(QUOTE X)` and `(FUNCTION F)`, and the elaborator interns `QUOTE` and `FUNCTION` to do it.**
  ANSI 2.4.6 and 2.4.8.2 fix those representations, and the reader never spelled the operator because `'x` is a branch tag rather than a token.
  That is why `elaborate` takes the symbol table by non-const reference.
  Backquote is deliberately given no representation here: ANSI 2.4.6.1 makes it implementation-defined, so it is the macroexpander's choice, not the elaborator's.
- **The datum tree carries no source positions, so every elaborator-level diagnostic has a default `source_pos`.**
  Reader-level errors keep their real positions, so a failure's position currently signals which *stage* failed and nothing more.
  Fixing this means putting a position or a span on `tagged_tree` nodes, which is reader and foundation work — out of scope here, worth scheduling, and worth knowing before writing an evaluator whose diagnostics would inherit the same gap.
- **Capacity is diagnosed, never asserted.**
  `add_leaf_checked`, `add_branch_checked` and `intern_checked` surface a full tree, symbol table or name pool as a `parse_error`.
  Keep that discipline in the evaluator: an assert is not an acceptable answer to a program that is merely too big.
- **`elaborator::detail::intern_checked` duplicates `reader::detail::intern_checked`.**
  Two copies is the signal the R1 brief used for `foundation/`, so a third means promoting it.
  R5 needs interning too, for `gensym` and for `defun`'s function slot, so decide there rather than growing a fourth copy.
- **Flag for the next docs-owning step**, none of which R4 edited, per its own file list.
  Still pending from R3: a toolchain divergence record for the GCC trunk r16-8246 memchr misfold, still worked around in `reader/number.hpp`.
  Still pending from R2: the plan §8 answer that the symbol table survives into the runtime program, and the DIV-0013 dated note that it still reproduces at r16-8246.
  New from R4: `core_cons` as a deliberate D18 exception, and elaborator diagnostics carrying no source position.
- **The informal SBCL differential check is still pending.**
  No CL implementation is installed in this environment, so R3's new reader syntax and R4's elaboration decisions were derived from the specification and pinned by tests.
  Run the cross-check when sbcl arrives (plan §4, tier 3), and prefer it over the old tree wherever the old tree pins a `defect`.
