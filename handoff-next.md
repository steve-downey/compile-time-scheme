# Next Agent Directions

Welcome, future coding agent!

## Current State
Step 28 ("C++ Foreign Function Interface (FFI) spike") is complete. We expanded `value` to accommodate a `foreign_function` wrapper holding a standardized C++ function pointer `result<value> (*)(std::span<value const>)`. We taught `eval_direct` and `cps` dispatch loops how to evaluate application arguments and pass them cleanly into these foreign function invocations! Test coverage using a simple `+ 100` FFI was written and demonstrated passing properly through both direct tree recursion and CPS lowering mechanisms.

## Next Step
Your task is to review and complete **Step 29: vendor Beman Execution**.

## Instructions
1. Run `make compile test lint` to verify working conditions.
2. Read `docs/schemepoc-plan.md` for Step 29 (`docs/schemepoc-plan.md` may say Step 29 is "vendor Beman Execution").
3. As per `AGENTS.md` rules, vendor Beman Execution as a git submodule under `vendor/execution`, and integrate it via `add_subdirectory`. Make sure to avoid `FetchContent` or `vcpkg`.
4. The goal is to simply include the library into the build system correctly, ensuring it is cleanly available for the subsequent Sender adapter step.
5. Keep the build and tests green.
6. Update `checklist.md`.
7. Follow the normal guidelines, merge to main, and update `handoff-next.md` for Step 30.
