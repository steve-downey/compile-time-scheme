# Next step: Step 4

## Goal

Add the parser combinators: `map`, `lift2`, `sequence_left` (`<*`), `sequence_right` (`*>`),
and `operator|` (alternation / choice).

Do not add `many`, `some`, `token`, `integer_p`, or any reader integration yet (those are Step 5+).

## Files expected to change

```txt
src/smd/schemepoc/parser.hpp
src/smd/schemepoc/parser.test.cpp
```

CMakeLists.txt does not need changes (parser files are already wired in).

## Context from previous steps

Step 3 produced `parser.hpp` with these definitions in `namespace smd::schemepoc`:

```cpp
template <class T>
struct parse_state {
    T      value;
    cursor rest;
};

template <class T>
using parse_result = result<parse_state<T>>;

template <class F>
class parser {
  public:
    constexpr explicit parser(F f);
    constexpr auto operator()(cursor cur) const;  // returns f_(cur)
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

- `parser` wraps a callable by value. No raw function pointers.
- `pure(v)` always succeeds, leaves cursor unchanged.
- `satisfy(pred, msg)` returns a `parse_result<char>`.
- `char_p(c)` is a `satisfy` specialisation matching exactly `c`.

Step 1 provided `result<T>` (over `std::variant<T, parse_error>`), `source_pos`, `source_span`,
`parse_error`, and `static_vector<T,N>`.

Step 2 provided `cursor` (constexpr, tracks offset/line/column), and free functions
`is_space`, `is_initial_symbol_char`, `is_symbol_char`, `is_delimiter`,
`skip_intertoken_space`.

`make compile` and `make test` pass (36 tests). `make lint` passes.

## Required implementation details

All additions go in `parser.hpp`, in `namespace smd::schemepoc`.

### `map`

```cpp
template <class PA, class F>
[[nodiscard]] constexpr auto map(PA pa, F f);
```

Runs `pa`. On success applies `f` to the parsed value, returns a new `parse_result` with the
mapped value and the rest cursor. On failure propagates the error unchanged.

### `lift2`

```cpp
template <class PA, class PB, class F>
[[nodiscard]] constexpr auto lift2(PA pa, PB pb, F f);
```

Runs `pa` then `pb` sequentially. On success of both, applies `f(a, b)` and returns the
result with the cursor after `pb`. Short-circuits on the first failure.

### `sequence_left` (the `<*` operator — keep the left value)

```cpp
template <class PA, class PB>
[[nodiscard]] constexpr auto sequence_left(PA pa, PB pb);
```

Equivalent to `lift2(pa, pb, [](auto a, auto) { return a; })`.

### `sequence_right` (the `*>` operator — keep the right value)

```cpp
template <class PA, class PB>
[[nodiscard]] constexpr auto sequence_right(PA pa, PB pb);
```

Equivalent to `lift2(pa, pb, [](auto, auto b) { return b; })`.

### `operator|` (alternation / choice)

```cpp
template <class PA, class PB>
[[nodiscard]] constexpr auto operator|(PA pa, PB pb);
```

Tries `pa` first. If it succeeds, returns its result. If it fails **without consuming
input**, tries `pb` on the original cursor. If `pa` consumed input (i.e. the cursor
advanced), propagate `pa`'s error without trying `pb`.

Detecting consumption: compare `cur.position().offset` before and after `pa`.

- Parsers must be captured by value in lambdas (no raw function pointers, no references
  to temporaries).
- All functions should be `constexpr` and `[[nodiscard]]`.
- Return types are deduced; do not spell out `parse_result<…>` in the function signatures
  where deduction suffices.

## Required tests

Add to `parser.test.cpp`:

- `static_assert` that `map(char_p('x'), [](char c){ return int(c); })(cursor{"xyz"})` has
  value equal to `int('x')`.
- `static_assert` that `map` on failure propagates the error.
- `static_assert` that `lift2(char_p('a'), char_p('b'), [](char a, char b){ return a == 'a' && b == 'b'; })(cursor{"ab"})` has value `true`.
- `static_assert` that `sequence_left(char_p('a'), char_p('b'))(cursor{"ab"}).value().value == 'a'` and cursor is after both.
- `static_assert` that `sequence_right(char_p('a'), char_p('b'))(cursor{"ab"}).value().value == 'b'`.
- `static_assert` that `(char_p('a') | char_p('b'))(cursor{"b"})` succeeds with `'b'`.
- `static_assert` that `(char_p('a') | char_p('b'))(cursor{"c"})` fails.
- Runtime `TEST_CASE` variants are welcome but the constexpr contracts must use `static_assert`.

## Required commands

```bash
make compile
make test
make lint
```

## Do not do

- Do not add `many`, `some`, `token`, `skip`, `integer_p`, or reader integration.
- Do not add comment-stripping to the cursor.
- Do not change `parse_state`, `parse_result`, or the `parser` class structure.
- Do not use raw function pointers.
- Do not change architecture decisions without documenting the reason in `handoff.md`.
