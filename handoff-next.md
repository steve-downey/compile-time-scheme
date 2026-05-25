# Next step: Step 5

## Goal

Add parser repetition and lexeme: `many`, `some`, `optional`, and `lexeme`.
Also add `alt` as an explicit named function (not just the operator).

Do not add integer or symbol parsing yet (those are Step 6+).

## Files expected to change

```txt
src/smd/schemepoc/parser_alternative.hpp     (new)
src/smd/schemepoc/parser_alternative.test.cpp  (new)
src/smd/schemepoc/CMakeLists.txt             (add new files)
```

`parser.hpp` and `parser.test.cpp` do not need changes.

## Context from previous steps

Step 4 completed `parser.hpp` with the full combinator set in `namespace smd::schemepoc`:

```cpp
template <class T>
struct parse_state { T value; cursor rest; };

template <class T>
using parse_result = result<parse_state<T>>;

template <class F>
class parser { ... };  // wraps callable by value, calls it from operator()

template <class F>
parser(F) -> parser<F>;

// primitives
template <class T>
[[nodiscard]] constexpr auto pure(T value);
[[nodiscard]] constexpr auto satisfy(auto pred, char const* expected);
[[nodiscard]] constexpr auto char_p(char expected);

// functor / applicative
template <class PA, class F>
[[nodiscard]] constexpr auto map(PA pa, F f);

template <class PA, class PB, class F>
[[nodiscard]] constexpr auto lift2(PA pa, PB pb, F f);

template <class PA, class PB>
[[nodiscard]] constexpr auto sequence_left(PA pa, PB pb);

template <class PA, class PB>
[[nodiscard]] constexpr auto sequence_right(PA pa, PB pb);

// choice (operator on parser objects)
template <class PA, class PB>
[[nodiscard]] constexpr auto operator|(PA pa, PB pb);
```

Key implementation notes from Step 4:
- `map` and `lift2` deduce return type with `using R = decltype(f(r.value().value))` inside
  the lambda, used in both branches. The `decltype(...)` in the error branch is an unevaluated
  operand, so calling `.value()` on an error result is safe for type deduction only.
- `operator|` detects input consumption via `cur.position().offset` vs `ra.error().where.offset`.
  (`parse_error` has `.where`, a `source_pos` with `.offset` — NOT `.pos`.)
- All parsers capture by value in lambdas.
- `parse_error` is defined in `source.hpp` as `struct parse_error { source_pos where; char const* message; }`.
- `cursor::position()` returns `source_pos` which has `.offset`.

Step 1 provided `static_vector<T,N>` in `static_vector.hpp` — use it for `many`/`some` results.

Step 2 provided `skip_intertoken_space(cursor)` in `reader_cursor.hpp` — use it for `lexeme`.

`make compile` and `make test` pass (43 tests). `make lint` passes.

## Required implementation details

All additions go in `src/smd/schemepoc/parser_alternative.hpp`,
in `namespace smd::schemepoc`.

Include `<smd/schemepoc/parser.hpp>` and `<smd/schemepoc/static_vector.hpp>`
(and `<optional>` for `optional`).

### `alt`

```cpp
template <class PA, class PB>
[[nodiscard]] constexpr auto alt(PA pa, PB pb);
```

Named alias for `operator|`. Tries `pa`; on failure without consuming, tries `pb`.
Implementation: `return pa | pb;`

### `many`

```cpp
template <int Capacity, class P>
[[nodiscard]] constexpr auto many(P p);
```

Repeatedly applies `p` until it fails or capacity is reached.
Returns `parse_result<static_vector<T, Capacity>>` where `T` is the value type of `p`.
`many` never fails — returns an empty vector on zero matches.
Stops silently at capacity (does not overflow).

### `some`

```cpp
template <int Capacity, class P>
[[nodiscard]] constexpr auto some(P p);
```

Like `many` but requires at least one match.
Fails with the error from the first application of `p` if it yields zero matches.

### `optional`

```cpp
template <class P>
[[nodiscard]] constexpr auto optional(P p);
```

Tries `p`. On success wraps the value in `std::optional<T>`. On failure (without consuming),
returns `std::optional<T>{}` (empty) without consuming input. Never fails.

### `lexeme`

```cpp
template <class P>
[[nodiscard]] constexpr auto lexeme(P p);
```

Skips leading intertoken space (using `skip_intertoken_space`), applies `p`,
then skips trailing intertoken space. Returns the value of `p`.
On failure from `p`, propagates the error unchanged.

Implementation sketch:

```cpp
template <class P>
[[nodiscard]] constexpr auto lexeme(P p) {
    return parser{[p](cursor cur) {
        auto start = skip_intertoken_space(cur);
        auto r     = p(start);
        if (!r.has_value())
            return r;
        auto rest = skip_intertoken_space(r.value().rest);
        using V   = decltype(r.value().value);
        return parse_result<V>{parse_state<V>{r.value().value, rest}};
    }};
}
```

## CMakeLists.txt

Add `parser_alternative.hpp` and `parser_alternative.test.cpp` to the existing
target in `src/smd/schemepoc/CMakeLists.txt`. Follow the same pattern as the
existing `parser.hpp` / `parser.test.cpp` entries.

## Required tests

In `parser_alternative.test.cpp`:

- `static_assert` that `alt(char_p('a'), char_p('b'))(cursor{"a"})` succeeds with `'a'`.
- `static_assert` that `alt(char_p('a'), char_p('b'))(cursor{"b"})` succeeds with `'b'`.
- `static_assert` that `alt(char_p('a'), char_p('b'))(cursor{"c"})` fails.
- `static_assert` that `many<4>(char_p('a'))(cursor{""}).value().value` is an empty vector (`.size() == 0`).
- `static_assert` that `many<4>(char_p('a'))(cursor{"aaa"}).value().value.size() == 3`.
- `static_assert` that `some<4>(char_p('a'))(cursor{""})` fails.
- `static_assert` that `some<4>(char_p('a'))(cursor{"aa"}).value().value.size() == 2`.
- `static_assert` that `optional(char_p('a'))(cursor{"b"}).has_value()` and the inner optional is empty.
- `static_assert` that `optional(char_p('a'))(cursor{"a"}).value().value.has_value()`.
- `static_assert` that `lexeme(char_p('x'))(cursor{"  x  "}).value().value == 'x'`.
- Runtime `TEST_CASE` variants are welcome but constexpr contracts must use `static_assert`.

## Required commands

```bash
make compile
make test
make lint
```

## Do not do

- Do not add `integer_p`, `symbol_p`, or any reader/datum integration.
- Do not add comment-stripping to the cursor.
- Do not change `parser.hpp`, `parse_state`, `parse_result`, or `parser` class.
- Do not use raw function pointers.
- Do not use `std::vector` or any heap allocation.
- Do not change architecture decisions without documenting in `handoff.md`.
