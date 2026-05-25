# Next step: Step 20 (lambda syntax)

## Goal

Add `lambda` syntax to the elaborator. We are not implementing closure values or lexical capture yet, just the structural elaboration of the form.

The reader already parses lists and symbols without problem.

## Elaboration rule

```txt
(lambda (param*) body) -> lambda node
```

Constraints:
- `params` must be a list of symbols.
- `body` is exactly one expression currently (no implicit `begin` yet, unless you want to add it).
- Duplicate parameter names should be rejected as an error.

## Files expected to change

- `src/smd/schemepoc/core_tree.hpp` (add `lambda_node` struct and ensure it's in the variant).
- `src/smd/schemepoc/elaborator.hpp` (recognize the `lambda` symbol and extract params and body).
- `src/smd/schemepoc/test/elaborator.test.cpp` or equivalent test file (tests for successfully validating or rejecting lambdas).
- `checklist.md` (check off step 20).
- `handoff-next.md` (prepare for step 21).

## Notes

- Remember to follow `AGENTS.md` and `docs/codestyle.org`.
- Create a feature branch and worktree if the implementation is non-trivial.
- Test thoroughly (e.g. valid single param, multiple params, no params, errors on duplicate param, error on non-symbol param, error on missing body).
