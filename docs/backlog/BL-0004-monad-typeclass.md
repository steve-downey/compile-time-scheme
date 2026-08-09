# BL-0004: finish the Monad typeclass — `and_then`'s home, and why `fold_left_short` keeps its loop

- **Status:** open — partially landed on branch `monad-typeclass`, unverified against the project toolchain
- **Date:** 2026-08-09
- **Origin:** a question about phase 24's "A fold that stops" — whether `fold_left_short` is classically a `foldM` or a right fold with delayed evaluation, whether the loop honours the spirit of D15's exemption, and whether rewriting it would be better, worse, or indifferent.
- **Frozen-tree impact:** none.

## What

Two separable things, one of which is done and one of which is not.

**Landed on `monad-typeclass`:**

- `src/smd/cl/foundation/monad.hpp` — the Monad typeclass in the house shape
  (CRTP base over `bind`/`pure`, `monad_typeclass<T>` variable template, `bind`
  and `join` CPOs). `pure` is implementor-facing only, as in `applicative.hpp`,
  because it cannot be dispatched from its argument.
- `src/smd/cl/foundation/result_instances.hpp` — `result_monad_impl` /
  `result_monad_map` and the `monad_typeclass<result<T>>` registration.
  `result_applicative_impl::apply` is now monad-derived, which is what
  `docs/CODING_RULES.md`'s Semantic Defaults ("default tree Applicative
  semantics are monad-derived") has asked for all along; it previously wrote
  the two error branches out by hand.
- `src/smd/cl/foundation/monad.test.cpp` — the three laws, the
  does-not-invoke-on-failure property, the derived operations, and four
  assertions pinning that the monad-derived `apply` has the semantics the
  hand-written one had (leftmost error wins).
- `src/smd/cl/foundation/fold_left_short.hpp` — documentation only. Records
  why this fold is not derived from `bind`.

**Not landed, and the reason this item stays open:**

- `and_then` (`src/smd/cl/foundation/result.hpp:104`) is `bind` under the
  datatype's own name and is still defined in the datatype header.
  `result_monad_impl::bind` currently delegates to it, so there is one
  implementation, but the dependency points the wrong way:
  `docs/CODING_RULES.md`'s Typeclass Design rules say datatypes and typeclass
  adaptations are separate concerns, and `result_instances.hpp` is the
  designated adapter location. Moving the definition there and leaving
  `and_then` as a spelling of the CPO would touch every caller's include list —
  `reader/read.hpp`, `elaborator/elaborate.hpp`, `eval/machine.hpp`,
  `eval/builtin.hpp`, `eval/environment.hpp` and their tests currently include
  `result.hpp` but not `result_instances.hpp`.

## Why it is not done

The include-list change above is mechanical but wide, and it is exactly the
class of change that needs a real compile to be worth trusting. The session
that wrote this had neither the toolchain nor the test framework (see
Evidence), so it stopped at the additive part.

## Evidence

**On the design question — should `fold_left_short` be a `foldM`?**

It is one. The monad was already in the tree unnamed: `and_then`
(`result.hpp:104`) is `bind`, and `result.hpp:92-99` already documents it as
the third shape alongside `traverse` and `fold_left_short`. So the premise
"nothing has needed a Monad typeclass yet" is not quite what happened —
something did, and it was spelled as a free function on the concrete type.

But the typeclass does not let the fold be rewritten, and the reason is worth
recording because it is easy to get backwards. **Early exit is not a Monad
operation.** `bind` skips the *work* after a failure, because it never invokes
its function; it does not stop the *iteration*. Haskell's
`foldM f z xs = foldr (\x k z -> f z x >>= k) return xs z` stops the traversal
only because `>>=` is lazy in `k`, `k` being the unevaluated tail of the entire
fold. Strict evaluation has to ask the effect whether it has already failed,
which is what `short_circuit_effect` is for. That concept is not a botched
Monad; it is the strictness surcharge, and it now says so in the header.

Two concrete routes were tried:

- *Materialized right fold.* `foldr c return xs` needs `k` as a value, and
  `k`'s type changes at every step, so a runtime-length range needs type
  erasure. This is the same wall `foldable.hpp:22-26` already documents for
  why `fold_right` is an instance primitive rather than derived from
  `fold_map`. Not attempted further.
- *Recursively-named continuation.* Monomorphic, uses only `bind`, needs no
  `has_value` anywhere. This was built and it works: it compiles under
  `constexpr` and short-circuits correctly, reproducing `visited == 2` on the
  input `fold_left_short.test.cpp:93` uses. Its cost is constexpr call depth
  linear in the range length, against the loop's O(1).

**Measured, with a caveat that matters.** The container available to this
session had GCC 13 and Clang 18 only — no GCC 16, no Catch2, no configured
build — so nothing here ran under the project's toolchain and `make test-matrix`
was never run. Under `clang++-18 -std=c++2b` with libstdc++ 13 and the default
512-frame `constexpr` depth limit, folding a `static_vector<int, N>` with a
`result<int>`-returning step:

| N | recursive `foldM` | current loop |
|---|---|---|
| 32 | ok | ok |
| 64 | ok | ok |
| 128 | depth exceeded | ok |
| 256 | depth exceeded | ok |

and with the same 32-element fold placed under an artificial enclosing
`constexpr` call depth, the recursive version failed between depth 300 and 400
while the loop was still fine — i.e. roughly four frames per element, so a
32-element fold consumes something like a quarter of a translation unit's
entire budget before the enclosing evaluation is counted.

Treat the *shape* as established and the *numbers* as indicative only. They are
specific to `result`-over-`std::variant`, to libstdc++ 13, and to Clang's
frame accounting; GCC 16's may differ. What does not depend on the compiler is
that the recursive form is O(n) in a budget the loop is O(1) in, and that this
fold runs inside `elaborate()`, which is itself constexpr-evaluated — so the
budget is already partly spent when it is reached.

**On the spirit of D15.** The loop is inside a substrate generic algorithm,
which D15 permits. It is also not what D15 is aimed at: the target is
`if (!r.has_value()) return r;` ladders *in passes*, and this loop is the one
place that ladder is written so no pass needs one. The exemption is working as
intended here.

## Open questions

- Does GCC 16 spend the same ~4 constexpr frames per element? If it is
  dramatically cheaper the recursive derivation deserves a second look, though
  O(n) vs O(1) in depth would still decide it for large columns.
- Should `topo_fold.hpp`'s `fold_up_short` take `short_circuit_effect` instead
  of hard-wiring `result<A>`? It re-implements the same early exit inline.
  Note it should *not* be routed through `fold_left_short`, which takes `Acc`
  by value and would move the whole column once per element instead of pushing
  in place.
- Does anything other than `result` want a Monad instance, or is a one-instance
  typeclass premature? `outcome` in `eval/` is the obvious candidate.

## Cost and risk

The landed part is additive: one new header, one new test file, two CMake
lines, and a `result_instances.hpp` change whose only semantic content is that
`apply` is now nested `bind`s instead of hand-written branches. Verified only
as far as this session could: every `static_assert` in `monad.test.cpp` passes
under Clang 18, and `result_instances`, `result`, `fold_left_short`,
`foldable`, `traversable` and `applicative` test translation units still
compile there against a stub `catch_test_macros.hpp`. **`make test-matrix` has
not been run. Treat the branch as unverified until it has.**

The unlanded part is wider — five headers plus tests gain an include — but is
mechanical and fully checked by the compiler.

## Decision criteria

Schedule the remaining move when a step is already touching those five
headers' include lists, or when a second Monad instance makes the CPO earn its
keep. Close as declined if `result` stays the only monad in the tree, in which
case `and_then` on the concrete type is honest and the typeclass is ceremony.
