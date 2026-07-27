# DIV-0015: the sender backend has runtime evidence only, no `static_assert` twins

- **Status:** accepted-permanent
- **Date:** 2026-07-27
- **Step:** L21 (sender backend for the CL core)
- **Authority diverged from:** docs/cl-pivot-plan.md

## What diverged

The L11-L15 merge criteria are stated in the two closure backends as `static_assert`s over a compile-time evaluation of the program, each with a runtime `TEST_CASE` twin.
`eval_direct.test.cpp` and `cps_code.test.cpp` both carry that pair for every criterion.

The sender backend carries the runtime half only.
`sender_program.test.cpp` has 68 runtime `TEST_CASE`s and no `static_assert` over an evaluation.

## Why

Beman Execution's `connect`/`start` machinery is not constant-evaluable, and neither is `sync_wait`, which runs a `run_loop` over a mutex and condition variable.
A sender cannot be started during constant evaluation, so a sender backend cannot produce a compile-time value for a program, so there is nothing for a `static_assert` to assert.

This is not new to `smdlisp`.
The Scheme sender backends have the same property and the same runtime-only test suites (`sender_program.test.cpp`, `sender_mendler_program.test.cpp` under `src/smd/smdscheme/sender/`), which is where the shape was inherited from.
What is new is that L21's merge criterion is *parity with a test set that is stated as `static_assert`s*, so the gap has to be named rather than passed over.

Note the direction of the standing rule this does **not** violate.
The rule from L14 is "every `static_assert` needs a runtime `TEST_CASE` twin, because a constexpr-green mechanism can still be broken at run time."
That rule protects against trusting compile-time evidence alone.
Here there is no compile-time evidence to over-trust; the backend has only the kind of evidence the rule says to insist on.

## Consequences

- Parity for L11-L15 is claimed at the level of *program and expected value*, not at the level of *evaluation phase*.
  Every source string in `sender_program.test.cpp` is copied verbatim from `cps_code.test.cpp` or `eval_direct.test.cpp` so the claim is checkable by diffing the sources.
- DIV-0013 (a `static_assert` involving `compiled_lisp<Source>` failing under the default Asan config on GCC trunk) cannot bite this backend, since it has no such `static_assert`.
- Any future step that adds a merge criterion expressed only as a `static_assert` should not expect the sender backend to mirror it.
  Step L23 (`tagbody`/`go`) and step L24 (documentation consolidation) are the ones in range.
- `sender_program::run` and `sender_program::operator()` are deliberately not marked `constexpr`, unlike their closure counterparts, so the constraint is visible in the API rather than only in the tests.

## Revisit condition

Closed if Beman Execution gains constant-evaluable `connect`/`start` for inline, synchronous operation states, which would let the whole graph run during constant evaluation.
Nothing in this project can bring that about, and it is not obviously desirable, so this is recorded as accepted-permanent rather than open.
