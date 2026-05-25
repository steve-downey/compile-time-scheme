# Next step: Step 21 (runtime closure values)

## Goal

Now that `lambda` syntax is elaborated and can produce `core_lambda` AST nodes, we need to introduce a runtime value to represent a lambda at runtime.

This step does *not* include full function application or lexical capture (those are steps 22 and 23). The sole objective is to model the `closure` value and have evaluation of a `core_lambda` node yield this value.

## Constraints

- Update `smd::schemepoc::value` in `src/smd/schemepoc/value.hpp` to include a `closure` type.
- The `closure` type needs enough information to reference the lambda parameters and body (e.g., holding the `node_id` of the `core_lambda` node).
- Lexical capture is step 23, so you do not need to capture the runtime environment yet; an empty capture or no capture is acceptable for this step.
- Update interpreters (`eval_direct.hpp`, etc.) so that evaluating a `core_lambda` yields a `closure` value instead of returning "unsupported form".
- Create testing ensuring evaluation correctly results in a closure value type.

## Files expected to change

- `src/smd/schemepoc/value.hpp` (add `closure` struct and add to `value` variant)
- `src/smd/schemepoc/eval_direct.hpp` (evaluate `core_lambda` -> `closure`)
- `src/smd/schemepoc/value.test.cpp` or `eval_direct.test.cpp` (add tests)
- `checklist.md` (check off step 21)
- `handoff-next.md` (prepare for step 22)
