# Next step: Step 15 (CPS closure backend facade, arithmetic only)

## Goal

Add `cps.hpp` and `cps_arithmetic.test.cpp` that compile a `core_tree` node to a
continuation-passing-style callable object. No lambdas yet — only arithmetic
expressions that `eval_direct` already handles. The first implementation is
interpreter-backed (calls `eval_direct` inside the CPS closure). The boundary
and test contract are what matter.

## Files expected to change

```txt
src/smd/schemepoc/cps.hpp              (new)
src/smd/schemepoc/cps_arithmetic.test.cpp  (new)
src/smd/schemepoc/CMakeLists.txt       (add new files)
checklist.md                           (mark step done)
handoff-next.md                        (update for step 16)
```

No existing pipeline files need to change.

## Context from previous steps

Steps 11–14 completed:

- `value.hpp`: `enum class builtin_op { add, multiply }`, `struct builtin { builtin_op op; }`,
  `using value = std::variant<int, bool, builtin>`,
  `template <int MaxBindings> class env`,
  `template <int MaxBindings> constexpr auto default_env() -> env<MaxBindings>`
- `eval_direct.hpp`:
  `template <int MaxNodes, int MaxList, int MaxBindings>`
  `constexpr auto eval_direct(core_tree<MaxNodes, MaxList> const&, node_id, env<MaxBindings> const&) -> result<value>`
- `elaborator.hpp`: all core types (`core_integer`, `core_boolean`, `core_symbol`,
  `core_if`, `core_application<MaxList>`, `node_id`, `core_tree<MaxNodes, MaxList>`),
  `constexpr auto elaborate(datum_tree<...>) -> result<core_tree<...>>`
- `reader.hpp`: `read_datum<MaxNodes, MaxList>(cursor{sv}) -> parse_result<datum_tree<...>>`
- `parser_ops.hpp`: typeclass-object facade over the parser free functions
- `fix.hpp`: `template <template <class> class F> class fix`,
  `template <class R, template <class> class F, class Algebra>`
  `constexpr auto fold_fix(fix<F> const&, Algebra) -> R`

106 tests pass. `make compile`, `make test`, `make lint` are all green.

## Required implementation

### cps.hpp

Define `cps_code<F>` and `compile_cps` in `namespace smd::schemepoc`:

```cpp
template <class F>
struct cps_code {
    F f;

    template <class Env, class K>
    constexpr auto operator()(Env const& env, K k) const {
        return f(env, k);
    }
};

template <class F>
cps_code(F) -> cps_code<F>;
```

`compile_cps` returns a `cps_code` backed by `eval_direct`:

```cpp
template <int MaxNodes, int MaxList, int MaxBindings>
[[nodiscard]] constexpr auto compile_cps(
    core_tree<MaxNodes, MaxList> const& core,
    node_id root) {
    return cps_code{[core, root](auto const& env, auto k) constexpr {
        auto v = eval_direct(core, root, env);
        if (!v.has_value())
            return v;
        return k(v.value());
    }};
}
```

Note: `compile_cps` captures `core` by value into the lambda. `MaxBindings` is
not a parameter to `compile_cps`; it is deduced from the `env` argument passed
at call time.

The `compile_cps` signature does NOT fix `MaxBindings`; instead the lambda's
`auto const& env` lets any matching `env<N>` be passed.

Headers needed: `elaborator.hpp`, `eval_direct.hpp`, `value.hpp`, `result.hpp`.

### cps_arithmetic.test.cpp

Build an arithmetic expression via reader + elaborator, compile it with
`compile_cps`, and invoke with a continuation.

Suggested test expression: `"(+ 3 4)"` → result should be `value{7}`.

Test pattern:
```cpp
constexpr auto src = std::string_view{"(+ 3 4)"};
constexpr auto parsed = read_datum<64, 16>(cursor{src});
// static_assert or require parsed.has_value()
constexpr auto ct = elaborate(parsed.value().value);
// static_assert or require ct.has_value()
constexpr auto code = compile_cps(ct.value(), ct.value().root());
constexpr auto env0 = default_env<16>();
constexpr auto result = code(env0, [](value v) constexpr {
    return smd::schemepoc::result<value>{v};
});
static_assert(result.value() == value{7});
```

Include the component header first and twice. Include a runtime
`TEST_CASE("CpsArithmeticTest - HeaderIsIdempotent")`.

### CMakeLists.txt changes

Add to `FILE_SET HEADERS`:
- `cps.hpp`

Add to `target_sources` for `schemepoc_test`:
- `cps_arithmetic.test.cpp`

Follow the same pattern as `fix.hpp` / `fix.test.cpp`.

## Design notes

- `compile_cps` is interpreter-backed in step 15. Steps 16–18 will replace
  the interpreter with a true bottom-up CPS compiler.
- `cps_code<F>` uses concrete templated callables, not `std::move_only_function`
  or `std::any`. This is intentional for constexpr compatibility.
- `K` (the continuation) must return `result<value>` so that errors propagate.
- Do not add lambda support, closures, or defunctionalization yet.

## Do not do

- Do not change `eval_direct.hpp`, `elaborator.hpp`, `value.hpp`, `reader.hpp`,
  `fix.hpp`, or any other existing pipeline file.
- Do not add `using namespace` in headers.
- Do not implement a true bottom-up CPS compiler (that is step 16).
- Do not add CPS for lambdas (that is step 20+).
- Do not proceed to step 16 in the same commit.
