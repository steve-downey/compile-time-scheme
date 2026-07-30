# BL-0002: size the node arena from a compile-time measurement

- **Status:** open
- **Date:** 2026-07-30
- **Origin:** the question "could the array-backed bump allocator be garbage collected at compile time to minimise unused runtime space?" (2026-07-30). Investigating it produced the finding below: there is no garbage, but there is a large amount of dead *capacity*.
- **Frozen-tree impact:** **none required.** The measure-then-build approach needs no change to `arena_box`, `tree_arena`, or `static_vector` — it only instantiates the existing templates at tighter arguments. It can land entirely in `src/smd/smdlisp/smdlisp.hpp`, which is editable. The variants that *would* touch the frozen tree are called out under Alternatives.

## What

A compiled program carries its whole fixed-capacity arena into the runtime
object. Size the arena from the source instead of from a default.

`cps_of` captures the arena **by value** into the returned lambda
(`smdscheme/closure/cps_code.hpp`, `cps_of`), and `static_vector<T, Capacity>`
is `std::array<T, Capacity>` plus a size (`foundation/static_vector.hpp`). So the
program object is `MaxNodes * sizeof(core_type)` bytes wide no matter how many
nodes the program uses. `smdlisp::lisp_program` defaults to `MaxNodes = 128`,
`MaxList = 16`; `smdscheme` defaults to 32 and 16.

The proposed mechanism is two passes over the same source, exploiting the fact
that the source is already an NTTP (`source_literal`):

```cpp
// pass 1: run the front end at a generous cap purely to measure it
template <source_literal Source>
consteval auto measure() -> arena_metrics;   // { nodes, max_arity }

// pass 2: instantiate the real program at exactly the measured bounds
template <source_literal Source>
using sized_lisp_program =
    lisp_program<Source, measure<Source>().nodes, measure<Source>().arity>;
```

Note that `MaxBindings` must stay 16 (`smdlisp.hpp` documents the constraint
against `closure/cps_code.hpp`), so only `MaxNodes` and `MaxList` are tunable.

## Why it is not done

Nobody has needed it. Nothing in `checklist.md` or `docs/cl-pivot-plan.md`
concerns runtime footprint — the thesis is compile-time compilation, and the
demo programs are small enough that 9 KB of `.rodata` costs nothing. It is worth
recording because the waste is structural rather than incidental, and because
the framing that prompted it ("garbage collect the arena") is the wrong fix and
would otherwise be attempted again.

**There is no garbage to collect.** Every allocation site in
`smdscheme/elaborator/elaborate.hpp` boxes a node it is about to link into the
tree — the `let*` unrolling, the `elaborate_quoted_datum` right-fold, the `cond`
desugar. Error paths propagate rather than backtrack, so nothing is
allocated-then-abandoned. The datum arena is already dropped on the Scheme side
(`closure_program.hpp` holds `arena_dr` as a local and never captures it); on the
`smdlisp` side `lisp_program` owns it by composition per DIV-0007. A mark-sweep
or mark-compact pass over the core arena would reclaim approximately nothing.

## Evidence

Measured on the `smdscheme` core layout. **Method:** gcc-16 was not available in
the environment, so a standalone probe reproducing `core_f_factory`'s variant,
`static_vector`, `arena_box`, and `fix` was compiled with g++-13 `-std=c++20
-O2`. Node layout does not depend on C++26 features, so the numbers should
transfer, but **re-measure against the real headers on gcc-16 before acting.**

| Config | `sizeof(core_type)` | arena | program object |
| --- | --- | --- | --- |
| `MaxNodes=32, MaxList=16` (Scheme default) | 280 B | 8968 B | 9248 B |
| `MaxNodes=256, MaxList=16` | 280 B | 71688 B | 71968 B |
| sized to 7 nodes, arity 2 | 56 B | 400 B | 456 B |

Two findings matter more than the totals:

- **`MaxList` dominates `sizeof(core_type)`, not `MaxNodes`.** Node size falls
  280 B → 56 B going from `MaxList=16` to `2`, because `core_lambda` embeds
  `static_vector<string_view, MaxList>` — 260 B of parameter slots present in
  *every* variant alternative, including `core_integer`. Compacting node count
  alone leaves the larger factor untouched.
- **The dead capacity reaches the binary.** A model of `cps_of`'s capture,
  compiled at `-O2` with the program invoked on a runtime argument, emits both
  objects at full width into `.rodata`:

  ```txt
  0000000000002060 0000000000000128 r program_tight   #  296 bytes
  00000000000021a0 0000000000002420 r program_big     # 9248 bytes
  ```

  For `(+ 1 (* 2 3))` in `src/examples/godbolt_arithmetic.cpp`, roughly 97% of
  that is dead. The optimiser does not fold it away.

The two-pass mechanism itself was proof-of-concept compiled against the same type
shapes (g++-13, C++20): the `consteval` count is usable as a template argument,
and node values survive the resize (`static_assert`-checked). `smdlisp` numbers
are **unmeasured** — with ~20 core node kinds and `MaxNodes=128` by default the
absolute waste should be larger, but that is an expectation, not a measurement.

## Open questions

- What is the compile-time cost? Pass 1 runs the reader and elaborator twice,
  but the deep template instantiation in the CPS/closure backend then happens at
  the *small* bound instead of the generous one. Plausibly a net improvement,
  especially for the `MaxNodes=128` default. Must be measured, not assumed —
  and measured on `make compile` wall time, not guessed from template counts.
- `MaxNodes` and `MaxList` are part of type identity (`arena_box<T, MaxNodes>`
  templates on `MaxNodes` purely to disambiguate handles). Shrinking them
  changes `core_type`, so user code naming a concrete instantiation breaks —
  `src/examples/godbolt_arithmetic.cpp:7` has a literal `core_type<32, 16>`, and
  `godbolt_lisp.cpp` reaches types through `program::core_type`, which is the
  pattern that survives. How much of the example and test surface needs to move
  to deduced types?
- Should this be a new API (`sized_lisp_program`) alongside the existing one, or
  should the defaults change? A new alias is additive and cannot regress
  anything; changing defaults is a smaller surface but touches every caller.
- Godbolt extractability is a project value (step 27, L22). Does the two-pass
  formulation still read clearly as a single-file demo, or does it obscure the
  point the examples exist to make? If it hurts the demo, that is a reason to
  decline.
- Does the measured `MaxList` need headroom for macro expansion? `defmacro`
  reifies expander output back into the datum arena (`cl-pivot-plan.md`), so the
  post-expansion tree is what must be measured. Pass 1 must run the full front
  end including expansion, not just the reader.

## Cost and risk

Small and contained: a `consteval` measure helper plus an alias template in
`smdlisp.hpp`, no changes to existing types, no frozen-tree edits. Perhaps 30–60
lines with tests.

The main risk is a measure/build mismatch — if pass 1 under-counts, pass 2
overflows the arena. That fails safely: `static_vector::push_back` asserts
`size_ < Capacity`, and in a constant-evaluated context an out-of-bounds
`std::array` write is a hard compile error, not UB. A wrong measurement is a
build failure, never a bad binary. This should be stated in a test.

## Alternatives, in increasing ambition

- **Defunctionalise the CPS backend** so each node yields a closure type holding
  its children as members rather than as arena indices. Then the arena is
  compile-time scaffolding only, footprint is proportional to live structure,
  and no sizing pass is needed. This is the structural fix. The obstacle is that
  `closure::closure<Core>` stores `&node`, a pointer into the arena
  (`cps_code.hpp`, `core_lambda` arm), which is *why* the arena must ride along.
  Frozen-tree work on the Scheme side.
- **`std::define_static_array` (P3491)** to promote the live nodes into static
  storage, reducing the arena to a `span` — pointer plus size in the program
  object, node data sized exactly, no template-parameter plumbing. Cleanest if
  available. **GCC 16 support unverified**; check before planning around it.
  Note it does not by itself fix the `MaxList` factor, since the inline
  `static_vector` members are inside each node.

## Decision criteria

- **Schedule** if a step needs many programs in one translation unit, a program
  large enough to make `MaxNodes` painful, or an embedded-style demo where
  `.rodata` size is the point being made.
- **Schedule** if the compile-time measurement comes back favourable — a
  measure-then-build that makes `make compile` *faster* at `MaxNodes=128` is
  worth doing for build time alone, independent of footprint.
- **Decline** if it complicates the Godbolt examples, which exist to show the
  pipeline rather than to be small.
