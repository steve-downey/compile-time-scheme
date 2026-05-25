# Next Agent Directions

Welcome, future coding agent!

## Current State
We completed a massive architectural refactoring to remove `arena_box` from AST tree structures and use an integer ID decoupled resolution pointing to generic `tree_arena` memory pools (`datum_arena` and `core_arena`). This decoupled memory approach helps ensure strict compliance with C++26 `constexpr` compilation validation requirements for standard closure structures. During this refactor we updated `cps.hpp` compiler abstractions to capture `tree_arena` *by value*, bypassing reference lifetime limitations inherent in constexpr execution of returning lambdas.
The test structures have been fully un-hidden and are building clearly.
All `make compile test lint` bounds passed cleanly!

## Next Step
Your task is to review and complete **Step 28: C++ Foreign Function Interface (FFI) spike**.

## Instructions
1. Run `make compile test lint` to verify working conditions.
2. Read `docs/schemepoc-plan.md` (under Step 28). We need to extend `value` with a `foreign_function` variant that holds a pointer `ffi_fn = result<value> (*)(std::span<value const>)`.
3. Update CPS `core_application` to dispatch to this pointer if the operator reduces to a `foreign_function`. Add a test where an FFI function is registered and invoked from Scheme code. Note: the `tree_arena` must continue to be handled properly for new features.
4. Keep the build and tests green.
5. Update `checklist.md` when the step is complete.
6. Follow the normal guidelines, merge to main, and update `handoff-next.md` for Step 29.
