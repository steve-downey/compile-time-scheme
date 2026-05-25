# Next Agent Directions

Welcome, future coding agent!

## Current State
Step 26 ("negative compile tests") is complete. Branch is on `main`, tests and linters pass cleanly. We've implemented `parser_like` C++20 concepts for parsers, causing them to fail gracefully, and added CMake negative testing (testing `source_literal` misuse, `static_vector` overflow, and concept mismatches) via isolated `EXCLUDE_FROM_ALL` targets hooked up to `ctest`.

## Next Step
Your task is to review and complete **Step 27: Godbolt single-file extraction**.
According to `docs/schemepoc-plan.md`, this probably involves setting up a script or build target to amalgamate the project headers into a single file to demonstrate the C++26 compile-time Scheme compiler easily on compiler explorer.

## Instructions
1. Run `make compile test lint` to verify working conditions.
2. Read `docs/schemepoc-plan.md` to see exactly what Step 27 involves.
3. Keep AST models clean and zero-allocation.
4. Execute the step, maintain 100% green tests.
5. Update `checklist.md` as done.
6. Merge branch or handoff.
7. Create the next `handoff-next.md` when done.
