# Next step: Step 11 (+ 12 combined)

## Goal

Add a direct tree-walking evaluator for the core language.
This is the first end-to-end milestone: `source string → datum_tree → core_tree → value`.
Steps 11 and 12 in the plan are combined because `core_if` is already elaborated — there is no reason to add evaluator support for it in a separate step.

Do not add CPS transformation, closure conversion, or any backend.
Do not add `core_lambda` evaluation (that is step 21 in the plan).
Do not add `core_define` evaluation yet.

## Files expected to change

```txt
src/smd/schemepoc/value.hpp             (new)
src/smd/schemepoc/value.test.cpp        (new)
src/smd/schemepoc/eval_direct.hpp       (new)
src/smd/schemepoc/eval_direct.test.cpp  (new)
src/smd/schemepoc/CMakeLists.txt        (add new files)
```

`elaborator.hpp`, `datum_tree.hpp`, `reader.hpp`, and all earlier files do not need changes.

## Context from previous steps

Step 9 (the step just merged) completed `elaborator.hpp` in `namespace smd::schemepoc`.
It combined plan steps 9 (core language model) and 10 (elaborator) into one deliverable.

The elaborator entry point:

```cpp
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto elaborate(datum_tree<MaxNodes, MaxList> const &dt)
    -> result<core_tree<MaxNodes, MaxList>>;
```

The root of the returned `core_tree` is always `ct.size() - 1`.

The core node types in `elaborator.hpp`:

```cpp
struct core_integer { int value; };
struct core_boolean { bool value; };
struct core_symbol  { std::string_view name; };   // variable reference
struct core_quote   { node_id datum; };            // datum_tree node_id kept as-is

struct core_if {
    node_id condition;
    node_id consequent;
    node_id alternative;
};

template <int MaxList>
struct core_lambda {
    static_vector<std::string_view, MaxList> params;
    node_id body;
};

template <int MaxList>
struct core_application {
    node_id func;
    static_vector<node_id, MaxList> args;
};

struct core_define {
    std::string_view name;
    node_id value;
};

template <int MaxList>
using core_node = std::variant<core_integer, core_boolean, core_symbol, core_quote,
                               core_if, core_lambda<MaxList>, core_application<MaxList>,
                               core_define>;
```

`core_tree<MaxNodes, MaxList>` has `add(core_node<MaxList>)`, `get(node_id)`, `size()`.

The reader: `read_datum<MaxNodes, MaxList>(cursor{sv})` returns
`parse_result<datum_tree<MaxNodes, MaxList>>`.
The value inside is `parse_state<datum_tree<...>>` with fields `.value` (the tree) and `.rest`.

89 tests pass.
`make compile`, `make test`, `make lint` are all green.

## Value type (`value.hpp`)

Define in `namespace smd::schemepoc`:

```cpp
enum class builtin_op { add, multiply };

struct builtin {
    builtin_op op;
    friend constexpr auto operator==(builtin, builtin) -> bool = default;
};

using value = std::variant<int, bool, builtin>;
```

`std::variant` equality is already defined so no additional `operator==` needed on `value` itself.

## Environment type

Define in `value.hpp` (or `eval_direct.hpp`):

```cpp
template <int MaxBindings>
class env {
  public:
    constexpr auto define(std::string_view name, value val) -> void;
    [[nodiscard]] constexpr auto lookup(std::string_view name) const
        -> result<value>;

  private:
    struct binding {
        std::string_view name;
        value val;
    };
    static_vector<binding, MaxBindings> bindings_{};
};

template <int MaxBindings>
[[nodiscard]] constexpr auto default_env() -> env<MaxBindings>;
```

`default_env()` must bind:
- `"+"` → `value{builtin{builtin_op::add}}`
- `"*"` → `value{builtin{builtin_op::multiply}}`

`lookup` searches from the end (so later `define` calls shadow earlier ones).
`lookup` returns a `parse_error` if the name is not found.

## Evaluator (`eval_direct.hpp`)

```cpp
template <int MaxNodes, int MaxList, int MaxBindings>
[[nodiscard]] constexpr auto eval_direct(
    core_tree<MaxNodes, MaxList> const &ct,
    node_id root,
    env<MaxBindings> const &environment) -> result<value>;
```

Evaluation rules (switch on variant alternatives of the root node):

- `core_integer{n}` → `value{n}`
- `core_boolean{b}` → `value{b}`
- `core_symbol{name}` → `environment.lookup(name)`
- `core_if{cond, cons, alt}`:
  - evaluate `cond`; if `holds_alternative<bool>` and `get<bool>` is `false`,
    evaluate `alt`; otherwise evaluate `cons`
- `core_application{func, args}`:
  - evaluate `func` → must be a `builtin` (return error otherwise)
  - evaluate each arg left-to-right → must all be `int` (return error otherwise)
  - for `builtin_op::add`: return `value{arg0 + arg1}` (exactly 2 args)
  - for `builtin_op::multiply`: return `value{arg0 * arg1}` (exactly 2 args)
  - wrong arity or non-integer arg → return `parse_error{{}, "type error"}`
- `core_quote`, `core_lambda`, `core_define`:
  → return `parse_error{{}, "eval_direct: unsupported form"}`

There is no helper for `result<value>`; use `parse_error{{}, "message"}` for errors.

## Required tests

Use `static_assert` for constexpr contracts; `TEST_CASE` for runtime variants.

Full pipeline static_assert (the key milestone):

```cpp
static_assert([] {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"(+ 1 (* 2 3))"sv});
    if (!dr.has_value()) return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value()) return false;
    auto const &ct = er.value();
    auto vr = eval_direct(ct, ct.size() - 1, default_env<16>());
    return vr.has_value() &&
           std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 7;
}());
```

Additional required static_assert cases:

- `"42"` → `value` holding `int{42}`
- `"#t"` → `value` holding `bool{true}`
- `"(+ 1 2)"` → `value` holding `int{3}`
- `"(* 3 4)"` → `value` holding `int{12}`
- `"(if #t 1 2)"` → `value` holding `int{1}`
- `"(if #f 1 2)"` → `value` holding `int{2}`
- `"()"` → error (elaboration fails before eval is reached)
- Unbound variable `"x"` → error from `lookup`

## CMakeLists.txt

Add to `FILE_SET HEADERS`:
- `value.hpp`
- `eval_direct.hpp`

Add to `target_sources` for `schemepoc_test`:
- `value.test.cpp`
- `eval_direct.test.cpp`

Follow the same pattern as `elaborator.hpp` / `elaborator.test.cpp`.

## Required commands

```bash
make compile
make test
make lint
```

## Do not do

- Do not add CPS transformation or any backend — those are later steps.
- Do not add `std::string` or any heap allocation.
- Do not evaluate `core_lambda` — that is step 21.
- Do not evaluate `core_define` — that is a later step.
- Do not change `elaborator.hpp`, `datum_tree.hpp`, `reader.hpp`, or any earlier file.
- Do not use `std::function` or type erasure.
- Do not use `using namespace` in headers.
- Do not change architecture decisions without documenting in `handoff.md`.
