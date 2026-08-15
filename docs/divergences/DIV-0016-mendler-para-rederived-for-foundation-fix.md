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

## 2026-08-06: R5 settles the evaluation half — the machine, not a Mendler fold

The one client the 2026-08-01 note left open — selective descent,
evaluation — landed in R5 as a defunctionalised-CPS abstract machine
(`src/smd/cl/eval/machine.hpp`), so the rebuild tree now contains no
Mendler scheme at all: `cata`/`para` carry elaboration, and evaluation
declined the fold entirely, because each `recurse` in a Mendler
interpreter is a C++ activation record, and the rebuild requires depth
and steps to be diagnosed capacities under constant evaluation
(`limits::frames`, `limits::steps`; phase 28 § "Evaluation is not a
fold").
One prediction on file is now obsolete rather than pending, and it is
recorded here because the document carrying it does not say so.
`docs/fixpoint-mendler-reference.md` § "Applicative Parallelism
Preserved" predicts that a CPS trampoline linearizes the independent
argument structure `Fix<CompF>` preserves for `when_all`. R5's
evaluator is that trampoline, so the prediction appears to name it. It
does not: the claim holds where children are reached through separate
`Box`es, and inverts on a columnar tree, where a branch's whole child
list is one `static_vector` that `machine::evaluate_subforms` holds at
once. `k_arguments` evaluates left to right by choice of frame, not
because the structure was erased. That whole reference doc describes
the `Fix`/`Box`/`Comp` foundation, which `src/smd/cl/` does not have;
it should be read as history for the rebuild tree.
The revisit condition above is unchanged.

## 2026-08-15: R7 settles the sender-backend shape — driving the machine, not forking it

Step R7 (`docs/cl-rebuild-plan.md` D17) had to choose a shape for the sender
backend before writing one, and the 2026-08-06 note above is exactly what
made the choice non-obvious: it says the columnar tree *inverts* the
"CPS trampoline linearizes independence" claim, which reads as an argument
*for* forking `k_arguments` into a `when_all` over sibling senders, the way
the pivot's `sender_eval.hpp` forks `comp_apply`'s argument list. R7 did not
take that path. It is recorded here rather than only in `machine_sender.hpp`
because this is where a future reader will land looking for "didn't the
2026-08-06 note argue for forking arguments?" and the answer needs to be
next to that argument, not only next to the code.

The reason is a concrete cost, not a re-litigation of the inversion claim
(which stands: a branch's whole child list genuinely is one `static_vector`,
visible at once). `eval::machine` is single-owner state — one frame stack
(`konts_`), one heap (`store_`), one control register
(`control_`/`env_`/`pending_`), all mutated in place by `step`. Forking an
argument list into sibling senders needs each sibling to make independent
progress, and nothing about that state supports two siblings advancing it at
once: a `when_all` over `machine`-internal continuations would need either a
second frame stack per sibling (at which point the siblings are not really
inside *this* machine any more, they are separate machines evaluating
sub-terms, with all the environment-sharing questions that raises for
`setq`/`defun` visibility) or a recursive call back into a fresh evaluator
for each argument, built in C++ recursion rather than the frame stack. The
second option spends exactly the property R5 was built to buy: no evaluation
recursion is C++ recursion, so `limits::frames` and `limits::steps` are
diagnosed capacities rather than stack depth. `limits::steps` defaults to
200000; rebuilding a Mendler-shaped recursive evaluator on the sender side to
get `when_all` over one call's arguments would make 200000 steps into 200000
C++ activation records in the one place R5 spent a whole step removing them.

R7 chose the other shape instead: `smd::cl::sender::machine_sender`
(`src/smd/cl/sender/machine_sender.hpp`) treats one whole `machine::run` as
the unit a sender completes. `start` drives the machine's own trampoline —
unchanged, still one loop, still bounded by the same `limits` — to
completion, then dispatches the resulting `eval::outcome` onto
`set_value`/`set_error`/`set_stopped`, D13's mapping spent a second time.
Nothing in `eval::machine`, `eval::outcome`, `eval::value` or `eval::heap`
changed; no seam needed exposing. This was not underdetermined between the
two shapes — the recursion-depth cost above is a real, checkable difference,
not a preference — so no escalation was needed to pick it.

What this shape still lets `when_all` say honestly: two whole-program
evaluations, each its own machine and its own heap with no shared mutable
state, are genuinely independent senders, and joining them is a true
structural-independence claim (`machine_sender.test.cpp`'s
`WhenAllJoinsTwoIndependentPrograms`). What it does not let `when_all` say,
and what R7's step brief was explicit must not be claimed: anything about
the evaluation order of one program's arguments, which stays exactly what
ANSI 3.1.2.1.2.3 requires and what `k_arguments` already gave it.

The revisit condition above is unchanged: this note does not reopen the
Mendler-scheme question, only records why R7's sender backend did not reopen
it either.
