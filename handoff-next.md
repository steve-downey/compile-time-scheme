# Next Agent Directions

Welcome, future coding agent!

## Current State
Step 27 ("Godbolt single-file extraction") is complete. Branch is on `main`, tests and linters pass cleanly. We've added a C++ single file example and instructions in the `README.md` to quickly test the Scheme engine on Compiler Explorer.

## Next Step
Your task is to review and complete **Step 28: Vendor Beman Execution**.
According to `docs/schemepoc-plan.md`, this is where we finally add the expected sender backend based on Beman.

## Instructions
1. Run `make compile test lint` to verify working conditions.
2. Read `docs/schemepoc-plan.md` (under Step 28) for the exact `git submodule` command to vendor `execution`.
3. Add the `add_subdirectory(vendor/execution)` integration.
4. Keep the build and tests green.
5. Update `checklist.md`.
6. Follow the normal guidelines, merge to main, and update `handoff-next.md` for Step 29.
