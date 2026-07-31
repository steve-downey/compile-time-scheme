# BL-0001: rewrite the elaborators as `fold_fix` algebras

- **Status:** scheduled — split across steps R1 and R4 of `docs/cl-rebuild-plan.md`
- **Date:** 2026-07-30, updated 2026-07-31
- **Origin:** the closing aside of the retired root-level `plan-refactor-arena-box.md` (deleted 2026-07-30; see `git log -- plan-refactor-arena-box.md`) — "since we have `fold_fix`, the elaborator can possibly be written as an algebra `DatumF<Core> -> Core`". Every other phase of that plan had shipped; this was the only part never attempted.
- **Frozen-tree impact:** none as of D11, which retires the D1 freeze.

## Resolution (2026-07-31)

Both halves are now scheduled, and the answer to each is the opposite of what the retired plan proposed.

**`fold_fix` is deleted, not repaired** (step R1).
It has no caller, no test, and an inverted `fmap` argument order.
`mendler_para` supersedes it, and DIV-0016 already established that the paramorphic form is the one this project needs.
Deleting it also removes the only untested API in `foundation/`, and under D11 that is an ordinary change rather than a divergence-doc event.

**The elaborator becomes a traversal, not a catamorphism** (step R4, decision D15).
`traverse` over the result applicative is the right shape: it replaces the `if (!r.has_value()) return r;` ladder that appears dozens of times, and `docs/CODING_RULES.md` already names `traverse` as the minimal Traversable operation.
A plain `fold_fix` catamorphism is the wrong scheme for the reasons in the analysis below, all of which still hold.

The rest of this document is retained as the reasoning that produced that conclusion.

## What

Replace the hand-written mutual recursion in the elaborators — `elaborate_node` / `elaborate_list` dispatching through `std::visit` — with a recursion scheme, so the traversal is written once and each core form contributes only its own algebra arm.

## Why a catamorphism is the wrong scheme

- **The algebra needs the whole node, not just its unwrapped layer.**
  DIV-0016 settled this for the sender backends: evaluating `core_lambda` stores a pointer to the node itself, so a paramorphism is required.
  The repo already has two Mendler-style `mendler_para` implementations for this reason, and unifying them is part of R1.
- **The datum tree is not a recursive value.**
  Children are `foundation::arena_box<R, MaxNodes>` handles (`reader/datum_type.hpp`), not inline `R`.
  Any traversal must thread the datum arena to resolve a handle, so the scheme needs a context parameter — exactly the `alg(recurse, ctx, tree, layer)` shape DIV-0016 describes, not `R(F<R>)`.
- **Elaboration is not compositional.**
  A catamorphism elaborates every child before the parent can inspect it, but several forms must read their children as *syntax*: `let`/`let*` binding lists are symbol/expression pairs, lambda parameter lists are names, and `elaborate_quoted_datum` treats an entire subtree as data.
  A bottom-up fold would elaborate `(n e)` as an application of `n` before the `let` arm ever saw it.
  This is the same reason real macro expanders are not catamorphisms.

R4 therefore targets the compositional fragment — expression positions — and leaves special-form syntax to explicit handling, rather than forcing the whole elaborator through one scheme.

## Evidence

All verified by inspection at commit `f47c037`.

- **`fold_fix` has zero call sites.**
  `grep -rn fold_fix src/smd/smdscheme src/smd/smdlisp` returns only its definition (`foundation/fix.hpp:49`), its own recursive call (`:51`), and a doc-comment mention in `sender/comp_tree.hpp:126` that does not call it.
  The `smd::fixpoint` playground has a separate, working `fold_fix` in `recursion_schemes.hpp`; that one is tested and is not this one.
- **`fold_fix` appears to be uninstantiable as written.**
  `fix.hpp:50` calls `fmap(tree.inner, lambda)`, but the CPO takes `(function, value)` (`foundation/functor.hpp`).
  With the arguments in that order the typeclass lookup is `functor_typeclass<lambda>`, which is `std::false_type`, and `std::false_type` has no `fmap`.
  Unqualified `fmap` inside `foundation` finds the CPO *variable*, so ADL never runs and no argument-dependent overload can rescue it.
  **Not compile-verified** — gcc-16 was unavailable in the environment where this was written, and nothing instantiates it.
  Confirm during R1 before deleting, so the deletion is recorded as removing dead code rather than as removing something that worked.
- **No Functor instance is registered for either AST layer.**
  The only `functor_typeclass` specialization in the tree is for `sender::string_writer` (`sender/string_writer.hpp:160`).
- **`fold_fix` is untested.**
  `foundation/fix.test.cpp` is a two-line stub — the path comment and the SPDX line — and `foundation/CMakeLists.txt` lists only `fix.hpp`.
  This is a real gap against the checklist's "every public constexpr API has a constexpr/static_assert test" ground rule, and it is why the argument-order defect went unnoticed.
