# Next step: Step 22 (function application)

## Goal

Now that we have `closure` values representing lambdas at runtime, we need to implement evaluating function application where the operator evaluates to a `closure`.

The goal is to update `eval_direct.hpp` to handle `core_application` when the operator evaluates to a `closure`, bind the arguments to the parameters, and evaluate the lambda's body.

Lexical capture (environments nested in closures) is deferred to Step 23. For Step 22, it is sufficient that applying a closure evaluates the body in an environment extended with the arguments.

## Constraints

- Update `eval_direct.hpp` for `core_application<MaxList>`. Currently, it assumes the function evaluates to a `builtin`. Add support for when the operator evaluates to a `closure`.
- When a `closure` is applied, retrieve the `core_lambda` node it points to in the `core_tree`.
- Check arity (number of provided arguments must match `lambda.params.size()`).
- Copy the current environment and define the evaluated arguments using the parameter names. (Full parent-linked lexical environments are Step 23, so just extending a copy of the current environment is sufficient).
- Evaluate the lambda's body with the extended environment.
- Add tests in `eval_direct.test.cpp` for evaluating something like `((lambda (x) (+ x 1)) 5)`.

## Files expected to change

- `src/smd/schemepoc/eval_direct.hpp` (add closure application handling)
- `src/smd/schemepoc/eval_direct.test.cpp` (add end-to-end tests for lambda application)
- `checklist.md` (check off step 22)
- `handoff-next.md` (prepare for step 23)
