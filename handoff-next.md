# Next step: Step 3

## Goal

Add the minimal parser object: a constexpr wrapper around a callable object, plus
`parse_state<T>`, `parse_result<T>`, `pure`, `satisfy`, and `char_p`.

Do not add combinators yet (those are Step 4).

## Files expected to change

```txt
src/smd/schemepoc/parser.hpp
src/smd/schemepoc/parser.test.cpp
src/smd/schemepoc/CMakeLists.txt
```

## Context from previous steps

Step 2 added `reader_cursor.hpp` with:

- `cursor`: explicit constructor from `std::string_view`. Methods: `empty()`, `peek()`,
  `bump()` (returns new cursor advancing offset and tracking line/column), `position()`
  (returns `source_pos`), `remaining()` (returns `std::string_view`).
- Free functions: `is_space(char)`, `is_initial_symbol_char(char)`, `is_symbol_char(char)`,
  `is_delimiter(char)`, `skip_intertoken_space(cursor)`.
- All constexpr. Header-only, classical `#ifndef` guards.

Step 1 provided:

- `source.hpp`: `source_pos` (offset/line/col), `source_span`, `parse_error`. All constexpr.
- `result.hpp`: `result<T>` over `std::variant<T, parse_error>`. Provides `has_value()`,
  `value()`, `error()`. Constexpr.
- `static_vector.hpp`: fixed-capacity constexpr container.

`make compile` and `make test` pass (30 tests). `make lint` passes.

## Required implementation details

`parser.hpp` must define these in `namespace smd::schemepoc`:

```cpp
template <class T>
struct parse_state {
    T value;
    cursor rest;
};

template <class T>
using parse_result = result<parse_state<T>>;

template <class F>
class parser {
  public:
    constexpr explicit parser(F f);
    constexpr auto operator()(cursor cur) const;
  private:
    F f_;
};

template <class F>
parser(F) -> parser<F>;

template <class T>
[[nodiscard]] constexpr auto pure(T value);

[[nodiscard]] constexpr auto satisfy(auto pred, char const* expected);
[[nodiscard]] constexpr auto char_p(char expected);
```

- `parser` wraps a callable `F`; `operator()` invokes `f_` with the cursor.
- `pure(value)` returns a parser that always succeeds and leaves the cursor unchanged.
- `satisfy(pred, expected)`: if the cursor is non-empty and `pred(peek())` is true, returns
  a `parse_state` with the peeked character and `bump()`ed cursor; otherwise returns a
  `parse_error` at `cur.position()` with `expected` as the message.
- `char_p(c)`: `satisfy` specialization that matches exactly `c`.
- Do not use raw function pointers anywhere. Parser combinators must capture parser objects
  by value so lambdas can hold them.
- Include `<smd/schemepoc/result.hpp>` and `<smd/schemepoc/reader_cursor.hpp>`.

## Required tests

- Header idempotency.
- `static_assert` that `char_p('x')(cursor{"xyz"})` has value, `value().value == 'x'`,
  and `value().rest.peek() == 'y'`.
- `static_assert` that `pure(42)(cursor{"abc"})` has value `42` and cursor unchanged.
- `char_p` on wrong character returns error.
- `satisfy` on empty cursor returns error.
- Runtime `TEST_CASE` variants using `REQUIRE` / `REQUIRE_FALSE` are welcome but the
  constexpr contracts must use `static_assert`.

## Required commands

```bash
make compile
make test
make lint
```

## Do not do

- Do not proceed to Step 4.
- Do not add `map`, `apply`, `sequence_left`, `sequence_right`, or `lift2` yet.
- Do not add comment support to the cursor.
- Do not use raw function pointers.
- Do not change architecture decisions without documenting the reason in `handoff.md`.
