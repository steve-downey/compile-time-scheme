# Next steps for future development

Welcome to the SchemePoC project.

## Current State

The core architectural phases of the `smd/schemepoc` project have been **fully completed** matching the original plan (Step 0 - Step 34).
The project now stands as a complete proof of concept of a compile-time Scheme-light dialect capable of being parsed and heavily executed entirely within C++26 `constexpr` conditions using GCC 16.

The architecture flows statically down this pipeline:
- `Reader` parses S-expressions into fixed-capacity flat arenas.
- `Elaborator` translates datum into semantically constrained `core_tree`s.
- `CPS Transformations` handle flow execution logic.
- Both synchronous closure backends and standard C++ coroutine sender execution paths exist.
- A `reflection_reify` spike was conducted to prove how environments map strictly securely natively back to compile-time C++ layout types via `^^` operator reflection features correctly.

## Goal for the next contributor

As the initial steps 1-34 are completed, your potential next activities could involve:

1. **Expanding the Standard Library**: Support more Scheme primitives across arithmetic, `cons`/`car`/`cdr`, mapping, or basic algorithms inside the elaborator.
2. **Integrating Reflection tightly**: Use the results of the reflection spike (`src/smd/schemepoc/reflection_reify.hpp`) explicitly within `closure_backend.hpp` to statically collapse captured environments natively into real concrete native structures instead of runtime variable mapping structs avoiding runtime allocations entirely!
3. **Optimizing the CPS Graph**: Write analytical passes over the CPS structures natively before emitting C++ Closures!

Whatever direction you take, the primary rules still apply.
- You MUST maintain C++26 baseline compatibility using GCC 16 (`std=gnu++26`).
- Write tests and stick to `make compile`, `make test`, `make lint` rigorously preventing regression.

## Expected Files to review

Please start by checking `docs/compiler_architecture.org` and `README.md` to map out the final integration flow correctly.
Read `src/examples/godbolt_lambda.cpp` and `godbolt_arithmetic.cpp` for basic entry points.
To understand the full integration of C++26 standard reflection (`-freflection`) interacting directly with the compilation engine, you should review `src/examples/advanced_reflection_ffi.cpp`. This generates an environment dynamically mapping Scheme closures into actual C++ `consteval` struct aggregates, whilst defining FFI foreign mappings executing `std::print` bridging C++ code safely side-stepping back to the Scheme language!
