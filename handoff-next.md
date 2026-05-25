# Next step: Step 9

## Goal

Add the elaborator.  The elaborator consumes a `datum_tree` produced by the
reader and classifies special forms, producing an elaborated core tree.

The elaborator owns recognition of `if`, `lambda`, `quote`, `define`, and
application.  Nothing in the reader or datum_tree may be changed to support
this — the datum_tree is immutable data; the elaborator reads it and builds
a new tree.

Do not add CPS transformation, closure conversion, or any backend — those
are later steps.

## Files expected to change

```txt
src/smd/schemepoc/elaborator.hpp      (new)
src/smd/schemepoc/elaborator.test.cpp (new)
src/smd/schemepoc/CMakeLists.txt      (add new files)
```

`reader.hpp`, `datum_tree.hpp`, and all earlier files do not need changes.

## Context from previous steps

Step 8 completed `reader.hpp` in `namespace smd::schemepoc`:

```cpp
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto read_datum(cursor cur)
    -> parse_result<datum_tree<MaxNodes, MaxList>>;
```

`read_datum` parses a Scheme datum expression bottom-up into a `datum_tree`.
It handles:
- integers (`datum_integer`), symbols (`datum_symbol`), booleans
  (`datum_boolean{true/false}` for `#t`/`#f`)
- lists (`datum_list<MaxList>` whose `elements` is a
  `static_vector<node_id, MaxList>` of child node IDs)
- quote shorthand (`datum_quote{quoted}` whose `quoted` is the inner
  node ID)

The root datum is always the *last* node added to the tree (highest node_id).
`datum_tree::size() - 1` is the root.

Step 7 completed `datum_tree.hpp`:

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
};
```

78 tests pass.  `make compile`, `make test`, `make lint` are all green.

## Elaborated core model

Define the elaborated AST in `elaborator.hpp`.  Suggested node kinds:

```cpp
struct core_integer { int value; };
struct core_boolean { bool value; };
struct core_symbol  { std::string_view name; };   // variable reference
struct core_quote   { node_id datum; };            // quoted constant (keeps datum node_id)

struct core_if {
    node_id condition;
    node_id consequent;
    node_id alternative;
};

struct core_lambda {
    // parameter names stored as a static_vector of string_view
    static_vector<std::string_view, MaxParams> params;
    node_id body;
};

struct core_application {
    node_id func;
    static_vector<node_id, MaxArgs> args;
};

struct core_define {
    std::string_view name;
    node_id value;
};
```

Choose template parameters `MaxParams` and `MaxArgs` that parallel
`MaxList`.  A `core_node` variant and a `core_tree` arena class parallel
`datum_node` / `datum_tree`.

Suggested entry point:

```cpp
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto elaborate(datum_tree<MaxNodes, MaxList> const &dt)
    -> result<core_tree<MaxNodes, MaxList>>;
```

Where `core_tree` mirrors `datum_tree` (arena of `core_node` variants
indexed by `node_id`).

## Elaboration rules

Walk the `datum_tree` from the root node (index `dt.size() - 1`) recursively.

- `datum_integer{n}` → `core_integer{n}`
- `datum_boolean{b}` → `core_boolean{b}`
- `datum_symbol{s}` → `core_symbol{s}`
- `datum_quote{inner}` → `core_quote{inner}` (keep the datum node_id as-is;
  the quoted data lives in the datum_tree)
- `datum_list` whose first element is a symbol:
  - `"if"` with 3 remaining children → `core_if`
  - `"lambda"` with `datum_list` as first child (params) and one body →
    `core_lambda`
  - `"define"` with a symbol and one value → `core_define`
  - `"quote"` with one child → `core_quote`
  - anything else → `core_application` (elaborate head and each arg)
- `datum_list` whose first element is not a symbol → `core_application`
- Empty `datum_list` → elaboration error (empty application)

Return a `result<core_tree<...>>` where failure carries a `parse_error`
describing what went wrong (reuse `parse_error` from `source.hpp`).

## Required tests

Use `static_assert` for constexpr contracts:

- Elaborate the datum for `"42"` → `core_integer{42}` at root.
- Elaborate `"foo"` → `core_symbol{"foo"}` at root.
- Elaborate `"#t"` → `core_boolean{true}` at root.
- Elaborate `"'x"` → `core_quote` at root wrapping the symbol datum.
- Elaborate `"(if #t 1 2)"` → `core_if` at root with correct children.
- Elaborate `"(lambda (x) x)"` → `core_lambda` at root with param `"x"`
  and body being `core_symbol{"x"}`.
- Elaborate `"(foo 1 2)"` → `core_application` at root.
- Elaborate `"(define x 42)"` → `core_define` at root.
- Elaborate `"()"` → elaboration error (empty application).

Runtime `TEST_CASE` variants are welcome; constexpr contracts must use
`static_assert`.

## CMakeLists.txt

Add `elaborator.hpp` to `FILE_SET HEADERS` and
`elaborator.test.cpp` to `target_sources` for `schemepoc_test`.
Follow the same pattern as `reader.hpp` / `reader.test.cpp`.

## Required commands

```bash
make compile
make test
make lint
```

## Do not do

- Do not add CPS transformation or any backend — those are later steps.
- Do not add `std::string` or any heap allocation.
- Do not change `datum_tree.hpp`, `reader.hpp`, or any earlier file.
- Do not use raw function pointers.
- Do not use `using namespace` in headers.
- Do not change architecture decisions without documenting in `handoff.md`.
