# BL-0004: finish the Monad typeclass — `and_then`'s home, and why `fold_left_short` keeps its loop

- **Status:** open — partially landed on branch `monad-typeclass`, verified on GCC 16
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
  `has_value` anywhere — and it is **drop-in**. Substituted for the loop
  through an include overlay, it compiles and constexpr-evaluates the entire
  pipeline (`read` → `elaborate` → `machine::run`) at every existing call
  site. Feasibility is not the objection; depth is.

**Measured on GCC 16** (`g++-16 16.0.1 20260322 trunk r16-8246`,
`-std=gnu++26`), 2026-08-09, by bisecting the minimum `-fconstexpr-depth`
each form needs. Synthetic fold of a `static_vector<int, N>` with a
`result<int>` step:

| N | recursive `foldM` | current loop |
|---|---|---|
| 4 | 48 | 19 |
| 8 | 80 | 19 |
| 16 | 144 | 19 |
| 32 | 272 | 19 |
| 64 | 528 | 19 |
| 128 | 1040 | 19 |

An exact fit: `depth(N) = 8N + 16` for the recursive form, a flat 19 for the
loop. **Eight frames per element, not the four an earlier Clang 18 measurement
suggested** — the caveat on those numbers was warranted and the correction went
against the recursive form.

Over the real pipeline rather than the synthetic fold, quoting a list of N
symbols:

| list length | recursive `foldM` | current loop |
|---|---|---|
| 4 | 171 | 60 |
| 8 | 171 | 60 |
| 16 | 178 | 60 |
| 32 | 306 | 60 |

The loop is flat at 60 frames whatever the input; the recursive form reaches
306, or 60% of the 512 default, on a 32-element list. Both fit, so this is a
margin argument and not a correctness one — 452 frames of headroom against
206.

Three claims from the pre-GCC-16 version of this note were wrong and are
withdrawn. The recursive derivation is not infeasible, only costlier. Depth
tracks *actual* data length and not declared capacity, so "a fold over
`default_max_nodes` (256) does not fit at any nesting" was misleading —
raising `MaxList` to 64 costs nothing by itself, as a direct test confirmed.
And readings that showed both forms failing at a 48-element list were
capacity exhaustion inside the machine (`static assertion failed` at any
depth), not depth exhaustion; the same artifact made an unimplemented `let`
look like a depth failure.

What survives: the loop keeps a constant where the alternative scales with
user data, against a ceiling that is a compiler flag rather than something
the library controls.

**On the spirit of D15.** The loop is inside a substrate generic algorithm,
which D15 permits. It is also not what D15 is aimed at: the target is
`if (!r.has_value()) return r;` ladders *in passes*, and this loop is the one
place that ladder is written so no pass needs one. The exemption is working as
intended here.

## Open questions

- ~~Does GCC 16 spend the same ~4 constexpr frames per element?~~ Answered
  2026-08-09: eight, and the whole-pipeline peak is 306 against the loop's 60
  on a 32-element list. Since both fit, whether the margin alone justifies
  keeping the loop is a judgement rather than a measurement, and is the one
  part of this item still genuinely open.
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
`apply` is now nested `bind`s instead of hand-written branches.

Verified on GCC 16 (`g++-16 16.0.1 20260322 trunk r16-8246`), 2026-08-09:
`make test-matrix` green in both configs — Debug 1066/1066 and Asan 1066/1066,
`MonadTest` among them — and `make lint` green after clang-format reflowed the
two new files. The `static_assert`s carry the real weight, since they pass by
virtue of compiling; the two `TEST_CASE`s add five runtime assertions.

Note that the 1066 includes the two new `MonadTest` cases, so the pre-existing
count was 1064; deriving `apply` from `bind` changed no test's outcome, which
is the result that matters — the hand-written applicative and the monad-derived
one agree on every case the suite already exercised.

The unlanded part is wider — five headers plus tests gain an include — but is
mechanical and fully checked by the compiler.

## Decision criteria

Schedule the remaining move when a step is already touching those five
headers' include lists, or when a second Monad instance makes the CPO earn its
keep. Close as declined if `result` stays the only monad in the tree, in which
case `and_then` on the concrete type is honest and the typeclass is ceremony.
