# Next Agent Directions

Welcome, future coding agent!

## Current State
Step 27 ("Godbolt single-file extraction") is complete. We also injected a new step for a C++ FFI spike in `docs/schemepoc-plan.md` right after Step 27.

## Next Step
Your task is to review and complete **Step 28: C++ Foreign Function Interface (FFI) spike**.

## Instructions
1. Run `make compile test lint` to verify working conditions.
2. Read `docs/schemepoc-plan.md` (under Step 28). We need to extend `value` with a `foreign_function` variant that holds a pointer `ffi_fn = result<value> (*)(std::span<value const>)`.
3. Update CPS `core_application` to dispatch to this pointer if the operator reduces to a `foreign_function`. Add a test where an FFI function is registered and invoked from Scheme code.
4. Keep the build and tests green.
5. Update `checklist.md`.
6. Follow the normal guidelines, merge to main, and update `handoff-next.md` for Step 29.
