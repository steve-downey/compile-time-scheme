# Next step: Step 7

## Goal

Add a fixed-capacity datum tree suitable for constexpr use.
This is the arena-based node store for parsed Scheme data (lists, atoms, quotes).
Do not add the reader grammar for lists or quote yet (that is Step 8).

## Files expected to change

```txt
src/smd/schemepoc/datum_tree.hpp        (new)
src/smd/schemepoc/datum_tree.test.cpp   (new)
src/smd/schemepoc/CMakeLists.txt        (add new files)
```

`reader_atom.hpp`, `parser.hpp`, `parser_alternative.hpp`, and `reader_cursor.hpp` do not need changes.

## Context from previous steps

Step 6 completed `reader_atom.hpp` in `namespace smd::schemepoc`:

```cpp
struct atom_integer { int value; };
struct atom_symbol  { std::string_view name; };
using atom = std::variant<atom_integer, atom_symbol>;

[[nodiscard]] constexpr auto integer_p();  // -> parse_result<atom_integer>
[[nodiscard]] constexpr auto symbol_p();   // -> parse_result<atom_symbol>
```

Step 1 provided `static_vector<T,N>` in `static_vector.hpp`.
Step 1 provided `source_pos`, `source_span`, `parse_error` in `source.hpp`.
Step 1 provided `result<T>` in `result.hpp`.

60 tests pass. `make compile`, `make test`, `make lint` are all green.

## Datum tree model

The datum tree stores nodes by integer ID in a flat arena.
This is deliberate: it avoids recursive C++ types, heap allocation, and
constexpr allocation persistence issues, keeping the Godbolt demo small.

Define in `datum_tree.hpp`:

```cpp
using node_id = int;
inline constexpr node_id invalid_node = -1;

struct datum_integer { int value; };
struct datum_symbol  { std::string_view name; };
struct datum_boolean { bool value; };

template <int MaxList>
struct datum_list {
    static_vector<node_id, MaxList> elements{};
};

struct datum_quote {
    node_id quoted{invalid_node};
};

template <int MaxList>
using datum_node = std::variant<datum_integer, datum_symbol, datum_boolean,
                                datum_list<MaxList>, datum_quote>;

template <int MaxNodes, int MaxList>
class datum_tree {
  public:
    constexpr auto add(datum_node<MaxList> value) -> node_id;
    [[nodiscard]] constexpr auto get(node_id id) const
        -> datum_node<MaxList> const &;
    [[nodiscard]] constexpr auto size() const -> int;

  private:
    static_vector<datum_node<MaxList>, MaxNodes> nodes_{};
};
```

Keep `datum_integer`, `datum_symbol`, `datum_boolean` distinct from
`atom_integer` / `atom_symbol` in `reader_atom.hpp`.
They serve different layers (datum tree vs parser output).
Do not merge or alias them.

## Required tests

In `datum_tree.test.cpp`, use `static_assert` to verify constexpr use:

- Manually add two integer nodes and a list node pointing to them; check `size() == 3`.
- Check that `get()` returns the correct variant.
- Runtime `TEST_CASE` variants are welcome but constexpr contracts must use `static_assert`.

Example constexpr test pattern:

```cpp
static_assert([] {
    datum_tree<8, 4> tree;
    auto a = tree.add(datum_integer{1});
    auto b = tree.add(datum_integer{2});
    datum_list<4> xs;
    xs.elements.push_back(a);
    xs.elements.push_back(b);
    tree.add(xs);
    return tree.size() == 3;
}());
```

## CMakeLists.txt

Add `datum_tree.hpp` to the `FILE_SET HEADERS` block and
`datum_tree.test.cpp` to the `target_sources` for `schemepoc_test`.
Follow the same pattern as `reader_atom.hpp` / `reader_atom.test.cpp`.

## Required commands

```bash
make compile
make test
make lint
```

## Do not do

- Do not add the reader grammar (parsing `(`, `)`, `'`) — that is Step 8.
- Do not add a core language model — that is Step 9.
- Do not add `std::string` or any heap allocation.
- Do not merge `datum_integer`/`datum_symbol` with `atom_integer`/`atom_symbol` from `reader_atom.hpp`.
- Do not change `reader_atom.hpp`, `parser.hpp`, `parser_alternative.hpp`, `reader_cursor.hpp`.
- Do not use raw function pointers.
- Do not change architecture decisions without documenting in `handoff.md`.
