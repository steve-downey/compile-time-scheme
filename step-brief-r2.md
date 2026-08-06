# Step brief: R2 (interned symbols)

Forward-only handoff for the next agent.
Read `docs/codestyle.org`, `AGENTS.md`, `docs/CODING_RULES.md`, `CLAUDE.md`, this file, and — last, immediately before writing code — `docs/cpp-rules.md`.
Nothing else, except the anchored sections named under "Dependencies" below.

## Next step's goal

Build the interned symbol table of decision D12 in `src/smd/cl/`, as the keystone the rest of the rebuild hangs on.

A symbol is an entry in a symbol table, referred to by a stable id, carrying a name, a value slot, a function slot, and a macro slot.
Identity is id comparison, not string comparison.
This is what replaces `smdlisp`'s `closure::symbol` (a `std::string_view` wrapper) and what closes or downgrades DIV-0002, DIV-0006, DIV-0007, DIV-0009, DIV-0010 and DIV-0014.
Design for `gensym` from the start: an uninterned symbol is a table entry with no name lookup, not a reserved-name convention.

## Merge criterion

`make compile`, `make test`, `make lint`, `make compile-headers` green.

Interning the same name twice yields the same id; `eq` on ids is identity; distinct names yield distinct ids; an uninterned symbol is never found by name lookup.
Slots are independently readable and writable.
Everything meaningfully constant-evaluable is constexpr with compile-time tests.

## Files this step owns

```txt
src/smd/cl/symbol/*.hpp
src/smd/cl/symbol/*.test.cpp
src/smd/cl/symbol/CMakeLists.txt
src/smd/cl/CMakeLists.txt     (one add_subdirectory line)
checklist.md                  (tick R2)
step-brief-r2.md              (retire), step-brief-r3.md (write)
```

Do not touch anything else.
`src/smd/smdlisp/**` is the rebuild's behavioural oracle and is never edited.
`src/smd/cl/foundation/**` is R1's finished substrate; use it, do not grow it in this step without treating that as a discovered deviation worth recording.

## Dependencies already satisfied

- `docs/cl-rebuild-plan.md` §2 — decision records; D12 is this step's mandate, D14 (capacity is not part of type identity) binds the table's shape.
- R1 landed `src/smd/cl/foundation/`: `static_vector`, `result` (+`result_instances`), `parse_error`, `source_pos`, `source_span`, `arena_box`, `functor`, `applicative`, `alternative`, `monoid`, `foldable`, `traversable`, `identity`, and `fold_left_short` — all under namespace `smd::cl::foundation`, all constexpr, all with law tests.
  The library target is `cl.foundation` (INTERFACE); link it.
- Error propagation over a sequence of fallible steps is `fold_left_short(range, init, f)` (early exit) or `traverse` over the result applicative (visits everything); pick by whether later work must be skipped.

## What R1 found that this step needs and cannot get elsewhere

- **`result<T>` is not default-constructible**, so `static_vector<result<T>, N>` does not instantiate (its `std::array` storage requires default-constructible elements).
  Effect-typed containers must hold values, not results; keep results in return channels.
  This will matter whenever a table operation wants to return `result<symbol_id>` collections.
- **The traverse/fold split is behavioural, not stylistic.**
  `traverse` over `result` visits every element and takes the leftmost error; `fold_left_short` stops visiting at the first error.
  Both are pinned by tests (`static_vector_instances.test.cpp`, `fold_left_short.test.cpp`); do not re-derive either behaviour, cite it.
- **DIV-0013 still reproduces** on the current toolchain (GCC 16.0.1 20260322, trunk r16-8246): under `-fsanitize=null`, a null comparison against the address of a subobject of a namespace-scope `constexpr` object does not fold.
  The divergence stays open; when a symbol table becomes part of a namespace-scope `constexpr` program object, do not guard internal pointers with null comparisons on the constant-evaluation path, or the Asan config will reject the `static_assert` twin.
  R1 could not append the re-verification note to `docs/divergences/DIV-0013-…md` (file ownership); whoever next owns a docs step should add a dated note: "2026-08-01: still reproduces at r16-8246 with the doc's five-line reproduction".
- **`fold_fix` was confirmed uninstantiable and was not ported.**
  Compiling it errors with `std::integral_constant<bool, false>` has no member `fmap` (arguments to the CPO are swapped at `fix.hpp:50`), and it has zero call sites in either repository.
  `src/smd/cl/` has no `fix.hpp`; recursion schemes arrive with `mendler_para` when R3/R4 need them, unified per DIV-0016.
- **`smd::cl` is a namespace, `cl` a directory**; guards are `SRC_SMD_CL_<AREA>_<NAME>_HPP`.
  The `add_subdirectory(cl)` hook went into `src/smd/CMakeLists.txt` (the R1 brief said `src/CMakeLists.txt`, but that file only descends to `smd/` and `examples/`; delegating only to immediate children is the CMake rule that wins).
- **BL-0002 was not re-measured**: R1 added only an accessor (`capacity()`) to `static_vector`; its data layout is untouched, so the probe numbers stand.

## Note for whoever picks this up

R1's substrate has no behaviour of its own; its law tests are the contract.
If a symbol-table design decision looks like it wants a new foundation facility, that is a deviation to record in this brief's successor, not a silent edit to `foundation/`.
