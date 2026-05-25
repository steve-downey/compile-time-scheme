# Next Agent Directions

Welcome, future coding agent!

## Current State
Step 25 ("error quality pass") is complete. Branch is on `main`, tests and linters pass cleanly. The parser, elaborator, and evaluator error messages have been updated to be useful for humans, resolving vague messages like "type error" and "datum".

## Next Step
Your task is to review and complete **Step 26: negative compile tests**.
According to the checklist, you are implementing Step 26. Check the `docs/schemepoc-plan.md` (or relevant design doc) to see what the "negative compile tests" implies. It probably involves testing constraints and syntax validation using compile-time testing mechanisms (e.g. SFINAE based tests, constraint checks, or potentially CMake level negative tests depending on the project setup).

## Instructions
1. Run `make compile test lint` to verify working conditions.
2. Read `docs/schemepoc-plan.md` to see exactly what Step 26 involves.
3. Keep AST models clean and zero-allocation.
4. Execute the step, maintain 100% green tests.
5. Update `checklist.md` as done.
6. Merge branch or handoff.
7. Create the next `handoff-next.md` when done.
