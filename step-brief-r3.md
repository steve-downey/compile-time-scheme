# Step brief: R3 (reader and core AST)

Forward-only handoff for the next agent.
Read `docs/codestyle.org`, `AGENTS.md`, `docs/CODING_RULES.md`, `CLAUDE.md`, this file, and — last, immediately before writing code — `docs/cpp-rules.md`.
Nothing else, except the anchored sections named under "Dependencies" below.

## Next step's goal

Build the reader and the core AST in `src/smd/cl/`, with Foldable and Traversable instances on the datum and core trees (D15), against `src/smd/smdlisp/reader/**` as the behavioural oracle.
Symbol names in read data are interned: the reader takes a `symbol::symbol_table` and produces `symbol::symbol_id`s, not string atoms, so a datum never holds a view into source text where a symbol is meant.
Traversal order is documented as part of each instance's contract.
The orchestrator pastes the plan's R3 phase section (`docs/cl-rebuild-plan.md` §6) alongside this brief; this file does not restate it.

## Merge criterion

`make compile`, `make test`, `make lint`, `make compile-headers` green.
Law tests (Functor, Foldable, Traversable identities; shape preservation; effect order) come before substantive tests.
Error propagation is `traverse` over the result applicative, or `fold_left_short` where later work must be skipped — never an `if (!r.has_value())` ladder.
Reading the same symbol name twice anywhere in an input yields the same `symbol_id`.
Everything meaningfully constant-evaluable is constexpr with compile-time twins.

## Files this step owns

```txt
src/smd/cl/reader/*.hpp
src/smd/cl/reader/*.test.cpp
src/smd/cl/reader/CMakeLists.txt
src/smd/cl/CMakeLists.txt     (one add_subdirectory line)
checklist.md                  (tick R3)
step-brief-r3.md              (retire), step-brief-r4.md (write)
```

Do not touch anything else.
`src/smd/smdlisp/**` is the rebuild's behavioural oracle and is never edited.
`src/smd/cl/foundation/**` (R1) and `src/smd/cl/symbol/**` (R2) are finished substrate; growing either is a discovered deviation to record in this brief's successor, not a silent edit.

## Dependencies already satisfied

- `docs/cl-rebuild-plan.md` §2 — decision records; D15 is this step's mandate, D12 (interned symbols) and D14 (capacity is not part of type identity) bind the shapes.
- R1 landed `src/smd/cl/foundation/` under `smd::cl::foundation`: `static_vector`, `result` (+`result_instances`), `parse_error`, `source_pos`, `source_span`, `arena_box`, `functor`, `applicative`, `alternative`, `monoid`, `foldable`, `traversable`, `identity`, `fold_left_short` — all constexpr, all with law tests. Target `cl.foundation` (INTERFACE).
- R2 landed `src/smd/cl/symbol/` under `smd::cl::symbol`: `symbol_id` (capacity-free strong index; default-constructed is invalid, `valid()` distinguishes) and `symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols, MaxNameChars>` with `intern`, `make_uninterned`, `find` (returns `std::optional<symbol_id>`; uninterned entries are invisible), `name`, `is_interned`, `size`/`capacity`, `name_chars_used`/`name_chars_capacity`, and independently readable/writable `value`/`function`/`macro` slots (`std::optional`-returning; empty means unbound). Target `cl.symbol` (INTERFACE), links `cl.foundation`.

## Resolved (R2): the plan §7 open question

**The symbol table survives into the runtime program; it is not compile-time-only.**

- D12's self-containedness promise is kept only if names travel with the program: ids alone support `eq`, but `symbol-name`, printing, and runtime diagnostics need name recovery, so the name pool must exist wherever a symbol is observable.
- The slots are runtime state: `setq` under the evaluator mutates the value slot, so the table cannot end at the compile/run boundary; the macro slot is the compile-time face of the same entries. One table serves both stages.
- D14 holds: capacities parameterise the table because the table is storage; programs traffic in `symbol_id`, which carries no capacity, so the table riding along puts nothing capacity-shaped into value type identity.
- BL-0002 footprint: the cost of survival is entries plus the name pool, proportional to what was interned; the representation can shrink later without touching `symbol_id` or any value type.

Flag for the next docs-owning step (do not edit these files from R3): record this answer against `docs/cl-rebuild-plan.md` §7, and append the DIV-0013 dated note R1 asked for ("2026-08-01: still reproduces at r16-8246 with the doc's five-line reproduction").
No divergence doc is needed for the resolution itself; it applies D12 and D14 rather than departing from them.

## What R2 found that this step needs and cannot get elsewhere

- **The table's slot types are template parameters** because what a binding *is* belongs to later stages.
  The reader needs only interning, so reader APIs should be generic over the table instantiation (template on the table type); do not invent slot types in R3 to name a concrete table.
- **Table overflow is a precondition, not an error channel**, matching `static_vector::push_back`.
  A reader consuming untrusted input must check `size() < capacity()` and `name_chars_used() + name.size() <= name_chars_capacity()` and surface a `parse_error` before interning, or the Asan build will trip the assert.
- **`symbol_id` is default-constructible on purpose** (invalid, index -1): R1 found `static_vector<T, N>` requires default-constructible elements, so containers of ids work but a default id must never reach a table accessor.
- **DIV-0013 caution carries forward**: when a table or program becomes a namespace-scope `constexpr` object, keep null-pointer comparisons off the constant-evaluation path (R2 needed none; `find` reports absence with `std::optional`, not a sentinel pointer).
- R2 added nothing to `foundation/` and recorded no deviations.
