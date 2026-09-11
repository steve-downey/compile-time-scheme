# BL-0010: `derive_monad`'s derived `fmap` captures by reference

- **Status:** open
- **Date:** 2026-09-11
- **Origin:** found while carrying `main`'s typeclass rename onto the parser-combinator branch. `parser<F>` is the tree's first deferred-continuation Monad instance, and it is what turns this from a latent shape into a reproducible failure.
- **Frozen-tree impact:** none. `src/smd/kit/foundation/monad.hpp` is a live-tree file.

## What

`derive_monad`'s derived `fmap` builds its continuation with a by-reference
capture:

```cpp
// src/smd/kit/foundation/monad.hpp:105
return impl_of(self).bind(std::forward<MA>(ma), [&](auto &&a) {
    return impl_of(self).pure(std::invoke(f, std::forward<decltype(a)>(a)));
});
```

For a **strict** instance this is correct and costs nothing: `bind` invokes the
function before returning, so `f` and `self` are alive throughout. For a
**deferred** instance — one whose `bind` stores the continuation inside the
value it returns, to be run later — the references outlive the call that made
them.

Decide whether the derivation should capture by value instead, and whether the
same change is owed at the two other sites listed under Evidence. The file is
`src/smd/kit/foundation/monad.hpp`; `monad.test.cpp` would gain the pinning
test.

## Why it is not done

Nothing in the kit is broken today. Every instance registered on `main` —
`result`, `static_vector`, `identity`, `tagged_tree` — is strict, so no
in-tree caller can reach the dangling case. The one instance that can is
`parser<F>`, which lives on the `cl-parser-combinators` branch and has already
been fixed there locally, by the route `derive_monad`'s own documentation
prescribes: the instance supplies a native `fmap` and the base's
probe-before-derive prefers it.

So this is a question about the **base**, not a live failure: should a
derivation that the documentation says a deferred instance must override
instead be written so that it does not need overriding? That is a design call
about what `derive_monad` promises, and it wants deciding rather than patching.

## Evidence

**The hazard is already recognised in this file.** `then` guards against
exactly it, and says so, at `src/smd/kit/foundation/monad.hpp:148`:

> `second` is captured by value rather than by reference: a deferred instance
> could outlive this call, and DIV-0007 is what a captured reference costs
> when it does.

`then` captures `second` by value at lines 160 and 175. `fmap` at line 105 does
not. The reasoning was applied at one site and not the other; that asymmetry is
the whole of this item.

**The failure, reproduced three ways** against `parser<F>` on
`cl-parser-combinators`:

- ASan (`-O3 -fsanitize=address,undefined,leak`) reports **stack-use-after-return**, pointing at `monad.hpp:105`.
- Constant evaluation rejects it: *accessing `<anonymous>` outside its lifetime*.
- A `static_assert` in `parser_instances.test.cpp` fails to compile when the native `fmap` is removed.

The third is the strongest witness and the one that matters: the defect is
caught at compile time in a `constexpr` context, not only under a sanitizer at
run time.

**A complete audit of the capture lists in `derive_monad`**, by line:

| line | operation | captures | verdict |
| --- | --- | --- | --- |
| 105 | `fmap` | `[&]` — `f` and `self` | **defective for a deferred instance; demonstrated** |
| 160, 175 | `then` | `second` by value | correct, and deliberately so |
| 202 | `kleisli` | `f`, `g` by value; `&self` | formally the same defect, not demonstrated |
| 237, 238 | `apply` | `&self`, `&argument_value`, `&function` | same shape as `fmap`, not demonstrated |
| 127, 136 | `join` | captureless | correct |

`apply` was not reachable from `parser<F>` — its *requires*-clause needs
`element_type_t<MA>`, which `parser<F>` has no way to supply — so its by-
reference captures of `argument_value` and `function` are **untested, not
proven safe**. A deferred instance that does have an `element_type` would meet
the same failure there.

`kleisli`'s `&self` is a reference to a typeclass object, and every typeclass
object in this kit is stateless and empty (see the `as_functor` comment at
`monad.hpp:248`). The dangle is formal rather than observable. It is listed for
completeness, not as a second bug.

**What is not affected.** `parser<F>` satisfies neither `monad_impl` nor
`monad_object`, structurally: `parser<F>::operator()` is a template over the
threaded context, so the parsed value type is not a property of the parser type
and no `value_type` alias could name it. `result` satisfies both, so the
concepts work as intended; it is `parser` that cannot be described by them.
That is a separate observation and not a defect.

## Open questions

- Should the derivation capture by value, or should the documentation's "supply it yourself" remain the answer for deferred instances?
- If by value: does it cost anything for the strict instances, which are all of today's callers? A by-value capture of `f` is a copy per `fmap` call that is currently elided.
- Does `apply` get the same treatment, on the same argument, before an instance exists that can reach it? Fixing an undemonstrated site is cheap here and expensive once a caller depends on the current shape.
- Is `kleisli`'s `&self` worth changing at all, given every typeclass object in this kit is empty?
- Does anything else in `src/smd/kit/foundation/` derive through a stored continuation? `applicative.hpp`, `traversable.hpp` and `foldable.hpp` were not audited for this.

## Cost and risk

Small — a capture list and a test, if the answer is "capture by value". The
audit above is the expensive part and it is done.

The risk is the reverse of the usual one: leaving it alone costs nothing until
the day a second deferred instance is registered, at which point it costs
whatever that instance's author spends rediscovering this — for the parser, an
ASan report to read and a derivation to trace back into the base.

Re-verify: `monad.test.cpp`, `applicative.test.cpp`, and the instance tests for
`result`, `static_vector`, `identity` and `tagged_tree`.

## Decision criteria

Schedule it when a second deferred-continuation instance is proposed for the
kit, or fold it into whatever step next edits `derive_monad` — it is a few
lines and the argument is already written.

Close it as declined if the position is that `derive_monad` is a strict-instance
derivation by design, in which case the documentation should say so where it
currently says a deferred instance "has to supply these operations itself":
name the operations, rather than leaving the reader to find out which.
