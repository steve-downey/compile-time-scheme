# Next step: Step 8

## Goal

Add the reader grammar for lists and quote.
The reader must be able to parse `(`, `)`, `'`, and nested lists of atoms,
producing a `datum_tree` by consuming a source string.
Do not add the elaborator or any core language model — that is Step 9.

## Files expected to change

```txt
src/smd/schemepoc/reader.hpp          (new)
src/smd/schemepoc/reader.test.cpp     (new)
src/smd/schemepoc/CMakeLists.txt      (add new files)
```

`datum_tree.hpp`, `reader_atom.hpp`, `parser.hpp`, `parser_alternative.hpp`,
and `reader_cursor.hpp` do not need changes.

## Context from previous steps

Step 7 completed `datum_tree.hpp` in `namespace smd::schemepoc`:

```cpp
using node_id = int;
inline constexpr node_id invalid_node = -1;

struct datum_integer { int value; };
struct datum_symbol  { std::string_view name; };
struct datum_boolean { bool value; };

template <int MaxList>
struct datum_list { static_vector<node_id, MaxList> elements{}; };

struct datum_quote { node_id quoted{invalid_node}; };

template <int MaxList>
using datum_node = std::variant<datum_integer, datum_symbol, datum_boolean,
                                datum_list<MaxList>, datum_quote>;

template <int MaxNodes, int MaxList>
class datum_tree {
  public:
    constexpr auto add(datum_node<MaxList> value) -> node_id;
    [[nodiscard]] constexpr auto get(node_id id) const -> datum_node<MaxList> const &;
    [[nodiscard]] constexpr auto size() const -> int;
  private:
    static_vector<datum_node<MaxList>, MaxNodes> nodes_{};
};
```

Step 6 completed `reader_atom.hpp`:

```cpp
struct atom_integer { int value; };
struct atom_symbol  { std::string_view name; };
using atom = std::variant<atom_integer, atom_symbol>;

[[nodiscard]] constexpr auto integer_p();  // -> parse_result<atom_integer>
[[nodiscard]] constexpr auto symbol_p();   // -> parse_result<atom_symbol>
```

Step 1 provided `static_vector<T,N>`, `source_pos`, `source_span`,
`parse_error`, and `result<T>` in `static_vector.hpp`, `source.hpp`, `result.hpp`.

Step 2 provided `parser<F>`, `cursor`, `parse_result<T>`, `parse_state<T>`,
`char_p`, `satisfy`, `pure`, `sequence_left`, `sequence_right`, `map`, `lift2`,
`alternation` in `parser.hpp`.

Step 4 provided `many<N>`, `some<N>`, `optional`, `lexeme` in
`parser_alternative.hpp`.

67 tests pass. `make compile`, `make test`, `make lint` are all green.

## Reader model

Define a function (or set of functions) in `reader.hpp` that parses a
complete Scheme datum expression into a `datum_tree`.

Suggested entry point:

```cpp
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto read_datum(cursor cur)
    -> result<datum_tree<MaxNodes, MaxList>>;
```

The reader must handle:

- Integer atoms: delegate to `integer_p()`, store as `datum_integer`.
- Symbol atoms: delegate to `symbol_p()`, store as `datum_symbol`.
- Boolean literals `#t` and `#f`: store as `datum_boolean`.
- Lists: `(` followed by zero or more datums followed by `)`.
  Store as `datum_list` with child node IDs; the list node is added last.
- Quote shorthand: `'<datum>` → `datum_quote{inner}` where `inner` is the
  node ID of the quoted datum.
- Whitespace between tokens is consumed by `lexeme` or `skip_intertoken_space`.

The reader must not classify special forms (`if`, `lambda`, `define`, etc.).
Those are the elaborator's job (Step 9 and beyond).

The tree is built bottom-up: atoms are added first, then enclosing lists.

Implementation note: mutual recursion between `read_datum` and list parsing
is expected. Because the parser combinators are lambda-based, you may need
to pass the tree by reference and use an explicit recursive helper function
rather than composing parsers directly, since parsers cannot refer to
themselves by name. A plain recursive `constexpr` function is fine.

## Required tests

In `reader.test.cpp`, use `static_assert` for constexpr contracts:

- Parse `"42"` → tree with one `datum_integer{42}` node.
- Parse `"foo"` → tree with one `datum_symbol{"foo"}` node.
- Parse `"#t"` → tree with one `datum_boolean{true}` node.
- Parse `"()"` → tree with one empty `datum_list`.
- Parse `"(1 2)"` → tree with three nodes: two integers and a list.
- Parse `"'x"` → tree with two nodes: a symbol and a quote wrapping it.
- Parse `"(1 (2 3))"` → nested list; check node count and structure.

Runtime `TEST_CASE` variants are welcome but constexpr contracts must use
`static_assert`.

## CMakeLists.txt

Add `reader.hpp` to the `FILE_SET HEADERS` block and
`reader.test.cpp` to the `target_sources` for `schemepoc_test`.
Follow the same pattern as `datum_tree.hpp` / `datum_tree.test.cpp`.

## Required commands

```bash
make compile
make test
make lint
```

## Do not do

- Do not add the elaborator or core language model — that is Step 9.
- Do not add `std::string` or any heap allocation.
- Do not merge `datum_integer`/`datum_symbol` with `atom_integer`/`atom_symbol`
  from `reader_atom.hpp`.
- Do not change `datum_tree.hpp`, `reader_atom.hpp`, `parser.hpp`,
  `parser_alternative.hpp`, `reader_cursor.hpp`.
- Do not use raw function pointers.
- Do not change architecture decisions without documenting in `handoff.md`.
