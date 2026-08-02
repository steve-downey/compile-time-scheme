<!-- markdownlint-disable MD013 -->

# Three semantics, three witnesses

A contract in this project can be evaluated three different ways, by three different implementations, with three different bug classes.
No one of them subsumes another, and this project has now been bitten in two opposite directions.

1. **Constant evaluation** — `static_assert`, `constexpr` variable initialisation.
   The compiler's own interpreter.
   Nothing is lowered to a library primitive, the optimizer never runs, and evaluation order is whatever the front end does, which in GCC is left to right.
2. **Unoptimized runtime** — `CONFIG=Debug`, which is `-O0 -fno-inline -g3`.
   Real code generation, but the optimizer has not chosen anything yet.
   Argument evaluation order, temporaries and inlining are all in their naive form.
3. **Optimized runtime** — `CONFIG=Asan` (`-O3 -g -fsanitize=address,undefined,leak`) and `CONFIG=RelWithDebInfo` (`-O3 -g -DNDEBUG`).
   Constant folding, lowering of library calls to builtins, inlining, reassociation.

## The two defects, one from each end

**Only the optimizer sees it.**
`reader/number.hpp` needs to find a `/` in a token.
Spelled `text.find('/')`, that lowers to `memchr`, and GCC trunk r16-8246 constant-folds the `memchr` call against the *original* string literal rather than the offset `string_view`, so a `remove_prefix(1)` upstream is enough to be off by one.
Every `static_assert` passed, because the constant evaluator does not lower anything to `memchr`.
An `-O0` build would not have folded it either.
The Asan run is what failed, and it failed because Asan is `-O3`.
The workaround is to spell the search as `std::ranges::find_if`, which is not lowered to `memchr`; both `find_char` and `find_exponent_marker` carry a comment saying so.

**Only the unoptimized build sees it.**
`foundation/foldable.test.cpp`'s `pair_box_foldable_impl::fold_map` returned `m.combine(f(pb.first), f(pb.second))`.
Those two calls to `f` are arguments to one call, so they are unsequenced.
The instance documents "traversal order: `first`, then `second`", and the derived `fold_left` observes that order, because it derives itself by handing `fold_map` an `f` that updates an accumulator.
Measured with `g++-16 -std=c++26`, the derived `fold_left` over `pair_box<int>{1, 2}`:

| Witness | Result | Contract says |
|---|---|---|
| constant evaluation (`static_assert`) | 12 | 12 |
| `-O0` | **21** | 12 |
| `-O1` | 12 | 12 |
| `-O2` | 12 | 12 |

The compile-time twin passed.
The default `make test` passed.
The contract was still wrong, and the only witness that could say so was the one nobody ran.

## Why `Asan` is not the careful build

The natural reading of a config list is that the sanitizer build is the thorough one and the rest are speed runs.
Here it is the other way round.
`CONFIG=Asan` is the *most* optimized configuration the project routinely runs, and that is deliberate: a sanitizer at `-O0` is much weaker than it looks.
Undefined behaviour that only comes into existence after inlining and constant folding never gets constructed at `-O0`, so there is nothing for ASan or UBSan to trap.
Sanitizers want the optimizer.
What they cannot also give you is the unoptimized ordering.

This cuts both ways, and the `-O0` leg is not a strictly better build either:

| | `-O0` (`Debug`) sees | `-O3` (`Asan`) sees |
|---|---|---|
| unspecified argument evaluation order | yes, in its naive form | no — the optimizer picks an order and usually the documented one |
| sequencing and temporary-lifetime slips | yes | often folded away |
| constant folding / builtin lowering defects | no — nothing is folded | yes |
| UB reachable only after inlining | no — it is never constructed | yes |
| sanitizer coverage generally | weak | full |
| `-Wmaybe-uninitialized` and flow-sensitive warnings | not emitted | emitted |

Neither column is a subset of the other, which is the whole point.

## The practice

`make test-matrix` runs `Debug` then `Asan` against one build tree — Ninja Multi-Config, so switching configuration does not reconfigure and the second leg is cheap.
Run it before merging a step.
`make test` on its own is still the inner-loop command; it is the `Asan` leg only.

The project's standing rule was that anything meaningfully constant-evaluable is stated twice, once as a `static_assert` over a constant evaluation and again as a runtime `TEST_CASE`.
That rule is now three ways, because "runtime" was never one thing.

## What this does not cover

`-O0` and `-O3` on one compiler are two points, not a matrix.
Unspecified evaluation order is unspecified per compiler as well as per optimization level.
A second compiler is the better test for ordering and this project cannot have one: GCC-16 is required for `-freflection` (P2996), so the CI matrix in `.github/workflows/ci_tests.yml` is GCC-only by necessity.
The optimization matrix is what stands in for the compiler matrix here, which is a reason to take it more seriously rather than less.
