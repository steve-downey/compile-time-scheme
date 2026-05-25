# Next step: Step 1

## Goal

Add the compile-time support vocabulary used by all later phases: source positions, a result type, and a fixed-capacity vector.

## Files expected to change

```txt
src/smd/schemepoc/source.hpp
src/smd/schemepoc/result.hpp
src/smd/schemepoc/static_vector.hpp
src/smd/schemepoc/source.test.cpp
src/smd/schemepoc/result.test.cpp
src/smd/schemepoc/static_vector.test.cpp
src/smd/schemepoc/CMakeLists.txt
```

## Context from previous step

Step 0 established the skeleton:
- `version.hpp`/`.cpp`/`.test.cpp` provide `version_major`, `version_minor`, `version_patch` as `inline constexpr int` in `smd::schemepoc`.
- `schemepoc.hpp`/`.cpp`/`.test.cpp` are minimal placeholders (empty namespace, idempotency test only).
- `hello.cpp` example prints the version.
- All files use `smd::schemepoc` namespace, canonical path comments, SPDX, and classical include guards.
- Tests use Catch2 with double-include verification.
- `make compile`, `make test`, `make lint` all pass.
- Governance files (`AGENTS.md`, `CLAUDE.md`, `docs/codestyle.org`, `docs/CODING_RULES.md`, `docs/schemepoc-plan.md`, `handoff.md`, `checklist.md`) are committed.

## Required implementation details

- `source.hpp`: `source_pos` (offset/line/column), `source_span` (first/last), `parse_error` (where/message). All constexpr, all with defaulted `operator==`.
- `result.hpp`: `result<T>` wrapping `std::variant<T, parse_error>`. Provides `has_value()`, `value()`, `error()`. Constexpr.
- `static_vector.hpp`: `static_vector<T, Capacity>` backed by `std::array<T, Capacity>`. Provides `push_back`, `size`, `empty`, `operator[]`. Constexpr. Use a simple assertion for capacity overflow.
- All three are header-only (no `.cpp` files needed unless you want a translation unit anchor).
- Add `static_assert` / constexpr tests for each type.

## Required tests

- `source.test.cpp`: header idempotency, `source_pos` default construction and equality.
- `result.test.cpp`: header idempotency, construct from value, construct from error, `has_value()` true/false paths.
- `static_vector.test.cpp`: header idempotency, `push_back`/`size`/`operator[]` in constexpr context, `static_assert` on a small vector.

## Required commands

```bash
make compile
make test
make lint
```

## Do not do

- Do not proceed to Step 2.
- Do not introduce optional dependencies unless this step requires them.
- Do not change architecture decisions without documenting the reason in `handoff.md`.
- Do not design a general allocator for `static_vector`.
- Do not add parser code yet.
