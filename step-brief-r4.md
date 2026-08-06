# Step brief: R4 (elaborator)

Forward-only handoff for the next agent.
Read `docs/codestyle.org`, `AGENTS.md`, `docs/CODING_RULES.md`, `CLAUDE.md`, this file, and — last, immediately before writing code — `docs/cpp-rules.md`.
Nothing else, except the anchored sections named under "Dependencies" below.

## Next step's goal

Build the elaborator in `src/smd/cl/elaborator/`: lower a `reader::datum_tree` to a `core::core_tree`, with error propagation as `traverse` over the result applicative and `fold_left_short` where later work must be skipped (D15) — never a check ladder.
Start from the pivot's baseline scope (literals, variables, keywords, calls, `if`, quote), growing `core_tag`/`core_leaf` in `src/smd/cl/core/ast.hpp` as forms demand; D18 says lower onto existing forms before adding tags.
The orchestrator pastes the plan's R4 phase section (`docs/cl-rebuild-plan.md` §6) alongside this brief; this file does not restate it.

## Merge criterion

`make compile`, `make test`, `make lint`, `make compile-headers` green.
Law tests before substantive tests for any new instance; the core tree's instances already have theirs (`foundation/tagged_tree_instances.test.cpp`).
Elaborating `(if (zerop n) 1 nil)` produces the shape `core/ast.test.cpp`'s `sample_core()` builds by hand.
Everything meaningfully constant-evaluable is constexpr with compile-time twins.

## Files this step owns

```txt
src/smd/cl/elaborator/*.hpp
src/smd/cl/elaborator/*.test.cpp
src/smd/cl/elaborator/CMakeLists.txt
src/smd/cl/core/ast.hpp            (grow leaves/tags as forms demand)
src/smd/cl/core/ast.test.cpp
src/smd/cl/CMakeLists.txt          (one add_subdirectory line)
checklist.md                       (tick R4)
step-brief-r4.md                   (retire), step-brief-r5.md (write)
```

Do not touch anything else.
`src/smd/smdlisp/**` is the behavioural oracle and is never edited.
`foundation/**`, `symbol/**`, and `reader/**` are finished substrate; growing any is a discovered deviation to record in this brief's successor.

## Dependencies already satisfied

- `docs/cl-rebuild-plan.md` §2 — decision records; D15 and D18 bind this step, D19 staged the tower.
- R1/R2 substrate as the R3 brief recorded: `foundation/` (targets `cl.foundation`) and `symbol/` (`cl.symbol`, `symbol_table`/`symbol_id`).
- R3 landed, all under D15's instances:
  - `foundation/tagged_tree.hpp` — `tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>`: a self-contained arena-tree value (no external arena; nodes are `variant<Leaf, tree_branch<Tag, MaxChildren>>`, children are `int` indices, root index, structural `==`). Children-before-parent construction is a precondition.
  - `foundation/tagged_tree_instances.hpp` — Functor/Foldable/Traversable for every instantiation. Contract: leaves in ascending node-index order — left-to-right source order for children-first builders; `traverse` rebuilds identical structure inside the effect and sequences leaf effects in that order; the result applicative visits every leaf (leftmost error wins); early exit is `fold_left_short`.
  - `reader/` (target `cl.reader`) — `read(source, symbol_table)` / `read_datum(cursor, symbol_table)`, generic over the table instantiation, default `standard_readtable`. Datum leaves: `datum_fixnum` (int), `datum_symbol`/`datum_keyword` (interned `symbol_id`, D12), `datum_character`, `datum_string` (own storage), `datum_tower` (kind bignum/ratio/floating + radix + spelling, D19: readable, not yet executable). Branch tags: `list, vector, quote, function, backquote, unquote, unquote_splice` (quote family: one child).
  - `core/ast.hpp` (target `cl.core`) — leaves `core_fixnum, core_variable, core_keyword, core_nil, core_t`; tags `core_call{callee symbol_id}, core_if, core_progn`; `core_tree<MaxNodes, MaxChildren>`. Tags are variant alternatives and may carry payload — that is how `core_call` keeps its Lisp-2 callee out of the children.

## What R3 found that this step needs and cannot get elsewhere

- **GCC trunk r16-8246 misfolds memchr-lowered searches on views offset into string literals.**
  `string_view::find`/`ranges::find`/`substr(...).find` after `remove_prefix(1)` of a constant literal folds to the offset in the *original* literal at `-O1` and above (compile-time evaluation is correct, so static_asserts pass while runtime fails).
  Reproduction: `constexpr auto f(std::string_view s){ s.remove_prefix(1); return s.find('/'); }` — `f("-3/4")` returns 2 at `-O2`, 1 at `-O0`.
  Workaround in `reader/number.hpp` (`detail::find_char`, `detail::find_exponent_marker`): spell the search as `ranges::find_if` with a predicate lambda, which is not memchr-lowered.
  Do the same in any new code; do not "simplify" those helpers back to member `find`.
- **Flag for the next docs-owning step** (do not edit these files from R4): a new toolchain divergence record for the misfold above; plus, still pending from R2: the plan §7 answer ("the symbol table survives into the runtime program") and the DIV-0013 dated note ("2026-08-01: still reproduces at r16-8246").
- **Recorded R3 deviations from its brief's file list** (both driven by D19's rewrite of the phase): the core AST went into a new `src/smd/cl/core/` rather than under `reader/`, and the shared tree container grew `foundation/` (`tagged_tree*`), so D15's instances are written once for both trees. `cl/CMakeLists.txt` gained two subdirectory lines, not one.
- **Keywords intern colon-kept** (`:foo` → table entry `":FOO"`): keyword-ness stays a datum kind *and* `eq` works on keywords, but `symbol-name` recovery must strip the colon until a package system exists; a printer gets `:FOO` for free. Known collision corner: the escaped symbol `|:FOO|` interns to the same entry as keyword `:foo`. A lone `:` reads as a symbol named `:` (oracle parity).
- **The reader is deliberately more ANSI than the oracle** where the oracle has defects: `+9` and `10.` are fixnums here, symbols there (so they are absent from `oracle_compare.test.cpp`); an over-long token errors here, silently splits there. Never pin the oracle's behaviour for these.
- **`t` and `nil` read as ordinary symbols** `T`/`NIL` (interned); mapping them to `core_t`/`core_nil` is this step's job, as is deciding what `datum_string`/`datum_character` elaborate to (they are executable-trivial but have no core leaf yet — add leaves when you elaborate them). `datum_tower` leaves elaborate to a diagnosed error until a tower lane lands (D19: readable ≠ executable).
- **Sequencing errors through parse-shaped code**: `reader/read.hpp` `detail::and_then` is a result bind used where there is no structure to traverse (a parse is an unfold). If the elaborator needs the same shape, promoting `and_then` into `foundation/result.hpp` is the natural growth — record it if done.
- **Not yet in the reader**, deferred with errors, not silence: `#n(...)` sized vectors, `#:` uninterned symbols, `#C(...)` complex (tower lane), dotted pairs (oracle's D6 carries over), packages (colon is an ordinary constituent, so `a:b` is a symbol named `A:B`).
- **The informal SBCL differential check of the new reader syntax (strings, characters, tower spellings, vectors) is pending until sbcl is installed** — no CL implementation exists in this environment; those behaviours were derived from the ANSI spec and pinned by `read.test.cpp`. Run the cross-check when sbcl arrives (plan §4 tier 3).
