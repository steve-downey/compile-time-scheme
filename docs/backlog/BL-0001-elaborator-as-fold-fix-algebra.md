# BL-0001: rewrite the elaborators as `fold_fix` algebras

- **Status:** open
- **Date:** 2026-07-30
- **Origin:** the closing aside of the retired root-level `plan-refactor-arena-box.md` (deleted 2026-07-30; see `git log -- plan-refactor-arena-box.md`) — "since we have `fold_fix`, the elaborator can possibly be written as an algebra `DatumF<Core> -> Core`". Every other phase of that plan had shipped; this was the only part never attempted.
- **Frozen-tree impact:** yes for the Scheme side. `smdscheme/elaborator/elaborate.hpp` and `foundation/fix.hpp` are both inside `src/smd/smdscheme/**` (D1) and would need a divergence doc plus orchestrator sign-off. `smdlisp/elaborator/elaborate.hpp` is editable. **Prefer scoping any first attempt to the `smdlisp` side only.**

## What

Replace the hand-written mutual recursion in the elaborators — `elaborate_node`
/ `elaborate_list` dispatching through `std::visit` — with a recursion scheme,
so the traversal is written once and each core form contributes only its own
algebra arm.

Two separable pieces, and they should not be conflated:

1. **Decide the fate of `foundation::fold_fix`.** It is currently dead code
   (see Evidence). Either make it work and test it, or delete it. This is worth
   doing on its own merits regardless of piece 2.
2. **Consider a recursion scheme for the elaborators.** Almost certainly
   *not* `fold_fix` — see below.

## Why it is not done

The original aspiration is partly superseded by what step L21 learned, recorded
in `DIV-0016`. A plain catamorphism is the wrong scheme here:

- **The algebra needs the whole node, not just its unwrapped layer.**
  DIV-0016 settled this for the sender backends: elaborating/evaluating
  `core_lambda` stores a pointer to the node itself (`closure/cps_code.hpp`
  around the `core_lambda` arm), so a paramorphism is required. The repo already
  has two Mendler-style `mendler_para` implementations for this reason.
- **The datum tree is not a recursive value.** Children are
  `foundation::arena_box<R, MaxNodes>` handles (`reader/datum_type.hpp`), not
  inline `R`. Any traversal must thread the datum arena to resolve a handle, so
  the scheme needs a context parameter — which is exactly the
  `alg(recurse, ctx, tree, layer)` shape DIV-0016 describes, not `R(F<R>)`.
- **Elaboration is not compositional.** A catamorphism elaborates every child
  before the parent can inspect it, but several forms must read their children
  as *syntax*: `let`/`let*` binding lists are symbol/expression pairs, not
  expressions (`elaborate.hpp` `let` and `let*` arms); lambda parameter lists
  are names; and `elaborate_quoted_datum` treats an entire subtree as data. A
  bottom-up fold would elaborate `(n e)` as an application of `n` before the
  `let` arm ever saw it. This is the same reason real macro expanders are not
  catamorphisms.

So the honest version of this item is narrower than the retired plan implied:
either apply the *existing* `mendler_para` to the compositional fragment
(expression positions), leaving special-form syntax handled by explicit
recursion, or accept the hand-written traversal as correct and close this as
declined.

## Evidence

All verified by inspection at commit `f47c037`:

- **`fold_fix` has zero call sites.** `grep -rn fold_fix src/smd/smdscheme
  src/smd/smdlisp` returns only its definition (`foundation/fix.hpp:49`), its
  own recursive call (`:51`), and a doc-comment mention in
  `sender/comp_tree.hpp:126` that does not call it. The `smd::fixpoint`
  playground has a separate, working `fold_fix` in `recursion_schemes.hpp`; that
  one is tested and is not this one.
- **`fold_fix` appears to be uninstantiable as written.** `fix.hpp:50` calls
  `fmap(tree.inner, lambda)`, but the CPO takes `(function, value)`
  (`foundation/functor.hpp` `fmap_fn::operator()`). With the arguments in that
  order the typeclass lookup is `functor_typeclass<lambda>`, which is
  `std::false_type`, and `std::false_type` has no `fmap`. Unqualified `fmap`
  inside `foundation` finds the CPO *variable*, so ADL never runs and no
  argument-dependent overload can rescue it. **Not compile-verified** — gcc-16
  was unavailable in the environment where this was written, and nothing
  instantiates it, so it is a read of the code, not an observed error. Confirm
  before acting.
- **No Functor instance is registered for either AST layer.** The only
  `functor_typeclass` specialization in the tree is for
  `sender::string_writer` (`sender/string_writer.hpp:160`). `comp_tree.hpp`
  provides `fmap_comp` as a plain free function, not a registered instance.
  Both `datum_f_factory::type` and `core_f_factory::type` would need instances.
- **`fold_fix` is untested.** `foundation/fix.test.cpp` is a two-line stub — the
  path comment and the SPDX line, nothing else — and `foundation/CMakeLists.txt`
  lists only `fix.hpp`. This is a real gap against the checklist's "every public
  constexpr API has a constexpr/static_assert test" ground rule, and it is why
  the argument-order defect above has gone unnoticed.

## Open questions

- Is `fold_fix` worth keeping at all, given `mendler_para` covers the cases the
  repo actually has? Deleting it would remove the only untested API in
  `foundation/` — but it lives in the frozen tree, so even deletion needs D1
  sign-off.
- Can the elaborator's compositional fragment be separated cleanly from
  special-form syntax handling, or does the split leave both halves harder to
  read than today's single traversal? Write the `if`/application/`prim` arms as
  an algebra first and judge from that.
- Does this interact with DIV-0016's revisit condition? Generalising
  `smd::fixpoint::mendler_para` to accept any type exposing one functor layer
  would retire one of the two duplicate implementations and might make this item
  cheap. Sequencing that first may be the better order.
- Do both front ends have to move together, or can `smdlisp` diverge
  structurally from the frozen Scheme elaborator? The latter is allowed but
  creates a second shape to explain in `docs/compiler_architecture.org`.

## Cost and risk

Piece 1 (`fold_fix`: fix-and-test or delete) is small — well under a step — but
gated on a D1 divergence doc.

Piece 2 is a step's worth of work per front end, touching the most
semantically dense file in each tree. The elaborators are covered by
`elaborate.test.cpp` plus every downstream backend test, so regressions should
surface; the risk is not silent breakage but churn with no payoff. Rewriting a
correct, working traversal into a recursion scheme is only worth it if it makes
adding the *next* core form cheaper, and the expression problem here was already
resolved the other way on purpose — `core_prim` deliberately uses one node with
a runtime `prim_op` switch to keep the number of `std::visit` arms small
(`elaborated_core.hpp`, `core_prim` doc comment).

## Decision criteria

- **Schedule** if a step needs to add several new core forms at once and the
  per-form `std::visit` arm cost across six evaluators becomes the bottleneck.
- **Schedule piece 1 independently** if the `fold_fix` argument-order defect is
  confirmed by compilation — shipping a public API that cannot instantiate is
  worth fixing or removing on its own.
- **Decline** if the compositional fragment turns out to be a minority of the
  elaborator, which the `let`/`let*`/quote/lambda arms suggest it may be. In
  that case delete `fold_fix`, note the traversal as deliberately hand-written
  in `docs/compiler_architecture.org`, and close this item.
