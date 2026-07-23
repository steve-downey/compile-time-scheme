# Common Lisp Pivot — Agent Execution Plan

This is the operational plan for pivoting the compile-time compiler from Scheme-light semantics to Common Lisp-light semantics.

The new component is `smd/smdlisp`, a compile-time Common Lisp-light compiler in C++26 on GCC16, built alongside and reusing the existing `smd/smdscheme` infrastructure.

This plan is written for an orchestrating agent that dispatches sub-agents, observes and checks their results, and maintains the handoff and divergence documentation described below.

---

# 0. Why the pivot

The Scheme road ends at `call/cc`, and `call/cc` is where the project's two goals collide.

The sender backend is built on the sender/receiver model of Beman Execution.
In that model an operation state completes exactly once, through exactly one of `set_value`, `set_error`, or `set_stopped`.
Completion is one-shot by contract.

Multishot `call/cc` reifies the current continuation as a first-class value that may be invoked zero, one, or many times, including after its capturing extent has already returned.
Expressing that over senders would require re-running or checkpointing completed operation states, which the sender contract forbids.
A sender backend can express at most a one-shot, upward-only escape.

Independently, full `call/cc` is unsound in the practical sense even inside Scheme:

- It interacts incoherently with `dynamic-wind` and resource cleanup; re-entering a captured continuation re-runs entry thunks against already-released resources, and the RAII / `unwind-protect` correspondence breaks.
- It makes every function call a potential re-entry point, destroying local reasoning about linear resource use.
- These arguments are collected in Oleg Kiselyov's "An argument against call/cc"; the project adopts that position.

Common Lisp deliberately has no `call/cc`.
Its nonlocal control operators — `block`/`return-from`, `catch`/`throw`, `tagbody`/`go`, `unwind-protect` — all have **dynamic extent**: once an exit point's extent ends, using it is undefined.
Every exit is one-shot and upward-only.

That is exactly the discipline the sender model enforces.
The thesis of the pivot: **Common Lisp's control operators are the largest control vocabulary a structured-concurrency backend can express soundly, and this project will demonstrate that correspondence.**

## What the pivot is not

- It is **not** a rewrite of the existing blog (phases 0–14) or documentation of building the Scheme front half.
  That history stands as written, including phase 14 (`set!`, `begin`, and a Store), the last Scheme-semantics post.
- It is **not** a deletion of `smd/smdscheme`.
  The Scheme pipeline remains buildable, tested, and demoable; blog phases 5–12 transclude live code from it via UUID anchors, so its semantics must not silently change.
- The new documentation stream first documents *the switch* from Scheme to CL semantics (the rationale above), then documents building out the rest of the language.

---

# 1. Rule and style precedence

> **This is an orchestrator/design document. Step worker agents do NOT read it.**
> It carries the dependency DAG and per-step specifications the orchestrator draws
> from when dispatching a worker; the worker's own runtime reading contract lives in
> `AGENTS.md`. (`docs/schemepoc-plan.md` is the superseded pre-pivot plan, historical
> only; `docs/history/handoff-archive.md` is the retired cumulative handoff log.)

A worker agent reads a bounded, three-tier set — nothing that grows per step:

```txt
Tier 1 — always (rules pack): docs/codestyle.org, AGENTS.md, docs/CODING_RULES.md, CLAUDE.md
Tier 2 — this step only:      your lane's step-brief-<lane>.md, checklist.md
Tier 3 — on demand, by anchor/section only, never wholesale:
         docs/compiler_architecture.org, this plan (orchestrator DAG), git log
```

Rule precedence is unchanged:

```txt
docs/codestyle.org > AGENTS.md > docs/CODING_RULES.md > CLAUDE.md > handoff/checklist files
```

This plan adds task-level direction; it does not override style rules.
C++26 and GCC16 are the baseline.
Tests use Catch2.
Do not introduce GTest.

---

# 2. Orchestration protocol

The plan is executed by an **orchestrator** agent managing **worker** sub-agents.

## Orchestrator duties

- Dispatch one worker per step, giving it the canonical clean-agent instruction (section 12) plus the step section from this plan.
- Dispatch **independent steps in parallel** when the dependency notes for the steps permit it; each parallel worker gets its own git worktree.
- After each worker reports completion, **independently verify**, do not trust the report:
  - Run `make compile`, `make test`, `make lint` in the worker's tree.
  - Review the diff; changes must be confined to the files the step declares plus its tests and CMake.
  - Confirm the frozen-tree rule: no semantic edits inside `src/smd/smdscheme/**` UUID anchor blocks (check with `git diff -- src/smd/smdscheme` — anchor-block edits require a divergence doc and orchestrator sign-off).
  - Confirm `checklist.md` was ticked, durable cross-step facts recorded in `docs/compiler_architecture.org` (in place, by anchor), and the lane's step brief rewritten per the step-brief contract (forward-only, bounded, no log).
  - Confirm any deviation from this plan or from ANSI CL semantics has a divergence doc (section 3).
- Merge to main with `--no-ff` only when all checks pass.
- **Branch policy:** all compiler work happens on feature branches, never directly on main.
  Main is always a complete set of work: a branch merges only when it finishes a feature goal (a step, or a coherent group of steps for one feature).
  Formatting-only and lint-only fixes are the exception: commit them by themselves, directly on main, before starting feature work, so feature diffs stay semantic.
- If a worker is blocked, capture the blocker in the lane's step brief and either re-scope the step or file a divergence doc and move on.

## Worker duties

- Work only the assigned step.
- Keep the step small and mergeable; do not continue into later steps.
- Do not leave vague TODOs.
- Run `make compile`, `make test`, `make lint` before declaring completion.
- Update `checklist.md`; record durable cross-step facts in `docs/compiler_architecture.org` (in place, by anchor); rewrite the lane's step brief per the step-brief contract in `AGENTS.md`.
- File divergence docs for anything done differently than this plan specifies, and for any knowing deviation from ANSI CL semantics.

---

# 3. Divergence issue docs

Divergence docs live in `docs/divergences/`, one file per issue, named `DIV-NNNN-short-slug.md`, numbered sequentially.
Use `docs/divergences/TEMPLATE.md` as the skeleton.

File a divergence doc when:

1. The implementation knowingly deviates from ANSI Common Lisp semantics (scope cuts, simplifications).
2. A step is implemented differently than this plan specifies (the plan sketch didn't survive contact with the compiler).
3. A frozen-tree edit to `src/smd/smdscheme/**` inside a UUID anchor block is unavoidable.

Each doc records: what diverged, from what authority (ANSI CL / this plan / frozen-tree rule), why, the consequences for later steps, and whether it is permanent or should be revisited.
Divergence docs are append-only history; supersede, don't rewrite.

---

# 4. Decision records

These decisions are made now so workers don't re-litigate them.
Overturning one requires a divergence doc and orchestrator sign-off.

**D1 — Layout: new sibling tree, freeze the Scheme tree.**
New code goes in `src/smd/smdlisp/`, namespace `smd::smdlisp`, one sub-namespace per directory, mirroring the `smdscheme` component structure.
`src/smd/smdscheme/**` is frozen for semantic changes because blog phases 5–12 transclude live code from it by UUID anchor; an in-place pivot would silently rewrite published posts.
`smdlisp` consumes `smdscheme`'s language-agnostic targets (`foundation`, `parser`) via their canonical includes and CMake targets; it does not copy them.
The Scheme-flavored components (`reader`, `elaborator`, `closure`, `sender`) are **adapted by copy** into `smdlisp`, then diverge freely.

**D2 — Symbol case: fold to uppercase at read time.**
The ANSI default readtable uppercases unescaped symbol names; `smdlisp` does the same and stores the folded spelling.
No `|escaped|` symbols, no readtable-case options.
Printing/round-tripping is not a project goal.

**D3 — `nil` and `t`.**
`nil` is a distinct value kind serving as false, the empty list, and the symbol `NIL`.
`t` reads as a symbol and elaborates to the canonical true value.
Truthiness everywhere is "anything but `nil` is true"; the Scheme `#f` convention does not carry over.

**D4 — Lisp-2.**
Environments carry separate variable and function namespaces.
`(f x)` looks up `f` in the function namespace; `#'f` / `(function f)` reifies a function binding as a value; `funcall` and `apply` call function values.

**D5 — One-shot dynamic-extent exits (the thesis).**
All nonlocal control (`block`/`return-from`, `catch`/`throw`, later `go`) is one-shot and upward-only.
In the CPS backend, exit points are data with explicit extent; using a dead exit is a diagnosed error, mirroring CL's "undefined behavior" as a checked error.
In the sender backend, exits map to early completion / stopped-and-error channels, and `unwind-protect` maps to cleanup on **all three** completion channels.

**D6 — Proper lists first.**
Cons cells exist as values from step L8 and support dotted pairs at the value level.
Reader syntax for dotted pairs (`(a . b)`) is deferred until a step needs it; file a divergence doc when deferring bites.

**D7 — One package plus keywords.**
No package system: a single implicit current package, plus `KEYWORD` for `:foo` self-evaluating keywords.
No `in-package`, `defpackage`, or package markers other than the leading colon.
This is a permanent ANSI divergence; it gets a divergence doc (seeded as DIV-0001).

**D8 — `tagbody`/`go` is optional.**
It is a step (L23) but may be deferred by divergence doc if the basic-block lowering doesn't fit the budget.
`block`/`catch`/`unwind-protect` are not optional; they are the payload of the thesis.

**D9 — Macro expansion is a separate datum→datum pass.**
Expansion runs between reader and elaborator, over the datum tree.
Host-defined (C++) macros come first; object-language `defmacro` comes later and runs the compile-time evaluator over code-as-list-values.

**D10 — Out of scope.**
Strings, characters, floats, ratios, bignums, vectors, CLOS, conditions/restarts beyond `catch`/`throw`, `format`, `loop`, `setf` expanders beyond `setq`, readtables, `eval-when`.
Each is a known ANSI divergence; the consolidation step records them in one limitations doc rather than one divergence doc each.

---

# 5. Reuse inventory

What the existing implementation contributes, by disposition.

## Use as-is (dependency, no copy)

| Component | Why it transfers |
|---|---|
| `foundation/` (static_vector, result, parse_error, arena_box, tree_arena, fix, source_pos/span, functor/applicative/alternative) | Language-agnostic; no datum/AST types leak in. |
| `parser/` (parser, cursor, alt, parser_ops) | Generic combinators; only the three char predicates are Scheme-flavored, and `smdlisp` supplies its own predicates (L4). |
| `src/smd/fixpoint/` (fix, box, recursion_schemes incl. mendler_fold/mendler_para) | Independent library, already external to smdscheme. |
| `sender_v` vocabulary aliases over Beman Execution | Thin aliases; include and reuse. |
| Build/infra: Makefile, toolchains, uv, pre-commit, Catch2 fetch, Beman submodule, CI | Unchanged. |
| Blog pipeline (org → transclusion → gfm export) | Unchanged; new phases plug in. |

## Adapt by copy into `smdlisp` (then diverge)

| Component | What changes |
|---|---|
| `reader/` (atom, datum_type, read_datum) | Case folding, keywords, `#'` shorthand, `;` comments, `t`/`nil`; same arena datum-tree shape. |
| `elaborator/` (elaborated_core, elaborate) | New special-operator set, Lisp-2 awareness, multi-expression bodies; same arena/fix core-tree shape. The landed `core_set`, `core_begin`, `core_prim`, and quoted-list elaboration (`elaborate_quoted_datum`) transfer almost directly to `setq`, `progn`, list builtins, and CL `quote`. |
| `closure/` (value, env, store, pairs, eval_direct, cps_code, closure_program) | `nil` truthiness, Lisp-2 env, escapes; same CPS-over-arena architecture and constexpr discipline. The landed `store` (mutable bindings for `set!`), `pair_heap`/`pair_ref` cons cells, `null_t`, and `unspecified` are the direct basis for L8 and L12. |
| `sender/` (comp_tree, fixpoint_eval, sender_mendler_*, scheme_tree graph dump) | Same structure over the CL core; escapes map to completion channels. `set!`/`begin`/pairs are already threaded through all five sender-side evaluators, so the CL ports have a worked pattern per backend. |
| `reflection/reified_environment` | Reused near-verbatim for the CL follow-on spike. |
| Public API (`smdscheme.hpp` pattern: `source_literal`, `compiled_closure`) | Same pattern as `smdlisp.hpp` / `compiled_lisp`. |

## New (no Scheme counterpart)

- Lisp-2 environment with function cells (L9).
- One-shot escape machinery: `block`/`return-from`, `catch`/`throw`, `unwind-protect` (L14–L15).
- Dynamic binding for special variables (L16).
- Macro expander pass, host macro registry, backquote, `defmacro` (L17–L19).
- Multiple values (L20).

## Untouched historical record

- `src/smd/smdscheme/**` (frozen per D1), blog phases 0–13, `docs/schemepoc-plan.md`, `docs/cps-direction.md`, existing checklist history.

---

# 6. Gap analysis

What Common Lisp-light needs versus what exists today.

| Gap | CL requirement | Current state | Steps |
|---|---|---|---|
| Reader case folding | Unescaped symbols fold to uppercase | Case-sensitive verbatim symbols | L4–L5 |
| Keywords | `:foo` self-evaluating | Absent | L5, L7 |
| Comments | `;` to end of line | No comments at all | L4 |
| `#'` | Sharpsign-quote → `(function f)` | Absent | L6 |
| `nil`/`t` truthiness | `nil` sole false value | `#f` sole false, hard-coded at every `if` site | L7 |
| Cons/lists as values | `cons`, `car`, `cdr`, quoted lists | Landed for Scheme (PR #26): `pair_heap`/`pair_ref` cells, `cons`/`car`/`cdr`/`list`/`null?` prims, quoted lists via `elaborate_quoted_datum` — adapt, don't invent | L8 |
| Lisp-2 | Separate function/variable namespaces | Single namespace, linear env | L9 |
| Multi-expression bodies / `progn` | Implicit progn in lambda, let, defun | Explicit `begin`/`core_begin` landed (PR #23); lambda/let bodies still single-expression | L10 |
| `let`/`let*` | Native or desugared | Desugared to lambda application (transfers) | L10 |
| `setq` | Assignment to lexical and special bindings | `set!` landed (PR #23) with store-backed mutable bindings — adapt for Lisp-2 and specials | L12 |
| `defun`/`defvar`/`defparameter` | Top-level definitions in the right namespace | `define` only, single namespace | L12 |
| `block`/`return-from` | Lexical one-shot exit | No nonlocal control | L14 |
| `catch`/`throw` | Dynamic-tag one-shot exit | Absent | L15 |
| `unwind-protect` | Cleanup on every exit path | Absent | L15 |
| Special variables | `defvar` + dynamic rebinding by `let` | Absent | L16 |
| Macros | `cond`, `when`, `unless`, `and`, `or`, `case` are macros | Absent (no macro layer at all) | L17 |
| Backquote | Template syntax for macros | Absent | L18 |
| `defmacro` | Object-language macros | Absent | L19 |
| Multiple values | `values`, `multiple-value-bind` | Single-value continuations | L20 |
| Sender parity | Full CL-light core incl. escapes on senders | Sender backends cover the old Scheme subset | L21 |
| `tagbody`/`go` | Intra-body jumps | Absent | L23 (optional, D8) |

Non-gaps worth naming: the arena/`fix` AST discipline, the constexpr-value pipeline, the CPS-with-native-continuations architecture (see `docs/cps-direction.md`), FFI via `foreign_function`, the reflection spike, and the freshly landed `set!`/`begin`/pairs machinery (PRs #23/#26, threaded through every backend) all transfer structurally; the CL work is additive on top of them.
The `MaxBindings == 16` hard-wiring in `constexpr_box<env<Core,16>>` is a known wart that the copied `closure/` code should fix in passing (parameterize, keep 16 as default) — worker discretion, divergence doc not required.

---

# 7. Layout and namespace

```txt
src/smd/smdlisp/CMakeLists.txt
src/smd/smdlisp/smdlisp.hpp                  (public one-shot API, L22)
src/smd/smdlisp/reader/                      (L4–L6)
src/smd/smdlisp/macroexpand/                 (L17–L19)
src/smd/smdlisp/elaborator/                  (L10, L12, L14–L16, L20)
src/smd/smdlisp/closure/                     (L7–L9, L11, L13–L16, L20)
src/smd/smdlisp/sender/                      (L21)
```

Namespace `smd::smdlisp`, one sub-namespace per directory (`smd::smdlisp::reader`, etc.).
Canonical includes: `#include <smd/smdlisp/reader/read_datum.hpp>`.
Cross-component dependencies on `smdscheme` use its canonical includes (`#include <smd/smdscheme/parser/parser.hpp>`) and link its CMake targets.
All existing style rules apply: file prologs, classical guards, angle includes, co-located `.test.cpp`, constexpr-first, UUID anchors for anything destined for prose.

---

# 8. Documentation workstream

The blog continues from phase 14 (`phase-14-set-bang.org`, the last Scheme-semantics post, already landed).
Each new phase is drafted as `docs/blog/phase-NN-slug.org` in the established format, transcluding live `smdlisp` code via new UUID anchors (never hand-copied blocks), and is a deliverable of the step listed.

| Phase | Working title | After step |
|---|---|---|
| 15 | Why not call/cc: from Scheme to Common Lisp | L2 |
| 16 | Reading Common Lisp: case, keywords, and `#'` | L6 |
| 17 | `nil`, `t`, and living in a Lisp-2 | L11 |
| 18 | `setq`, `defun`, `progn`: a programmable core | L13 |
| 19 | `block`, `catch`, `unwind-protect`: one-shot control in CPS | L15 |
| 20 | Macros in a compile-time compiler | L19 |
| 21 | Common Lisp control flow as sender graphs | L21 |
| 22 | What we left out, and why it matters | L24 |

Prose rules: one sentence per line; drafts are written in the author's voice per the repo's voice conventions and are marked `DRAFT — pending author revision` at the top; agents draft, the author finalizes.
Phase 15 is the pivot-rationale post and draws directly on section 0 of this plan; it should also note that phase 14's `set!`/store work survives the pivot as the seed of `setq` (L12) and that the pairs work seeds the CL list machinery (L8).

---

# 9. Steps

Each step states goal, key files, sketch where the design is novel, merge criteria, and dependencies.
"Verify" always means `make compile`, `make test`, `make lint` green, plus the orchestrator checks in section 2.
For adapted components, follow the corresponding `smdscheme` file's architecture unless the step says otherwise; the sketches only spell out what differs.

## Step L0 — Governance install

Append the "Common Lisp pivot" section (section 10 below) to `checklist.md`.
Confirm `docs/divergences/TEMPLATE.md` and `DIV-0001` exist.
Place the pivot invariants (frozen `smdscheme` tree, `smdlisp` layout, decision
records D1–D10 by reference) into the stable read tier
(`docs/compiler_architecture.org`), not a growing log.
Write `step-brief-<lane>.md` for L1.
Dependencies: none.

## Step L1 — `smdlisp` skeleton

`src/smd/smdlisp/{CMakeLists.txt,version.hpp,version.cpp,version.test.cpp}`, wired into `src/smd/CMakeLists.txt`, linking `smdscheme` foundation and parser targets to prove the dependency direction compiles.
Merge criteria: verify passes; `smdscheme` targets untouched.
Dependencies: L0.

## Step L2 — Blog phase 15 (parallel track)

Draft `docs/blog/phase-15-why-common-lisp.org` from section 0: the one-shot completion contract of senders, the multishot problem, the dynamic-wind/resource argument, CL's dynamic-extent design, the thesis.
No code.
Merge criteria: `make blog-md` renders it; existing phases' output unchanged.
Dependencies: L0.
May run in parallel with L1 and everything through L6.

## Step L3 — Blog deps infra fix (parallel track, optional)

The generated `.md.deps` files miss `orgit:` transclusion links (the Makefile sed only matches `[[file:…]]`), so posts don't rebuild when transcluded source changes.
Extend the sed/rule to capture `orgit:` link targets.
Merge criteria: regenerating deps for phase-12 lists its transcluded sources; verify passes.
Dependencies: L0.
Fully independent of all other steps.

## Step L4 — CL lexical layer

`src/smd/smdlisp/reader/cl_chars.hpp` (+ test): CL constituent/terminating character predicates, case folding of a single char, `;`-comment skipping integrated with intertoken space.
Reuses `smdscheme::parser::cursor` unchanged; supplies CL predicates instead of the Scheme ones in `parser/cursor.hpp`.
Merge criteria: static_asserts for folding, comment skipping, delimiter set.
Dependencies: L1.

## Step L5 — CL atoms

`src/smd/smdlisp/reader/atom.hpp` (+ test): integers (unchanged rules), symbols (folded to uppercase per D2), keywords (leading `:`, folded, distinct datum kind).
`t` and `nil` read as ordinary symbols; their meaning is assigned downstream.
Merge criteria: `read_atom("foo")` yields symbol `FOO`; `:bar` yields keyword `BAR`.
Dependencies: L4.

## Step L6 — CL datum reader

`src/smd/smdlisp/reader/{datum_type.hpp,read_datum.hpp}` (+ tests): arena datum tree with kinds integer, symbol, keyword, list, quote, function-quote.
Grammar: `datum := atom | list | 'datum | #'datum`; `#'x` lowers to a `datum_function` node (not to a `(function x)` list — preserve source reality, mirror how `'x` is handled today).
No dotted pairs in the reader (D6).
Merge criteria: `(defun f (x) (if x 1 2))` round-trips into the tree; failure cases for unterminated lists and stray `)`.
Deliverable: blog phase 16 draft.
Dependencies: L5.

## Step L7 — CL value model

`src/smd/smdlisp/closure/value.hpp` (+ test), adapted from the Scheme one: kinds `nil`, `integer`, `symbol`, `keyword`, `builtin`, `closure`, `foreign_function` (cons arrives in L8).
One truthiness function, `constexpr auto is_true(value const&) -> bool` — `nil` is the sole false — used by every later `if` site; the Scheme per-site encoding does not recur.
Merge criteria: static_asserts for truthiness of `nil`, `0`, `t`-symbol, keywords.
Dependencies: L1 (parallel with L4–L6).

## Step L8 — Cons cells and list builtins

Adapt the landed Scheme pairs machinery (PR #26: `pair_heap`, `pair_cell`, `pair_ref`, `null_t` in `smdscheme/closure/value.hpp` and `pairs.hpp`, plus `core_prim` and `elaborate_quoted_datum`) into the `smdlisp` value model.
CL deltas: `nil` replaces `null_t` as the empty list (per D3), `car`/`cdr` of `nil` is `nil` instead of an error, and the builtin set is `cons`, `car`, `cdr`, `list`, `null`, `eq`, `eql`, `atom` (CL names, no `?` suffixes).
Merge criteria: static_assert that a quoted-list value structure satisfies `car`/`cdr`/`null` laws, including the `nil` cases.
Dependencies: L7.

## Step L9 — Lisp-2 environment

`src/smd/smdlisp/closure/env.hpp` (+ test):

```cpp
// separate namespaces per D4; both linear, most-recent-first, as today
template <class Core, int MaxBindings>
class env {
  public:
    constexpr auto define_value(symbol name, value val) -> void;
    constexpr auto define_function(symbol name, value fn) -> void;
    [[nodiscard]] constexpr auto lookup_value(symbol name) const -> result<value>;
    [[nodiscard]] constexpr auto lookup_function(symbol name) const -> result<value>;
    // setq support (L12) will add: set_value(symbol, value) -> result<void>
};
```

Default environment installs builtins in the **function** namespace (`+`, `*`, `cons`, `car`, `cdr`, `list`, `null`, `eq`, `eql`, `atom`, `funcall`, `apply`).
Parameterize the closure-capture box capacity instead of the inherited hard-wired 16.
Merge criteria: static_asserts that `f` as a variable and `f` as a function coexist without shadowing each other.
Dependencies: L8.

## Step L10 — CL core model and baseline elaborator

`src/smd/smdlisp/elaborator/{elaborated_core.hpp,elaborate.hpp}` (+ tests), adapted from the Scheme elaborator.
Core kinds: integer, symbol-ref (variable namespace), keyword, quote (now including list data), `if`, `progn`, `lambda` (params + **body as expression sequence**, implicit progn), `function` (function-namespace ref or lambda), application (head symbol resolves in function namespace; `(lambda …)` head allowed), `funcall`/`apply` stay ordinary builtins, `let`/`let*` desugar to lambda application as today.
Special operators recognized here: `quote`, `if`, `progn`, `let`, `let*`, `lambda`, `function`.
`if` with two args gets implicit `nil` alternative.
Merge criteria: elaboration tests for each form; empty application and malformed forms produce errors with positions.
Dependencies: L6 and L9.

## Step L11 — Direct evaluator

`src/smd/smdlisp/closure/eval_direct.hpp` (+ tests): structural-recursive evaluator over the CL core, Lisp-2 lookup, `is_true` truthiness, implicit progn, closures capturing both namespaces.
Merge criteria (end-to-end static_asserts):

```lisp
(if nil 1 2)                        ; => 2
((lambda (x) (car (cdr x))) '(1 2 3))  ; => 2
(funcall #'cons 1 nil)              ; => (1)
```

Deliverable: blog phase 17 draft.
Dependencies: L10.

## Step L12 — `setq`, `defun`, `defvar`, `defparameter`

Adapt the landed `set!` machinery (PR #23: `core_set`, the `store` class of mutable binding cells, and its wiring through all backends) as the basis for `setq`.
Elaborator + evaluator: `setq` mutates the nearest lexical variable binding (error if unbound; special-variable interaction arrives in L16); `defun` defines in the function namespace at top level; `defvar`/`defparameter` define in the variable namespace and mark the symbol special (the mark is stored now, used in L16).
`setq` returns the assigned value per CL, not the Scheme `unspecified`.
Top-level sequencing: the program is an implicit progn of top-level forms.
Merge criteria: `(progn (defun twice (x) (+ x x)) (twice 4))` ⇒ `8` at compile time.
Dependencies: L11.

## Step L13 — CPS closure backend for the baseline

`src/smd/smdlisp/closure/{cps_code.hpp,closure_program.hpp}` (+ tests), adapted from the Scheme CPS backend per the architecture in `docs/cps-direction.md` (native higher-order continuations over the flat arena).
Covers everything through L12.
`progn` compiles to continuation chaining (the landed `begin` CPS wiring in `smdscheme/closure/cps_code.hpp` is the pattern); that structure is what L14 hooks into.
Merge criteria: every L11/L12 end-to-end test also passes through `compile_to_closure`.
Deliverable: blog phase 18 draft.
Dependencies: L12.

## Step L14 — `block` / `return-from`

The first payoff step; this is new machinery, sketch below.

```cpp
// One-shot lexical exit, per D5.
// block establishes an exit record; return-from completes to the
// block's continuation and kills the record; a dead exit is a
// diagnosed error, not UB.
using exit_id = int;

struct exit_record {
    symbol   name{};          // block name, function namespace not involved
    exit_id  id{invalid_exit};
    bool     live{false};
};
```

Elaborator: `(block name body…)` and `(return-from name expr)` core kinds; `return-from` resolves its block lexically at elaboration time (unknown block name is an elaboration error).
CPS: dispatch threads an exit table; `block` registers a live record whose continuation is the block's `k`; `return-from` evaluates its value, checks liveness, marks the record dead, and invokes the block continuation, skipping intervening frames; falling off the block end also kills the record.
`defun` bodies get an implicit `(block function-name …)` per CL.
Merge criteria:

```lisp
(block b 1 (return-from b 42) 3)   ; => 42
(defun f (x) (if x (return-from f 0) 1))  ; (f t) => 0
```

plus a diagnosed-error test for a dead exit (closure smuggling a `return-from` out of its extent).
Dependencies: L13.

## Step L15 — `catch` / `throw` and `unwind-protect`

`catch` establishes a **dynamic** exit keyed by an evaluated tag value (`eq` comparison); `throw` searches the dynamic exit stack innermost-first; both share the one-shot exit machinery from L14.
`unwind-protect` registers cleanup forms that run on **every** exit path through its extent: normal completion, `return-from`, and `throw`; in CPS this wraps both the normal continuation and the escape path through the exit table.
Merge criteria: cleanup-ordering tests (innermost cleanups first), `throw` through nested `unwind-protect`, uncaught `throw` is a diagnosed error.
Deliverable: blog phase 19 draft.
Dependencies: L14.

## Step L16 — Special variables and dynamic binding

`let` of a symbol marked special (by `defvar`/`defparameter`, L12) performs dynamic binding: save, bind, and restore via the unwind machinery of L15 so nonlocal exits restore correctly.
`setq` of a special assigns the innermost dynamic binding.
Merge criteria: the classic test —

```lisp
(defvar *x* 1)
(defun get-x () *x*)
(let ((*x* 2)) (get-x))   ; => 2 dynamically, would be 1 lexically
```

plus a test that `throw` past a dynamic binding restores the outer value.
Dependencies: L15.

## Step L17 — Macro expander with host macros

`src/smd/smdlisp/macroexpand/expander.hpp` (+ tests): a datum→datum pass between reader and elaborator.

```cpp
// Host macro: C++ function from a call-form datum to a replacement datum,
// writing into the same arena.  Registry is a static_vector keyed by symbol.
struct host_macro {
    symbol name{};
    // (tree, call-node) -> new node id, or error
    macro_fn expand{};
};
// macroexpand-1 applies one step at the head position;
// macroexpand iterates to fixpoint under a MaxExpansions budget
// (budget exhaustion is a diagnosed error, not a hang).
```

Implement `cond`, `when`, `unless`, `and`, `or`, `case` as host macros expanding to `if`/`progn`/`let` — matching their macro status in CL.
Wire the pass into the pipeline: read → expand → elaborate.
Merge criteria: expansion-shape tests plus end-to-end `(cond (nil 1) (t 2))` ⇒ `2`.
Dependencies: L10 (pipeline), independent of L14–L16 — may run in parallel with them.

## Step L18 — Backquote

Reader support for `` ` ``, `,`, `,@` lowering to datum template nodes, and an expansion of templates into `cons`/`list`/`append` builds during macro expansion.
`append` joins the builtin set.
Merge criteria: `` `(a ,x ,@ys) `` expansion tests at both datum and value level.
Dependencies: L17.

## Step L19 — `defmacro`

Object-language macros: `defmacro` registers a macro whose expander is a compiled closure run by the **compile-time evaluator** during the expansion pass, receiving the macro call as list values (L8) and returning a value that is reified back into the datum arena.
This is the "compiler running the language it compiles, at compile time" milestone; the datum⇄value reification pair is the new machinery.
Merge criteria: a `defmacro`-defined `my-when` behaves identically to the host `when`; expansion-budget error test.
Deliverable: blog phase 20 draft.
Dependencies: L18 (and transitively L11).

## Step L20 — Multiple values

`values` and `multiple-value-bind`; CPS continuations become n-ary (a `static_vector<value, MaxValues>` payload), single-value contexts take the primary value, missing values default to `nil` per CL.
Merge criteria: `(multiple-value-bind (a b) (values 1 2) (+ a b))` ⇒ `3`; `(+ (values 1 2) 10)` ⇒ `11`.
Dependencies: L13; independent of L14–L19 in principle, but schedule after L15 to avoid rebasing the escape machinery.

## Step L21 — Sender backend for the CL core

`src/smd/smdlisp/sender/` adapted from the Scheme sender/mendler backends: the CL core over senders, using `smd::fixpoint` `mendler_para` where the Scheme side does.
The thesis made executable, per D5: `return-from`/`throw` complete the corresponding scope's sender early; `unwind-protect` is an adapter running cleanup on `set_value`, `set_error`, **and** `set_stopped`; uncaught `throw` surfaces on the error channel.
Port the graph-dump toolchain so a `catch`/`throw`/`unwind-protect` program's sender graph can be rendered for the blog.
Merge criteria: sender-backend parity for the L11–L15 test set; a DOT dump of an `unwind-protect` example.
Deliverable: blog phase 21 draft.
Dependencies: L15 (L16/L20 coverage may follow in-step or as a recorded gap with divergence doc).

## Step L22 — Public API, Godbolt, FFI parity

`src/smd/smdlisp/smdlisp.hpp`: `source_literal` reuse and `template <source_literal Source> inline constexpr auto compiled_lisp = …` mirroring `compiled_closure`.
A Godbolt-extractable example and an FFI example (`foreign_function` calling C++ from Lisp) under `src/examples/`.
Merge criteria: example compiles standalone; public API test.
Dependencies: L13; update after L19/L20 land if signatures move.

## Step L23 — `tagbody` / `go` (optional, D8)

Lower `tagbody` to a set of basic-block continuations indexed by tag; `go` is a one-shot-per-invocation transfer using the L14 exit discipline (the tags' extent is the `tagbody`'s dynamic extent).
If the lowering exceeds budget, defer with a divergence doc.
Dependencies: L15.

## Step L24 — Documentation consolidation

Blog phase 22 (limitations per D10 and what they mean), a `docs/cl-limitations.md` recording every ANSI divergence in one place (rolling up the divergence docs), and a final `docs/compiler_architecture.org` pass for the two-language layout and finished state.
Merge criteria: `make blog-md` clean; all divergence docs referenced from the limitations doc.
Dependencies: everything else.

## Parallelism summary

- Track A (code spine): L1 → L4 → L5 → L6 → L10 → L11 → L12 → L13 → L14 → L15 → L16 → L21 → L24.
- Track B (values): L7 → L8 → L9 feeds L10; L7 starts right after L1, parallel to L4–L6.
- Track C (macros): L17 → L18 → L19 runs parallel to L14–L16 after L11.
- Track D (docs/infra): L2 and L3 run parallel to everything; blog drafts ride their named steps.
- L20, L22, L23 slot in as their dependencies allow.

---

# 10. Checklist section to append to `checklist.md`

```markdown
## Common Lisp pivot (docs/cl-pivot-plan.md)

- [ ] Step L0: governance install
- [ ] Step L1: smdlisp skeleton
- [ ] Step L2: blog phase 15 — why not call/cc
- [ ] Step L3: blog deps infra fix (optional)
- [ ] Step L4: CL lexical layer
- [ ] Step L5: CL atoms
- [ ] Step L6: CL datum reader (+ phase 16 draft)
- [ ] Step L7: CL value model
- [ ] Step L8: cons cells and list builtins (adapt landed pairs work)
- [ ] Step L9: Lisp-2 environment
- [ ] Step L10: CL core model and baseline elaborator
- [ ] Step L11: direct evaluator (+ phase 17 draft)
- [ ] Step L12: setq, defun, defvar, defparameter (adapt landed set! work)
- [ ] Step L13: CPS closure backend (+ phase 18 draft)
- [ ] Step L14: block / return-from
- [ ] Step L15: catch / throw / unwind-protect (+ phase 19 draft)
- [ ] Step L16: special variables and dynamic binding
- [ ] Step L17: macro expander with host macros
- [ ] Step L18: backquote
- [ ] Step L19: defmacro (+ phase 20 draft)
- [ ] Step L20: multiple values
- [ ] Step L21: sender backend for the CL core (+ phase 21 draft)
- [ ] Step L22: public API, Godbolt, FFI parity
- [ ] Step L23: tagbody / go (optional, D8)
- [ ] Step L24: documentation consolidation (+ phase 22 draft)
```

---

# 11. Durable invariants (stable tier — never a growing log)

These are project invariants, not step history. They belong in the stable read tier
(`AGENTS.md` / `docs/compiler_architecture.org`) and are referenced by anchor, never
re-narrated per step and never appended to a cumulative handoff:

```txt
The project pivoted from Scheme-light to Common Lisp-light semantics; rationale in docs/cl-pivot-plan.md section 0.
src/smd/smdscheme is frozen for semantic changes; blog phases 5-12 transclude live code from it by UUID anchor.
New work lives in src/smd/smdlisp, namespace smd::smdlisp, reusing smdscheme foundation and parser as dependencies.
Decision records D1-D10 in docs/cl-pivot-plan.md govern layout, case folding, nil/t, Lisp-2, one-shot exits, packages, macros, and scope cuts.
Divergence issue docs live in docs/divergences/, one numbered file per issue.
nil is the sole false value in smdlisp; truthiness goes through one is_true function, never per-site encodings.
All nonlocal control is one-shot and dynamic-extent; a dead exit is a diagnosed error.
```

---

# 12. Canonical clean-agent instruction

```txt
Read only your bounded reading set — nothing that grows per step:
  Tier 1 (rules pack): docs/codestyle.org, AGENTS.md, docs/CODING_RULES.md, CLAUDE.md
  Tier 2 (this step):  your lane's step-brief-<lane>.md, checklist.md
Then read the step section pasted below (the orchestrator pastes it; do not open the full plan).
Do NOT read docs/history/handoff-archive.md, docs/schemepoc-plan.md, or the full plan front to back.
Consult docs/compiler_architecture.org only by the anchor your step brief names. If you need a
fact you do not have, that is a defect in your step brief — report it; do not go spelunking.

Proceed to the next unchecked step in the "Common Lisp pivot" section of checklist.md, using the matching step section of docs/cl-pivot-plan.md (pasted by the orchestrator) as the specification.

Finish only that step.
Do not edit src/smd/smdscheme except where the step explicitly says so; never edit inside its UUID anchor blocks.

Run:

make compile
make test
make lint

When everything is green: update checklist.md; record any durable cross-step fact in docs/compiler_architecture.org (in place, by anchor), not in a log; rewrite your lane's step brief for the next agent per the step-brief contract in AGENTS.md (forward-only, bounded, no history); and file docs/divergences/DIV-NNNN docs for anything done differently than the plan or ANSI CL specifies.

Do not continue into the following step unless the current step is blocked and you document the blocker.
```
