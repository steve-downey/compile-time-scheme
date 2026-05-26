# SchemePoC: Compile-Time Scheme-Light in C++26

[![OpenSSF Baseline](https://www.bestpractices.dev/projects/12577/baseline)](https://www.bestpractices.dev/projects/12577)

SchemePoC is a proof-of-concept **compile-time Scheme compiler and evaluator** targeting C++26 and GCC 16. It leverages the latest C++ language features (like deep `constexpr` evaluation, C++26 reflection spike, and coroutines via Beman Task) to safely parse, elaborate, and evaluate a small LISP/Scheme-like language natively at compile time.

## Architecture

> **Note:** The source was originally laid out under `src/smd/schemepoc/` with namespace `smd::schemepoc`. After the initial 34-step build plan was complete, the project was refactored into a per-module directory hierarchy under `src/smd/smdscheme/` with namespace `smd::smdscheme`. The pipeline and architecture are unchanged; only the file paths and namespace identifiers differ.

The project is structured as a staged pipeline to cleanly separate parsing from semantic elaboration and evaluation:

1. **Source String**: Raw `char` sequence input.
2. **Reader (`src/smd/smdscheme/reader/reader.hpp`)**: Parses the input string into a raw S-expression/Datum tree. It uses an arena allocator designed for `constexpr` to avoid heap persistence issues across compile-time boundaries.
3. **Elaborator (`src/smd/smdscheme/elaborator/elaborator.hpp`)**: Traverses the generic datum tree to classify specific semantic Scheme forms (like literals, symbols, `if` statements, `lambda`, and applications) into a strongly typed `core_tree`.
4. **Evaluator (`src/smd/smdscheme/elaborator/eval_direct.hpp`)**: Direct tree-walking evaluator representing standard synchronous execution.
5. **CPS Transformation (`src/smd/smdscheme/cps/cps.hpp`)**: Continuation-Passing Style structural transformation, paving the way for advanced non-blocking branch flows.
6. **Closure Backend (`src/smd/smdscheme/closure/closure_backend.hpp`)**: Compiles the CPS-transformed elements into explicitly typed C++ function closures for zero-overhead evaluation.
7. **Sender Backend (`src/smd/smdscheme/sender/sender_backend.hpp`)**: An asynchronous compiler backend relying on C++ Execution primitives (`beman::task`) to resolve Scheme branch flows directly natively inside C++ coroutines, bypassing `variant` recursion limits.
8. **Reflection Spike (`src/smd/smdscheme/reflection/reflection_reify.hpp`)**: Demonstrates translating captured environment metadata straight into generated C++ aggregates using the experimental C++26 `std::meta` (`-freflection`).

## Build Instructions

This project requires GCC 16 to support the C++26 reflection and core constant-evaluation semantics required by the implementations.

The build system leverages CMake, the Beman project's infrastructure, and a local Python virtual environment managed by `uv`.

```bash
make          # default: build and run all tests with Asan enabled
make compile  # compile only
make test     # rebuild and run Catch2 test binaries
make ctest    # run CTest on the current build
make lint     # run pre-commit linters (clang-format, cmake, codespell, etc.)
```

## Documentation

The `docs/` folder contains deeply explored architectural documentation explaining how the compiler transitions concepts across boundaries natively:
- `docs/compiler_architecture.org`: Describes the phases from Parser through Closure Materialization.
- `docs/schemepoc-plan.md`: The initial roadmap detailing the 34 chronological steps used to achieve this repository state.
- `docs/Compile-Time Scheme-Light in C++26.md`: Preliminary research documentation that sets out the guiding philosophy of the project combining CPS, applicator parsers, and execution layers in C++26.
- `docs/constexpr-allocations.md`: Details how `constexpr_box` and Arena identifiers resolve standard `std::vector` leaks at C++ boundaries.

## Try it on Compiler Explorer (Godbolt)

You can try the direct S-expression arithmetic or lambda examples embedded within the `src/examples` code natively in GCC 16 (trunk) via [Compiler Explorer](https://godbolt.org/) instantly by copying `src/examples/godbolt_arithmetic.cpp` or `src/examples/advanced_reflection_ffi.cpp` using the `-std=c++26 -freflection` flag.

## Licensing

Code follows the Apache 2.0 license with LLVM exception.

Enjoy evaluating Scheme inside C++ templates!
