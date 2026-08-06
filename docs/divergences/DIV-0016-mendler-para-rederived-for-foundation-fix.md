# DIV-0016: `mendler_para` re-derived locally instead of reusing `smd::fixpoint`'s

- **Status:** open
- **Date:** 2026-07-27
- **Step:** L21 (sender backend for the CL core)
- **Authority diverged from:** docs/cl-pivot-plan.md

## What diverged

The plan's §L21 says the sender backend should use `smd::fixpoint`'s `mendler_para` "where the Scheme side does".

`smd::smdlisp::sender::detail::mendler_para` (in `src/smd/smdlisp/sender/sender_eval.hpp`) is a local re-derivation of it, not a call to it.
It is the same recursion scheme with the same Mendler-style algebra signature `alg(recurse, ctx, tree, layer)`; it is eight lines, and it exists only because the original cannot be applied to the `smdlisp` core AST.

## Why

`smd::fixpoint::mendler_para` takes a `smd::fixpoint::Fix<F> const &`.

The `smdlisp` core AST is not a `Fix`.
`elaborator::core_type<MaxNodes, MaxList>` is `smd::smdscheme::foundation::fix<core_f_factory<...>::type>`, a *different* fixed-point type that happens to have the same shape -- one member holding one unwrapped functor layer -- but is nominally distinct and lives in a different namespace with a different unwrapping convention (`.inner` directly, versus `Fix`'s `wrap_fix`/`unwrap_fix` free functions).

There are three places the mismatch could have been fixed, and two of them are closed:

- **`smd::smdscheme::foundation::fix`** could gain a conversion to `Fix`.
  That tree is frozen for semantic changes under decision D1.
- **`smd/fixpoint/recursion_schemes.hpp`** could gain an overload on `foundation::fix`.
  That is outside this step's lane (`elaborator/**`, `closure/**`, `sender/**`), and it would also make the general-purpose `smd::fixpoint` library depend on the `smdscheme` foundation, which inverts the intended dependency direction.
- **The sender backend** could carry the bridge itself, which is what was done.

The Scheme side does not hit this because its sender/mendler backends operate on `Comp<MaxList>`, a purpose-built `smd::fixpoint::Fix` computation tree produced by `core_to_comp` (`smdscheme/sender/comp_tree.hpp`).
Building the `smdlisp` equivalent -- a second full representation of all twenty core node kinds, with `Box` children instead of arena handles, plus the conversion and its error paths -- is a step's worth of work on its own and buys nothing this step needs: the Mendler algebra can walk the arena-backed core directly, since the arena is already in the context the algebra is threaded.

Two deliberate differences from the original, neither semantic:

- it is parameterised on the concrete fixed-point type rather than on `fix<F>`, because deducing a template template argument that is a dependent alias template -- which `core_f_factory<MaxNodes, MaxList>::type` is -- is exactly the kind of deduction that does not survive contact with a real compiler;
- it reads the layer as `tree.inner` rather than through `unwrap_fix`, because `foundation::fix` has no unwrapping free function.

The paramorphic (rather than catamorphic) variant is the right one for the same reason it is on the Scheme side: the algebra needs the whole node as well as its unwrapped layer, because evaluating `core_lambda` stores a pointer to the node itself in the resulting closure.

## Consequences

- There are now two `mendler_para`s in the repository with the same semantics and different parameter types.
  Anyone changing the recursion scheme must change both, or explicitly decide not to.
- Step L24 (documentation consolidation) should record the two fixed-point types as a known duplication in `docs/compiler_architecture.org`, since it is the sort of thing that reads as an accident when found later.
- If a future step does build a `smdlisp` `Comp` tree -- for a defunctionalised or reflected backend, say -- the local bridge becomes dead and should be deleted rather than left as a second way to do it.
- This does not affect the observable behaviour of any program.
  It is a structural divergence only.

## Revisit condition

Closed when either fixed-point type is retired in favour of the other, or when `smd::fixpoint::mendler_para` is generalised to accept any type exposing one functor layer (a concept rather than a concrete `Fix<F>`), at which point the local copy can be deleted and the call site pointed back at the library.

## 2026-08-01: R3's schemes supersede this for the rebuild tree

`src/smd/cl/foundation/tagged_tree_schemes.hpp` lands `cata`/`para` and
`scan_down` as linear folds over the columnar tree, and deliberately no
Mendler variant: over a materialized column the children's results are
computed by position before the parent is visited, so the recurse-knob
this record argues for has nothing left to turn there. `para` carries
the tree and the node's own index, which is this record's actual driver
— the algebra that needs the node itself. The two `mendler_para` copies
this record describes remain what the frozen trees use.
