# DIV-0020: the multiple-value operator set is `values` and `multiple-value-bind`, and values are capped

- **Status:** open
- **Date:** 2026-07-27
- **Step:** L20 (multiple values)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

ANSI CL's multiple-value facility is a family: `values`, `values-list`, `multiple-value-bind`, `multiple-value-list`, `multiple-value-call`, `multiple-value-setq`, `multiple-value-prog1`, and `nth-value`.

This step implements two of them -- `values` and `multiple-value-bind` -- which are exactly the two `docs/cl-pivot-plan.md` step L20 names.
The other six are unbound: `(multiple-value-list (values 1 2))` is an undefined-function error, not a list.

ANSI CL also requires an implementation to accept at least 20 values from a single `values` form (`multiple-values-limit` >= 20).
Here the cap is `closure::default_max_values`, which is **8**.
`(values 1 2 3 4 5 6 7 8 9)` is a diagnosed error ("values: more values than MaxValues"), not a nine-value return.

## Why

The operator set is the plan's scope decision, not a discovery, and the four omitted *forms* are each a small amount of new elaborator work rather than new semantics -- the n-ary channel this step built is what any of them would need, and it now exists.

The cap is a deliberate space trade.
`MaxValues` is the width of the `static_vector` payload every continuation and every recursive evaluator frame carries, in both closure backends, in `constexpr` evaluation as well as at run time.
Eight covers every program in this project with room to spare, and raising it to 20 to satisfy the letter of the standard would widen a payload that appears in every frame of a tree-walking interpreter to buy nothing this project can use.
`MaxValues` is a defaulted template parameter on `eval_direct_values`, `apply_function_value_values`, `cps_dispatch`, `cps_apply_function_value`, `cps_of` and `compile_cps`, so a caller who needs 20 can ask for 20 without touching the headers; only `closure_program`/`lisp_program` pin the default.

Note also that the cap interacts with `MaxList`: `values` receives its arguments through the ordinary call path, so a call with more than `MaxList` arguments fails earlier and for a different reason.

## Consequences

- A program written against real CL that uses `multiple-value-call` or `nth-value` fails as an undefined function, with no hint that the facility is partial.
- `values: more values than MaxValues` is reachable from user code, so it is a real diagnostic and not an internal assertion.
- Step L24 (documentation consolidation) should list the supported operator set explicitly rather than saying "multiple values are supported".
- A later step adding `multiple-value-list` / `multiple-value-call` needs no new evaluator machinery: `multiple-value-list` is `core_multiple_value_bind`'s value-form handling plus a list construction, and `multiple-value-call` is the same handling feeding an argument spread.

## Revisit condition

Closed when the remaining operators are implemented, or accepted-permanent if the project decides the two-form subset is the intended teaching surface.
The cap is separately closed by raising `default_max_values` to at least 20, which is a one-line change whose only cost is frame size.

## Classification (2026-07-31, rebuild phase R0)

Appended, not edited in place, per the append-only rule for these docs.

**Class: mixed.**
The two-form operator subset is a `scope-decision` and may be pinned by tests.
The cap of 8 is a `defect`: ANSI requires `multiple-values-limit` to be at least 20, so this is nonconformance rather than scope.
`EvalDirectTest - MoreValuesThanMaxValuesIsDiagnosed` pins the defect half and must not be carried forward.
