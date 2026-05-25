# Next step: Step 19 (public one-shot API)

## Goal

Expose a clean, minimal public API over the full pipeline so that demo
snippets and future Godbolt extractions can use a single header.  The
concrete deliverable is a variable template `compiled_closure<"...">` that
evaluates a Scheme expression at compile time and holds the resulting
`closure_program` as a `constexpr` object.

## Files expected to change

```txt
src/smd/schemepoc/schemepoc.hpp       (add compiled_closure variable template)
src/smd/schemepoc/schemepoc.test.cpp  (add tests for public API)
checklist.md                          (mark step done)
handoff-next.md                       (update for step 20)
```

No new headers are needed.  The public API lives in `schemepoc.hpp`.

## Current state

`schemepoc.hpp` is a near-empty shell:

```cpp
// 44cc988c-7353-43aa-a7d3-8840f92371a6
namespace smd::schemepoc {} // namespace smd::schemepoc
// 44cc988c-7353-43aa-a7d3-8840f92371a6 end
```

`schemepoc.test.cpp` has only the `HeaderIsIdempotent` stub test.

The full pipeline is already implemented:

```
read_datum<MaxNodes, MaxList>(cursor{src})   -> parse_result<datum_tree>
elaborate(datum_tree)                        -> result<core_tree>
compile_cps(core_tree, root_id)              -> cps_code<F>
closure_program{cps_code}                   -> operator()(env) -> result<value>
compile_to_closure<MaxNodes, MaxList>(src)   -> result<closure_program<F>>
```

`compile_to_closure` is in `closure_backend.hpp` (added in step 18).

## Target API for step 19

```cpp
// source_literal: NTTP carrier for string literals.
template <std::size_t N>
struct source_literal {
    char text[N]{};
    constexpr source_literal(char const (&input)[N]) {
        std::copy_n(input, N, text);
    }
    constexpr auto view() const -> std::string_view { return {text, N - 1}; }
};

// compiled_closure: evaluates Source at compile time.
// Use: constexpr auto prog = compiled_closure<"(+ 1 2)">;
//      auto r = prog(default_env<16>());
template <source_literal Source>
inline constexpr auto compiled_closure =
    compile_to_closure(Source.view()).value();
```

`source_literal` allows a string literal to be a non-type template parameter
(C++20 class NTTP).  `compile_to_closure` uses default arena sizes
`<32, 16>`.

## Static_assert test pattern

Tests in `schemepoc.test.cpp` should use the immediately-invoked lambda
pattern for any test that holds an intermediate `core_tree` or `cps_code`
object.  The `compiled_closure` variable template itself is fine at namespace
scope because it is `inline constexpr` — there is one ODR-safe instance per
TU and the size is the same as a `closure_program<F>` (which captures a
`core_tree<32,16>` inside the lambda, ~17 KB).

For `static_assert` tests over `compiled_closure`, invoke it inside the
assertion expression, not via a named `constexpr auto`:

```cpp
static_assert([] {
    auto env0 = default_env<16>();
    auto r = compiled_closure<"(+ 1 (* 2 3))">(env0);
    return r.has_value() && std::get<int>(r.value()) == 7;
}());
```

## Compilation memory

Large `core_tree<32,16>` objects are captured inside `cps_code` and therefore
inside `closure_program` and `compiled_closure`.  At 20 parallel jobs GCC may
OOM during the static instantiation phase.  If compile fails with OOM, limit
parallelism:

```bash
cmake --build .build/build-gcc-16 --config Asan --target all -- -k 0 -j4
```

## Do not do

- Do not implement lambda or closure capture (steps 20–21).
- Do not implement a define form (step 20+).
- Do not change the arena sizes from the defaults (32, 16).
- Do not add a runtime `main` or example program (that comes later).
- Do not proceed to step 20 in the same commit.
