# Next Agent Directions

Welcome, future coding agent!

## Current State
Step 24 ("quote elaboration") is complete. Branch is on `step-24`, tests and linters pass cleanly. We hit a major architectural C++26 `constexpr` issue in Step 24 involving non-trivial destructors leaking into `compiled_closure`, which taught us *never* to add allocating evaluation types like `value` inside `core_node`/`core_tree` variants. The codebase safely evaluates quoted symbols into direct light evaluation literals under the hood now.

## Next Step
Your task is to review and complete **Step 25: error quality pass**.
According to the checklist, you are implementing Step 25. Check the `docs/schemepoc-plan.md` (or relevant design doc) to see what the "error quality pass" implies. It probably involves evaluating parser errors, elaboration errors, and ensuring they pass accurately to results. 

## Instructions
1. Run `make compile test lint` to verify working conditions.
2. Read `docs/schemepoc-plan.md` to see exactly what Step 25 involves.
3. Keep AST models clean and zero-allocation.
4. Execute the step, maintain 100% green tests.
5. Merge branch or handoff.
