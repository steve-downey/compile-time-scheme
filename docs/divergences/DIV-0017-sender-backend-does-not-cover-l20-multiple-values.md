# DIV-0017: the sender backend does not cover L20 multiple values

- **Status:** open
- **Date:** 2026-07-27
- **Step:** L21 (sender backend for the CL core)
- **Authority diverged from:** docs/cl-pivot-plan.md

## What diverged

Step L21's stated dependency is L15, with "L16/L20 coverage may follow in-step or as a recorded gap with divergence doc".

L16 (special variables and dynamic binding) is covered in-step.
L20 (multiple values) is **not** covered, and this is the record of that gap.

## Why

L20 was being built in a sibling worktree while L21 was being built, and had not merged.
Nothing of it exists on this branch: the core AST (`elaborator/elaborated_core.hpp`) has no `values`/`multiple-value-bind` node kinds, the elaborator does not produce them, and `closure::value<Core>` is a single value with no n-ary form.
There was therefore nothing for the sender backend to port -- not a decision to skip work, but an absence of work to port.

Covering it speculatively would have been worse than not covering it.
The sender backend's carrier is `outcome<Core>`, which holds one `closure::value<Core>` on its value channel; widening that to an n-ary payload ahead of the design being settled would have meant guessing at L20's representation and then almost certainly disagreeing with it.

L16, by contrast, was already merged into this lane's branch, and its two helpers -- `closure::detail::bind_lambda_parameters` and `closure::detail::unwind_dynamic_bindings` -- are templates on the environment and arena types with no dependency on `eval_direct`.
The sender backend gets shallow binding by *calling* them, not by reimplementing them, which is why covering L16 was nearly free and covering L20 was not possible at all.

## Consequences

- The sender backend's value channel carries exactly one `closure::value<Core>`.
  Its completion signature set (`sender::lisp_completions`, in `defer_outcome.hpp`) names `set_value_t(closure::value<Core>)`, and both hand-written senders share that one set by construction.
  Adding multiple values means changing that alias and the two senders that use it, plus `outcome`, `outcome_receiver`, `detail::complete_with`, and the arms of `eval_algebra` that construct values.
- The gap is expected to be temporary rather than permanent.
  L20 is being re-dispatched immediately after this step merges, with n-ary CPS continuations carrying a `static_vector<value, MaxValues>` payload, per the plan's literal §L20 wording.
  That step is the natural place for the sender side of multiple values, since it will already be settling the representation; nothing in this step has been pre-built to accommodate it, deliberately.
- Until then, a program using `values` or `multiple-value-bind` will fail at elaboration under *all three* backends, not just this one, so there is no backend-parity discrepancy to explain to a user.
- Step L24 (documentation consolidation) should check whether this divergence has been closed by the re-dispatched L20 before describing the sender backend's coverage.

## Revisit condition

Closed when L20 lands multiple values in the core AST and the sender backend's `outcome` carries the same n-ary payload the closure backends' continuations do, with the L20 test set mirrored into `sender_program.test.cpp` the way the L11-L16 set already is.
