# DIV-0026: GCC trunk r16-8246 misfolds memchr-lowered `find` on an offset string_view

- **Status:** open
- **Date:** 2026-08-11
- **Step:** R6 (conformance corpus and differential oracle); discovered and
  flagged by R3 (`src/smd/cl/reader`)
- **Authority diverged from:** none (a compiler defect, not a project rule)

## What diverged

`src/smd/cl/reader/number.hpp`'s `detail::find_char` and
`detail::find_exponent_marker` locate a character in a `std::string_view`
with `std::ranges::find_if` and a single-character predicate, spelled that
way instead of the more direct `text.find(target)` or
`std::ranges::find(text, target)`. The file's own comment records why:

> GCC trunk r16-8246 misfolds memchr-lowered searches (member `find`,
> `ranges::find` of a char) on a view offset into a string literal — after
> `remove_prefix(1)`, `find` reports the offset in the original literal.

Minimal shape of the defect: a `constexpr` `std::string_view` built from a
string literal, advanced with `remove_prefix`, then searched with
`.find(c)` (or `std::ranges::find`, which lowers to the same `memchr` call)
reports an offset as if searching from the *un-advanced* start of the
literal, not from the view's current begin.

## Why

Not a project defect: `text.find(target)` and `find_char` are the same
algorithm, and only the compiler's optimization of one of the two spellings
disagrees with the source. GCC's `__builtin_memchr`-based folding of
`basic_string_view::find`/`std::ranges::find` under constant evaluation is
what miscomputes the offset; the predicate form (`find_if`) takes a
different, unoptimized path and folds correctly. `docs/verification-matrix.md`
already establishes that toolchain trunk defects are witnessed by the
project's own tests rather than assumed away.

## Consequences

- `src/smd/cl/reader/number.hpp` must keep `find_char`/`find_exponent_marker`
  spelled as predicate searches (`find_if`) rather than reverting to
  `std::string_view::find` or `std::ranges::find` for a single character,
  even though the predicate form reads as more machinery for the same
  question. A reviewer simplifying this file back to `text.find(target)`
  would reintroduce the misfold under constant evaluation.
- No test in this project constructs the minimal reproduction directly
  (unlike DIV-0013, which does); the workaround was applied at the point of
  discovery rather than isolated into its own regression test. If a future
  GCC trunk fixes this, `number.hpp`'s comment and this doc are the record
  to check before reverting to the more direct spelling.

## Revisit condition

Closed when a GCC trunk build folds `std::string_view::find` (or
`std::ranges::find` of a single `char`) correctly on a view produced by
`remove_prefix` from a string-literal-backed view, under constant
evaluation. Until then, keep the predicate-search workaround.
