# Next step: Step 14 (generic fixpoint playground)

## Goal

Add a small, isolated `fix<F>` and `fold_fix` experiment without converting
the main arena-based compiler. This step introduces recursion-scheme machinery
in a safe sandbox. The existing compiler pipeline must not be modified.

## Files expected to change

```txt
src/smd/schemepoc/fix.hpp         (new)
src/smd/schemepoc/fix.test.cpp    (new)
src/smd/schemepoc/CMakeLists.txt  (add new files)
checklist.md                      (mark step done)
handoff-next.md                   (update for step 15)
```

No existing files need to change.

## Context from previous steps

Steps 11-13 completed the first full pipeline milestone and a presentation layer:

- `value.hpp`: `enum class builtin_op { add, multiply }`, `struct builtin`,
  `using value = std::variant<int, bool, builtin>`,
  `template <int MaxBindings> class env`,
  `template <int MaxBindings> constexpr auto default_env() -> env<MaxBindings>`
- `eval_direct.hpp`: `template <int MaxNodes, int MaxList, int MaxBindings> constexpr auto eval_direct(core_tree<MaxNodes, MaxList> const &ct, node_id root, env<MaxBindings> const &environment) -> result<value>`
- `elaborator.hpp`: all core types, `elaborate(datum_tree)` function
- `reader.hpp`: `read_datum<MaxNodes, MaxList>(cursor{sv})` returning `parse_result<datum_tree<...>>`
- `parser_ops.hpp`: `parser_applicative_ops`, `parser_alternative_ops`,
  `struct parser_ops : parser_applicative_ops, parser_alternative_ops {}`,
  `inline constexpr parser_ops parser_v{}`

105 tests pass. `make compile`, `make test`, `make lint` are all green.

## Required implementation

The `fix.hpp` header must define `fix<F>` and `fold_fix` in
`namespace smd::schemepoc`. The `fix` type wraps a single layer of a
functorial expression family:

```cpp
template <template <class> class F>
class fix {
  public:
    using layer_type = F<fix<F>>;

    constexpr explicit fix(layer_type layer);

    [[nodiscard]] constexpr auto layer() const -> layer_type const &;

  private:
    layer_type layer_;
};
```

`fold_fix` folds a `fix<F>` tree using a carrier-algebra. It requires that
`fmap` is findable via ADL for the layer type:

```cpp
template <template <class> class F, class Algebra>
constexpr auto fold_fix(fix<F> const &tree, Algebra algebra) {
    auto mapped = fmap(tree.layer(), [&](auto const &child) {
        return fold_fix(child, algebra);
    });
    return algebra(mapped);
}
```

To test, define a tiny expression family in the test file (not the header):

```cpp
template <class A>
struct int_f {
    int value{};
};

template <class A>
struct add_f {
    A left;
    A right;
};

template <class A>
using expr_f = std::variant<int_f<A>, add_f<A>>;
```

Define `fmap` for `expr_f<A>` in the test file as well:

```cpp
template <class A, class Fn>
constexpr auto fmap(expr_f<A> const &layer, Fn fn) {
    return std::visit(
        [&](auto const &node) -> expr_f<decltype(fn(node.left))> {
            // int_f has no children; add_f maps both children
            ...
        },
        layer);
}
```

The eval algebra maps `expr_f<int>` to `int`:

```cpp
constexpr auto eval_algebra = [](expr_f<int> layer) -> int {
    return std::visit([](auto const &node) -> int { ... }, layer);
};
```

Test that `(1 + 2) + 3` evaluated through `fold_fix` gives `6`.

Keep tests as `static_assert` where possible. Include a runtime
`TEST_CASE("FixTest - HeaderIsIdempotent")`.

## CMakeLists.txt changes

Add to `FILE_SET HEADERS`:
- `fix.hpp`

Add to `target_sources` for `schemepoc_test`:
- `fix.test.cpp`

Follow the same pattern as `parser_ops.hpp` / `parser_ops.test.cpp`.

## Required commands

```bash
make compile
make test
make lint
```

## Design notes

- `fmap` must be defined by the user of `fix`, not inside `fix.hpp` itself.
  `fix.hpp` depends on ADL to find `fmap` for each specific functor family.
- `fix<F>` is value-semantics. No heap allocation.
- Do not use `std::recursive_variant` or `std::any`.
- `constexpr` throughout where GCC16 supports it for `std::variant`.

## Do not do

- Do not change any existing files except `CMakeLists.txt`, `checklist.md`,
  and `handoff-next.md`.
- Do not refactor `reader.hpp`, `elaborator.hpp`, `eval_direct.hpp`, or
  any other pipeline file onto `fix<F>`.
- Do not add `using namespace` in headers.
- Do not add CPS, closure backend, or sender changes.
- Do not proceed to Step 15 (CPS closure backend) in the same commit.
- Do not change architecture decisions without documenting the reason in
  `handoff.md`.
