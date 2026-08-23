# SchemePoC checklist

## Ground rules

- [x] `docs/codestyle.org` copied into repo.
- [x] `docs/CODING_RULES.md` copied into repo.
- [x] `AGENTS.md` created.
- [x] C++26 baseline recorded.
- [x] GCC16 baseline recorded.
- [x] Catch2 test framework recorded.
- [x] No GTest references remain.
- [x] Beman Execution policy recorded.
- [x] Beman dependency submodule policy recorded.
- [x] Every file has canonical path comment and Emacs mode line.
- [x] Every source-like file has SPDX header.
- [x] Header guards use project convention.
- [x] Includes use canonical angle-bracket spelling.
- [x] No relative project includes.
- [x] No `using namespace` in headers.
- [x] Component header is included first and twice in tests.
- [x] Every public constexpr API has a constexpr/static_assert test.
- [x] CMake uses targets and file sets.
- [x] Local CMakeLists only list local files.
- [x] `make compile` passes.
- [x] `make test` passes.
- [x] `make lint` passes.

## Steps

- [x] Step -1: add agent/style governance files
- [x] Step 0: repository recustomization and skeleton
- [x] Step 1: core utility vocabulary
- [x] Step 2: input cursor and lexical primitives
- [x] Step 3: minimal parser object
- [x] Step 4: functor and applicative combinators
- [x] Step 5: alternative, repetition, and lexeme
- [x] Step 6: reader atom model
- [x] Step 7: fixed-capacity datum tree
- [x] Step 8: datum reader for lists and quote
- [x] Step 9: core language model
- [x] Step 10: datum-to-core elaborator for literals, variables, calls
- [x] Step 11: direct evaluator for core arithmetic
- [x] Step 12: elaborate `if`
- [x] Step 13: typeclass-object facade for parser operations
- [x] Step 14: generic fixpoint playground
- [x] Step 15: CPS closure backend facade
- [x] Step 16: bottom-up CPS direction decision
- [x] Step 17: defunctionalized CPS program
- [x] Step 18: closure materialization over CPS program
- [x] Step 19: public one-shot API
- [x] Step 20: lambda syntax
- [x] Step 21: runtime closure values
- [x] Step 22: function application
- [x] Step 23: lexical closure capture
- [x] Step 24: quote elaboration
- [x] Step 25: error quality pass
- [x] Step 26: negative compile tests
- [x] Step 27: Godbolt single-file extraction
- [x] Step 28: C++ Foreign Function Interface (FFI) spike
- [x] Step 29: vendor Beman Execution
- [x] Step 30: sender adapter over Beman Execution
- [x] Step 31: sender backend over CPS program using Beman Execution
- [x] Step 32: optional Beman Task integration, only if needed
- [x] Step 33: reflection spike
- [x] Step 34: documentation consolidation

## Common Lisp pivot (docs/cl-pivot-plan.md)

- [x] Step L0: governance install
- [x] Step L1: smdlisp skeleton
- [x] Step L2: blog phase 15 — why not call/cc
- [x] Step L3: blog deps infra fix (optional)
- [x] Step L4: CL lexical layer
- [x] Step L5: CL atoms
- [x] Step L6: CL datum reader (+ phase 16 draft)
- [x] Step L7: CL value model
- [x] Step L8: cons cells and list builtins (adapt landed pairs work)
- [x] Step L9: Lisp-2 environment
- [x] Step L10: CL core model and baseline elaborator
- [x] Step L11: direct evaluator (+ phase 17 draft)
- [x] Step L12: setq, defun, defvar, defparameter (adapt landed set! work)
- [x] Step L13: CPS closure backend (+ phase 18 draft)
- [x] Step L14: block / return-from
- [x] Step L15: catch / throw / unwind-protect (+ phase 19 draft)
- [x] Step L16: special variables and dynamic binding
- [x] Step L17: macro expander with host macros
- [x] Step L18: backquote
- [x] Step L19: defmacro (+ phase 20 draft)
- [x] Step L20: multiple values
- [x] Step L21: sender backend for the CL core (+ phase 21 draft)
- [x] Step L22: public API, Godbolt, FFI parity
- [x] Step L23: tagbody / go (optional, D8)
- [x] Step L24: documentation consolidation (+ phase 22 draft)

## Common Lisp rebuild (docs/cl-rebuild-plan.md)

Supersedes the pivot's decision set with D11–D17.
`src/smd/smdlisp/**` is frozen as a behavioural oracle from R1 onward and is never edited — decision D32 (`docs/cl-language-scoping.md`) retires that rule at step A3 of the plan added below, but the rule holds until then.

Every phase ships a blog post as a deliverable (D20), drafted by an agent that did not do the work and reviewed by a clean agent running the `voice` skill.
R0–R4 landed before that was recorded; `docs/history/blog-backfill-plan.md` covers the arrears as steps B5–B11.

- [x] Step R0: decisions, divergence classification, plan
- [x] Step R1: substrate — `src/smd/cl/foundation`, short-circuiting fold, topological folds, typeclass instances, law tests, the test matrix
- [x] Step R2: interned symbol table with value/function/macro slots (D12) — `src/smd/cl/symbol`
- [x] Step R3: reader and core AST — the tree as its own base functor's fixed point, with instances and schemes — `src/smd/cl/reader`, `src/smd/cl/core`, `foundation/tagged_tree`
- [x] Step R4: elaborator as three schemes (D15) — `traverse` atoms, `scan_down` roles, `para_short` emission — `src/smd/cl/elaborator`, `foundation::and_then`
- [x] Step R5: one evaluator, three channels (D13) — recursive `defun` is the acceptance witness — `src/smd/cl/eval`, `foundation/trampoline`, `symbol::intern_checked` (+ phase 28)
- [x] No step: the two invariants of a `tagged_tree`, and `from_nodes`' unchecked precondition — a reconsideration of phase 28's Mendler argument, `is_children_before_parent` and three asserts (+ phase 29)
- [x] Step R6: conformance corpus and differential oracle (D16) (brief: `step-brief-r6.md`) (+ phase 30)
- [x] Step R7: sender backend (D17) (+ phase 31)
- [x] Step R8: extract the kit — `smd::kit::foundation` (R8 moved seventeen `foundation/` files; eighteen forwarding shims now, since `monad.hpp` landed; `parser/` has no `cl` client, DIV-0028) (+ phase 32)

## Blog backfill (docs/history/blog-backfill-plan.md)

Phases 23–27, for steps R0–R4, which landed without posts.
B6–B10 are independent of each other and depend only on B5.

- [x] Step B5: tags `blog/phase-24`–`blog/phase-27` placed on the four step merges, whose anchors landed with their steps
- [x] Step B6: blog phase 23 — R0, why rebuild rather than refactor (no pin, no transclusions)
- [x] Step B7: blog phase 24 — R1, the substrate
- [x] Step B8: blog phase 25 — R2, interned symbols
- [x] Step B9: blog phase 26 — R3, reader and core AST
- [x] Step B10: blog phase 27 — R4, elaboration as three schemes
- [x] Step B11: `docs/blog/index.org` entries, `docs/blog/pins.md` rows and third-era note, `make blog-md` and transclusion verification green

## Plan reconciliation and reader combinators (tmp/plan/)

Executed as a `tmp/plan/` fan-out rather than hand-driven, per `tmp/plan/checklist.md`.
Phase 0 (A0) merges to `main` directly.
Phase A (A1–A5) merges to integration branch `cl-retire-trees`, created off `main` after A0.
Phase B (B1–B8) merges to integration branch `cl-parser-combinators`, created off `main` after Phase A's gate.

- [x] Step A0: rebase the record against reality — the root checklist, decision D32, three backlog items, and a dated amendment to each scoping note
- [ ] Step A1: freeze both iterations as tags; move their architecture prose to a pinned history doc
- [ ] Step A2: neutralise the dead trees' build consumers (examples, install test, export list)
- [ ] Step A3: delete `smdscheme` and `smdlisp` from trunk, executing D32
- [ ] Step A4: a `prin1`-shaped printer for `cl` datums, proved against SBCL in the same step
- [ ] Step A5: broaden the SBCL reader differential into a real corpus
- [ ] Step B1: split `read.hpp` into component headers plus an umbrella (mechanical, zero behavioural diff)
- [ ] Step B2: `smd::kit::parser`: a Monad instance over a context-threaded parser, with `read_radix_number` as its first client
- [ ] Step B3: intertoken space and comments onto the layer
- [ ] Step B4: choice and bounded repetition; `read_delimited` onto the combinator layer
- [ ] Step B5: text: strings and character literals
- [ ] Step B6: forms: the quote family and token data
- [ ] Step B7: sharpsign dispatch and `read_node`
- [ ] Step B8: integration: the D30 measurement, DIV-0028 dissolved, architecture section, project checklist
