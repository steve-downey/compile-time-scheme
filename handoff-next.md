# Next Agent Directions

Welcome, future coding agent!

## Current State
Step 30 ("Sender Adapter over Beman Execution") is successfully completed. We have constructed our namespace pipeline layer at `src/smd/schemepoc/sender_adapter.hpp` defining direct abstractions mapping into `beman::execution26`. Functions like `just`, `then`, `when_all`, `sync_wait`, etc., are now ready for immediate pipeline integration. A fully mocked test harness passing standard evaluated components up into pipeline executions asserts this behavior firmly. The codebase builds perfectly without warnings.

## Next Step
Your task is to implement **Step 31**: "sender backend over CPS program using Beman Execution".

## Instructions
1. We need to begin utilizing real evaluated AST nodes directly into our `sender_adapter` pipelines.
2. Based on the documentation (`schemepoc-plan.md`), the CPS defunctionalized syntax should branch dynamically over this sender pipeline.
3. Establish `src/smd/schemepoc/sender_backend.hpp` and its tests (`src/smd/schemepoc/sender_backend.test.cpp`). 
4. The goal is to evaluate defunctionalized `core_tree` instances returning `sender_adapter` sequences (`just`, `then`, `when_all`). Look at how current continuations execute, and construct matching interfaces taking `sender` flows rather than generic synchronous tail calls.
5. Do not decrease tests or regress coverage! Compile the examples and verify regressions are kept to zero. Ensure `make compile`, `make test`, and `make lint` remain strictly green.
6. Verify whether we require integrating Beman Task (Step 32) at all through these pipeline demands or if `sender` is adequately powerful. You do not have to perform step 32, simply leave a note marking its required status for the next executor.
7. Merge back onto `main`, mark step 31 checked in `checklist.md`, and leave instructions targeting Step 32 / Step 33 onwards in `handoff-next.md`.
