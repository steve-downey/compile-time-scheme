# Next step: Step 23 (lexical closure capture)

## Goal

Currently, `closure` values only store the `node_id` of their `core_lambda` and do not capture their defining lexical environment. We need to implement lexical environment capture so closures can access variables bound outside their parameters.

## Details

- Update the `closure` type (in `value.hpp`) to store a copy of the lexical `env` present when the lambda was evaluated. Since `env` is currently templated on `MaxBindings` (often `16`), consider how to embed an environment within a `closure`. Since we are relying on fixed-capacity structures, adding `env<16>` to `closure` directly may be an acceptable first step if type dependencies align (or templating `closure` / `value` if necessary, though this cascades... a fixed type or pointer could work, but be mindful of `constexpr` allocation).
- Update `eval_direct.hpp`: when evaluating a `core_lambda`, capture the current `environment` into the created `closure`.
- When evaluating a `closure` application, the base environment should be the captured environment, NOT the caller's environment. Copy the closure's captured environment, extend it with the new evaluated arguments, and evaluate the lambda body within that extended environment.
- Add tests in `eval_direct.test.cpp` to verify lexical scope (e.g. `(((lambda (x) (lambda (y) (+ x y))) 3) 4)` should return `7`).

## Constraints

- Ensure the structures stay `constexpr`-friendly.
- Be careful with circular type dependencies between `value` and `env`. `env` stores `value`s; if `closure` is in `value` and requires `env`, you can forward-declare or restructure definition order.
- Maintain tests, keeping things green.

## Files expected to change

- `src/smd/schemepoc/value.hpp` (add captured env to closure, resolve loop)
- `src/smd/schemepoc/eval_direct.hpp` (capture env on creation, use captured env on apply)
- `src/smd/schemepoc/eval_direct.test.cpp` (add lexical capture tests)
- `checklist.md` (check off step 23)
- `handoff-next.md` (prepare for step 24)
