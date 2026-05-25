# Next step: Step 2

## Goal

Add a constexpr cursor over `std::string_view` and lexical classification functions for Scheme datum reading.
The cursor tracks offset, line, and column as it advances through input.

## Files expected to change

```txt
src/smd/schemepoc/reader_cursor.hpp
src/smd/schemepoc/reader_cursor.test.cpp
src/smd/schemepoc/CMakeLists.txt
```

## Context from previous step

Step 1 added the core utility vocabulary:

- `source.hpp`: `source_pos` (offset/line/column), `source_span` (first/last), `parse_error` (where/message).
  All constexpr.
  `source_pos` and `source_span` use defaulted `operator==`.
  `parse_error` uses a custom `operator==` that compares `char const*` messages by string content (not pointer identity), handling nullptr.
- `result.hpp`: `result<T>` wrapping `std::variant<T, parse_error>`.
  Provides `has_value()`, `value()`, `error()`.
  Constexpr.
  Includes `<smd/schemepoc/source.hpp>` and `<variant>`.
- `static_vector.hpp`: `static_vector<T, Capacity>` backed by `std::array<T, Capacity>`.
  Provides `push_back`, `size`, `empty`, `operator[]` (mutable and const).
  Uses `assert()` for capacity overflow.
  Constexpr.
  Includes `<array>` and `<cassert>`.
- All three are header-only with no `.cpp` files.
- All headers use classical `#ifndef`/`#define` guards.
- Tests use Catch2, double-include verification, and `static_assert` for constexpr contracts.
- `make compile` and `make test` pass (18 tests).
- `make lint` passes for all non-network hooks (markdownlint requires Node.js download which fails on SSL in this environment; that is a pre-existing infrastructure issue, not a code issue).

## Required implementation details

- `reader_cursor.hpp`: `cursor` class with explicit constructor from `std::string_view`.
  Methods: `empty()`, `peek()`, `bump()` (returns new cursor), `position()` (returns `source_pos`), `remaining()` (returns `std::string_view`).
  `bump()` must advance offset, and update line/column (newline increments line, resets column).
- Free functions: `is_space(char)`, `is_initial_symbol_char(char)`, `is_symbol_char(char)`, `is_delimiter(char)`, `skip_intertoken_space(cursor)`.
- Symbol classification: initial = alphabetic or `+ - * / = < > ! ?`; rest = initial or digit.
- Delimiter: whitespace, `(`, `)`, `'`, end of input.
- Do not support comments yet.
- All constexpr.
- `cursor` must include `<smd/schemepoc/source.hpp>` for `source_pos`.

## Required tests

- Header idempotency.
- `static_assert` that `is_delimiter('(')` and `is_delimiter(')')` are true.
- `static_assert` that `skip_intertoken_space` on `"  x"` peeks `'x'`.
- Cursor tracks offset, line, and column correctly through `bump()`.
- `is_initial_symbol_char` and `is_symbol_char` classifications.
- `skip_intertoken_space` skips whitespace and stops at non-whitespace.

## Required commands

```bash
make compile
make test
make lint
```

## Do not do

- Do not proceed to Step 3.
- Do not add parser combinators yet.
- Do not add comment support yet.
- Do not introduce optional dependencies unless this step requires them.
- Do not change architecture decisions without documenting the reason in `handoff.md`.
