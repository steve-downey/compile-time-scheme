# step-brief (Track C) — no successor step

Forward-only brief for the next clean agent on **Track C**. Bounded; not a log.

**Track C has no successor.** L20 (multiple values) was its last step and is
merged. L23 (`tagbody`/`go`) is optional and unassigned; L24 (documentation
consolidation, + phase 22 draft) is the only remaining unchecked work in
`checklist.md`, and it is a whole-project step, not a Track C continuation.
If the orchestrator dispatches L24, read this file for what L20 left it, then
delete this file as part of that step.

## What L24 must account for from L20

### The core AST gained one node kind, and lost the one you might expect

`elaborator::core_multiple_value_bind<R, MaxNodes>` (anchor
`98a22b58-964f-4853-be88-20a1d4a9b9d6` in `elaborated_core.hpp`) is the only
new variant alternative. There is deliberately **no** `core_values` node:
`values` is a *function* in ANSI CL, so it is an ordinary builtin
(`closure::builtin_op::values`, installed as `VALUES` by all three
`default_env` overloads) and `(values 1 2)` elaborates to a plain
`core_application`. That is what makes `#'values`, `(funcall #'values 1 2)`
and `(apply #'values '(1 2))` work, and all three are tested.

`multiple-value-bind` stays a special operator because it is a *binding*
form; it lowers to a real `core_lambda` that the evaluators **call**, which
is how it inherits implicit progn, arity checking, closure capture, and
L16's dynamic-binding rule without restating any of them.

### The evaluators' return channel is n-ary

`closure::value_list<Core, MaxValues>` = `static_vector<value<Core>,
MaxValues>`, with `default_max_values == 8` (anchor
`a4a90af9-fd64-4f75-ad47-609c7c1e50fd` in `value.hpp`, alongside
`primary_value` and `single_value`). Each of the two closure-backend
operations is now a pair, `_values` primitive + single-value adapter:

- `eval_direct_values` / `eval_direct`
- `apply_function_value_values` / `apply_function_value`
- CPS: `cps_dispatch` and `cps_apply_function_value` invoke continuations
  with a `value_list`; `detail::identity_k<Core, MaxValues>` takes one.
  `closure_program` gained `call_values`, and `operator()` is the adapter
  over it.

The pre-L20 single-value signatures are unchanged, which is why
`macroexpand/expander.hpp` needed no edit at all.

`MaxValues` is a **defaulted** template parameter (after `MaxEnvs`, before
the deduced `Cont`/`K`) on `eval_direct_values`, `apply_function_value_values`,
`cps_dispatch`, `cps_apply_function_value`, `cps_of` and `compile_cps`, so
every pre-existing four-argument explicit instantiation still compiles.

### Three divergences to describe, not to re-derive

- **DIV-0018** — the sender backend diagnoses `values` and
  `multiple-value-bind` rather than supporting them. Two arms were added to
  `sender_eval.hpp` (one `builtin_op` case, one variant arm), both returning
  an error on the error channel, and two tests in `sender_program.test.cpp`
  pin the messages. **DIV-0017 (L21's record of the same gap from the other
  side) is still open**; closing one closes both.
- **DIV-0019** — `return-from`/`throw` carry only the primary value, because
  `exit_record`/`catch_record` payloads are single-valued. Everything else
  (`if`/`progn`/lambda-body tail positions, `unwind-protect`'s protected
  form, `funcall`/`apply`) carries all of them.
- **DIV-0020** — the operator set is exactly `values` and
  `multiple-value-bind`; `multiple-value-call`/`-list`/`-setq`/`-prog1`,
  `values-list` and `nth-value` are absent. `default_max_values` is 8, below
  ANSI's required minimum of 20.

The honest one-line summary for prose: *multiple values propagate through
returns, not through escapes, and not into the sender backend.*

### Anchors added in this step

`a4a90af9-...` (`value.hpp`: `value_list`/`primary_value`/`single_value`),
`98a22b58-...` (`elaborated_core.hpp`: `core_multiple_value_bind`),
`4f5a55a0-...` (`eval_direct.hpp`: the four-entry-point forward declaration
block, which is where the `_values`/adapter convention is explained). All
three are real `uuidgen` output and exist at this step's merge commit, so a
phase-22 post pinned to `blog/phase-22` can transclude them.

## Standing constraints (unchanged)

- `src/smd/smdscheme/**` is frozen (D1) — read-only reference, never edit.
- UUID anchors: see the anchor section in `AGENTS.md`. Inside
  `src/smd/smdlisp/**` the *contents* of an anchored region may be edited;
  only deleting or nesting an anchor pair is forbidden. L20 edited inside
  `1d30e953` and `e6a2c1de` (`eval_direct.hpp`) freely, which is correct and
  is what the two previous over-broad readings of this rule got wrong.
- `docs/compiler_architecture.org` is still smdscheme-only by design; L24 is
  the step that opens the `smdlisp` section. L20 deliberately added nothing
  to it.
- C++26/GCC16 baseline; Catch2; suite is **792 tests** as of the L20 merge.
- File a DIV under `docs/divergences/` for any knowing ANSI CL deviation.
  **DIV-0020 is the highest used**; re-check the directory before filing.
- Before handoff: `make compile`, `make test`, `make lint`.
