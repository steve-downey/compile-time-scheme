# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

SchemePoC (`smd/schemepoc`) is a compile-time Scheme-light compiler proof of concept targeting C++26 on GCC16. The architecture is a staged pipeline:

```
source string -> reader datum tree -> elaborated core tree
  -> CPS / defunctionalized program -> closure backend
  -> Beman Execution sender backend -> reflection reification spike
```

The reader parses Scheme data but does not classify special forms. The elaborator handles `if`, `lambda`, application, quote, and binding forms. CPS is the semantic center. The closure backend is the stable demo path. The sender backend uses Beman Execution vendored as a git submodule at `vendor/execution`. Reflection is isolated until explicitly integrated.

See `docs/schempoc-plan.md` for the full implementation plan and step-by-step checklist.

## Build commands

The top-level Makefile drives all workflow. Do not bypass it for normal work.

```bash
make                    # default: build and run all tests (Asan config)
make compile            # compile only
make test               # rebuild and run tests
make ctest              # run CTest on current build without rebuilding
make lint               # run pre-commit linters (clang-format, gersemi, markdownlint, codespell, etc.)
make coverage           # build with Gcov profile and generate coverage report
make install            # install the project
make testinstall        # test the installed package
make compile-headers    # verify interface header sets compile independently
make help               # show all targets
```

Selecting a different compiler: `make TOOLCHAIN=gcc-17` or `make TOOLCHAIN=clang-23`. Default is `gcc-16`. Prefer the newest available toolchains (trunk is fine) over compromising on technique; toolchain files live in `etc/`.

Selecting a build config: `make CONFIG=RelWithDebInfo` (default is `Asan`). Available: `RelWithDebInfo`, `Debug`, `Tsan`, `Asan`, `Gcov`.

Tooling (cmake, ninja, pre-commit, clang-format, gcovr) is managed by `uv` and installed into a local `.venv`.

## Source layout

Merged `src/` layout (Pitchfork-style). Headers, implementations, and tests are co-located:

```
src/smd/schemepoc/<component>.hpp
src/smd/schemepoc/<component>.cpp
src/smd/schemepoc/<component>.test.cpp
src/smd/schemepoc/detail/<helper>.hpp    (non-public support only)
src/examples/                            (example programs)
```

Do not create split `include/`, `src/`, and `tests/` trees.

## C++ conventions

- **C++26 baseline, GCC16 baseline.** Do not add fallback paths for older standards or other compilers.
- **Namespace:** `smd::schemepoc`, aligned with directory path and include spelling.
- **Includes:** canonical angle-bracket spelling only (`#include <smd/schemepoc/reader.hpp>`). No relative includes. No transitive include reliance.
- **Header guards:** classical `#ifndef`/`#define` guards, not `#pragma once`. Guard names follow the local convention (e.g., `INCLUDED_SCHEMEPOC` or `SRC_SMD_SCHEMEPOC_READER_HPP`).
- **File prolog:** every source file starts with a canonical repo-relative path comment with Emacs mode line, then SPDX on the next line:
  ```cpp
  // src/smd/schemepoc/reader.hpp                                  -*-C++-*-
  // SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
  ```
- **Formatting:** clang-format is enforced (config in `.clang-format`). gersemi formats CMake files. Do not fight formatter output.
- Prefer `constexpr` for APIs that can be meaningfully constant-evaluated.
- No `using namespace` in headers.
- Use `snake_case` for file names. Prefer `.hpp`, `.cpp`, `.test.cpp`.

## Testing

- Tests use **Catch2**. Do not introduce GTest.
- Test files include the component header first, then include it again to verify idempotency.
- Co-locate tests with source under `src/`.
- Test skeleton:
  ```cpp
  // src/smd/schemepoc/reader.test.cpp                              -*-C++-*-
  // SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

  #include <smd/schemepoc/reader.hpp>
  #include <smd/schemepoc/reader.hpp>  // test 2nd include OK

  #include <catch2/catch_test_macros.hpp>

  TEST_CASE("ReaderTest - HeaderIsIdempotent") { REQUIRE(true); }
  ```

## CMake conventions

- Target-based CMake (cmake_minimum_required 4.0). Define a target first, attach sources with `target_sources`. Use `FILE_SET HEADERS` for public headers.
- Each `CMakeLists.txt` lists only files in its own directory. Descend with `add_subdirectory`. Do not reach upward or sideways.
- Catch2 is fetched via `infra/cmake/use-fetch-content.cmake` (set via `CMAKE_PROJECT_TOP_LEVEL_INCLUDES`).
- Beman Execution: vendored as git submodule at `vendor/execution`, integrated with `add_subdirectory`. Do not use FetchContent, vcpkg, or install-time discovery for Beman dependencies.

## UUID anchors and org-transclusion

Source files contain UUID-delimited blocks (e.g., `// 44cc988c-7353-43aa-a7d3-8840f92371a6`) used by org-transclusion to embed live code in `schemepoc.org` for presentations. Do not delete or nest existing UUID anchor pairs.

## Infrastructure

The `infra/` directory is vendored from the [Beman Project infra](https://github.com/bemanproject/infra) via git subtree. Toolchain files are in `etc/`. The `.pre-commit-config.yaml` drives linting (clang-format, gersemi, markdownlint, codespell, checkmake, shellcheck, gitleaks).
