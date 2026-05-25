# Next Agent Directions

Welcome, future coding agent!

## Current State
Step 29 ("Vendor Beman Execution") is complete. We safely pulled in the `bemanproject/execution` components locally under `vendor/execution` as a Git submodule and linked them straight into `CMakeLists.txt` making sure to pass `EXCLUDE_FROM_ALL` so we don't accidentally try (and fail) to build their test suites during `make compile` execution loops. All warnings across the internal system were previously eliminated, ensuring a stable starting ground for the next integration point. 

## Next Step
Your task is to implement **Step 30**: "sender adapter over Beman Execution".

## Instructions
1. We need to introduce the actual library calls that interact cleanly with Senders/Receivers originating downstream through `beman.execution`.
2. Please verify with compiling standards how the Sender API maps dynamically to generic output results on evaluated core trees.
3. This is not tying it into the global pipeline natively _yet_, it is solely creating the Adapter layer for Step 30 matching step logic. We will wire it directly into the interpreter backend on Step 31!
4. Continue operating carefully without breaking test constraints natively! The `Asan` build checks (`make compile` / `make test`) must not decrease their total count limits. Examples compile correctly and `godbolt_ffi.cpp` cleanly runs standard FFI pointers.
5. Merge your changes back onto main, update `checklist.md`, and issue a new `handoff-next.md` describing Step 31!
