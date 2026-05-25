# Next step: Step 6

## Goal

Add reader atom parsers: `integer_p` and `symbol_p`.
These produce typed `datum` variants (or simpler typed tokens) from raw character sequences.

Do not add list or pair parsing yet (that is Step 7+).
Do not add the datum tree or arena yet.

## Files expected to change

```txt
src/smd/schemepoc/reader_atom.hpp        (new)
src/smd/schemepoc/reader_atom.test.cpp   (new)
src/smd/schemepoc/CMakeLists.txt         (add new files)
```

`parser.hpp`, `parser_alternative.hpp`, and `reader_cursor.hpp` do not need changes.

## Context from previous steps

Step 5 completed `parser_alternative.hpp` with the full repetition combinator set
in `namespace smd::schemepoc`:

```cpp
// named alias for operator|
template <class PA, class PB>
[[nodiscard]] constexpr auto alt(PA pa, PB pb);

// zero or more, returns static_vector<T, Capacity>; never fails
template <int Capacity, class P>
[[nodiscard]] constexpr auto many(P p);

// one or more; fails with error from first application if zero matches
template <int Capacity, class P>
[[nodiscard]] constexpr auto some(P p);

// wraps value in std::optional<T>; never fails
template <class P>
[[nodiscard]] constexpr auto optional(P p);

// skips leading/trailing intertoken space, applies p
template <class P>
[[nodiscard]] constexpr auto lexeme(P p);
```

Key implementation notes:
- `many` and `some` return `parse_result<static_vector<V, Capacity>>` where `V` is the
  value type of the sub-parser.
- `optional` returns `parse_result<std::optional<V>>`. On failure it returns an empty
  optional in a successful `parse_result` (not an error) and does not consume input.
- `lexeme` uses `skip_intertoken_space` from `reader_cursor.hpp`.
- All parsers capture by value. No raw function pointers. No heap.

Step 1 provided `static_vector<T,N>` in `static_vector.hpp` — already used by `many`/`some`.
Step 2 provided lexical predicates in `reader_cursor.hpp`:
  - `is_space(char)`, `is_initial_symbol_char(char)`, `is_symbol_char(char)`,
    `is_delimiter(char)`, `skip_intertoken_space(cursor)`.
Step 3 provided `parser<F>`, `parse_state<T>`, `parse_result<T>` in `parser.hpp`.
Step 4 added `map`, `lift2`, `sequence_left`, `sequence_right`, `operator|`.
Step 5 added `alt`, `many`, `some`, `optional`, `lexeme` in `parser_alternative.hpp`.

`make compile` and `make test` pass (49 tests). `make lint` passes.

## Atom model

Define a tagged-union `atom` type in `reader_atom.hpp` that can represent:
- an integer (a signed 64-bit integer value, or `int` if that suffices for now)
- a symbol (a `std::string_view` into the original source — no allocation)

A minimal representation:

```cpp
struct atom_integer { int value; };
struct atom_symbol  { std::string_view name; };

using atom = std::variant<atom_integer, atom_symbol>;
```

Keep it simple. No heap. `std::string_view` points into the cursor's underlying
`std::string_view` (the `remaining()` slice at parse time).

## Required parsers

### `integer_p`

```cpp
[[nodiscard]] constexpr auto integer_p();
```

Parses an optional leading `-` followed by one or more decimal digits.
Returns `parse_result<atom_integer>` with the numeric value.
Uses `some<20>(satisfy(...))` and converts the digit sequence to an integer.
Does not use `std::stoi` (not constexpr). Compute manually:

```cpp
// inside the lambda after digit collection:
int sign = negative ? -1 : 1;
int n = 0;
for (int i = 0; i < digits.size(); ++i)
    n = n * 10 + (digits[i] - '0');
return parse_result<atom_integer>{parse_state<atom_integer>{atom_integer{sign * n}, rest}};
```

For the sign: use `optional(char_p('-'))` to detect it.

### `symbol_p`

```cpp
[[nodiscard]] constexpr auto symbol_p();
```

Parses an initial symbol character followed by zero or more symbol characters.
Returns `parse_result<atom_symbol>` with a `std::string_view` into the source.

`std::string_view` can be constructed from the cursor's `remaining()` at the start,
then `substr(0, length)` where `length` is the number of consumed characters.

```cpp
// inside the lambda:
auto start   = cur;
auto first   = satisfy(is_initial_symbol_char, "symbol")(cur);
if (!first.has_value()) return parse_result<atom_symbol>{first.error()};
auto rest_cur = first.value().rest;
auto tail    = many<64>(satisfy(is_symbol_char, "symbol char"))(rest_cur);
auto end_cur  = tail.value().rest;
int  len      = end_cur.position().offset - start.position().offset;
auto name    = start.remaining().substr(0, len);
return parse_result<atom_symbol>{parse_state<atom_symbol>{atom_symbol{name}, end_cur}};
```

## CMakeLists.txt

Add `reader_atom.hpp` to the `FILE_SET HEADERS` block and
`reader_atom.test.cpp` to the `target_sources` for `schemepoc_test`.
Follow the same pattern as `parser_alternative.hpp` / `parser_alternative.test.cpp`.

## Required tests

In `reader_atom.test.cpp`:

- `static_assert` that `integer_p()(cursor{"42"})` succeeds with value `42`.
- `static_assert` that `integer_p()(cursor{"-7"})` succeeds with value `-7`.
- `static_assert` that `integer_p()(cursor{"abc"})` fails.
- `static_assert` that `symbol_p()(cursor{"foo"})` succeeds with `.name == "foo"`.
- `static_assert` that `symbol_p()(cursor{"+"})`  succeeds with `.name == "+"`.
- `static_assert` that `symbol_p()(cursor{"42"})` fails.
- Runtime `TEST_CASE` variants are welcome but constexpr contracts must use `static_assert`.

## Required commands

```bash
make compile
make test
make lint
```

## Do not do

- Do not add list/pair/quote atom types.
- Do not add a datum tree or arena.
- Do not add `std::string` or any heap allocation.
- Do not use `std::stoi`, `std::stol`, or any non-constexpr standard library function
  for the integer conversion.
- Do not change `parser.hpp`, `parser_alternative.hpp`, `reader_cursor.hpp`, or any
  prior component.
- Do not add `lexeme` wrapping to the atom parsers (leave that to the caller).
- Do not use raw function pointers.
- Do not change architecture decisions without documenting in `handoff.md`.
