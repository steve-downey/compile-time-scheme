# BL-0002: size the node arena from a compile-time measurement

- **Status:** closed — superseded by D14, checked 2026-08-23 against the code
- **Date:** 2026-07-30, updated 2026-07-31, closed 2026-08-23
- **Origin:** the question "could the array-backed bump allocator be garbage collected at compile time to minimise unused runtime space?" (2026-07-30). Investigating it produced the finding below: there is no garbage, but there is a large amount of dead *capacity*.
- **Frozen-tree impact:** none.

## Relationship to the rebuild (2026-07-31)

Decision D14 — capacity is not part of type identity — is the general form of this item.
This document measures one symptom of the coupling; D14 removes the cause.

The sequencing that followed: do **not** implement two-pass sizing on `smdlisp`.
Design `src/smd/cl/` so that capacity is a property of storage rather than of the types stored, which makes the measurement either unnecessary or trivial.

## Closed, 2026-08-23

R1 landed and nobody had revisited this item since; the question was answerable against the tree rather than deferrable again, so it was checked directly.
`grep -rln "MaxNodes\|Capacity" src/smd/cl --include=*.hpp` outside `src/smd/cl/foundation/` returns nothing — no runtime value type in `src/smd/cl/` carries a capacity template parameter.
`src/smd/cl/eval/value.hpp`'s `value` variant (`value_fixnum`, `value_character`, `value_symbol`, `value_string`, `value_cons`, …) is capacity-free.
`src/smd/cl/core/ast.hpp` states the rule at the type it could have broken: `core_ast<MaxNodes, MaxChildren>` is the storage alias over `foundation::tagged_tree`, and its per-node leaf and tag types are documented as "a constant rather than a template parameter, for the same reason" (D14).
`datum_tree<MaxNodes, MaxList>` being parameterised is not a counter-example, per the same reasoning: that is storage, which is where D14 puts capacity.
D14 holds in the tree as built; this item's question has an answer and does not need a future revisit condition.
The measurements below stay useful regardless: they are the evidence for how much the *old*, `smdscheme`-era coupling cost, not a live measurement of `src/smd/cl/`.

## What

A compiled program carries its whole fixed-capacity arena into the runtime object.

`cps_of` captures the arena **by value** into the returned lambda (`smdscheme/closure/cps_code.hpp`), and `static_vector<T, Capacity>` is `std::array<T, Capacity>` plus a size (`foundation/static_vector.hpp`).
So the program object is `MaxNodes * sizeof(core_type)` bytes wide no matter how many nodes the program uses.
`smdlisp::lisp_program` defaults to `MaxNodes = 128`, `MaxList = 16`; `smdscheme` defaults to 32 and 16.

## Why the original framing was wrong

There is no garbage to collect.
Every allocation site in `smdscheme/elaborator/elaborate.hpp` boxes a node it is about to link into the tree — the `let*` unrolling, the `elaborate_quoted_datum` right-fold, the `cond` desugar.
Error paths propagate rather than backtrack, so nothing is allocated-then-abandoned.
The datum arena is already dropped on the Scheme side, and on the `smdlisp` side `lisp_program` owns it by composition per DIV-0007.
A mark-sweep or mark-compact pass over the core arena would reclaim approximately nothing.

The waste is unused capacity, which is a type-level problem, not an allocation-level one.

## Evidence

Measured on the `smdscheme` core layout.
**Method:** gcc-16 was not available, so a standalone probe reproducing `core_f_factory`'s variant, `static_vector`, `arena_box`, and `fix` was compiled with g++-13 `-std=c++20 -O2`.
Node layout does not depend on C++26 features, so the numbers should transfer, but re-measure against the real headers before acting.

| Config | `sizeof(core_type)` | arena | program object |
| --- | --- | --- | --- |
| `MaxNodes=32, MaxList=16` (Scheme default) | 280 B | 8968 B | 9248 B |
| `MaxNodes=256, MaxList=16` | 280 B | 71688 B | 71968 B |
| sized to 7 nodes, arity 2 | 56 B | 400 B | 456 B |

Two findings matter more than the totals.

**`MaxList` dominates `sizeof(core_type)`, not `MaxNodes`.**
Node size falls 280 B to 56 B going from `MaxList=16` to `2`, because `core_lambda` embeds `static_vector<string_view, MaxList>` — 260 B of parameter slots present in *every* variant alternative, including `core_integer`.

**The dead capacity reaches the binary.**
A model of `cps_of`'s capture, compiled at `-O2` with the program invoked on a runtime argument, emits both objects at full width into `.rodata`:

```txt
0000000000002060 0000000000000128 r program_tight   #  296 bytes
00000000000021a0 0000000000002420 r program_big     # 9248 bytes
```

For `(+ 1 (* 2 3))` in `src/examples/godbolt_arithmetic.cpp`, roughly 97% of that is dead.
The optimiser does not fold it away.

`smdlisp` numbers are unmeasured; with ~20 core node kinds and `MaxNodes=128` by default the absolute waste should be larger, but that is an expectation, not a measurement.

## The mechanism, if it is still needed after D14

Two passes over the same source, exploiting the fact that the source is already an NTTP.

```cpp
template <source_literal Source>
consteval auto measure() -> arena_metrics;   // { nodes, max_arity }

template <source_literal Source>
using sized_program = program<Source, measure<Source>().nodes, measure<Source>().arity>;
```

Proof-of-concept compiled against the same type shapes (g++-13, C++20): the `consteval` count is usable as a template argument, and node values survive the resize.

A measure/build mismatch fails safely: `static_vector::push_back` asserts, and in a constant-evaluated context an out-of-bounds write is a hard compile error.
A wrong measurement is a build failure, never a bad binary.

## Alternatives

- **Defunctionalise the CPS backend** so each node yields a closure type holding its children as members rather than as arena indices.
  Then the arena is compile-time scaffolding only and footprint is proportional to live structure.
  The obstacle is that `closure::closure<Core>` stores `&node`, a pointer into the core arena, which is why the arena must ride along.
- **`std::define_static_array` (P3491)** to promote the live nodes into static storage, reducing the arena to a `span`.
  Cleanest if available; **GCC 16 support unverified**.
  It does not by itself fix the `MaxList` factor, since the inline `static_vector` members are inside each node.
