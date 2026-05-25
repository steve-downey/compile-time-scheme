# CPS direction: structural recursion over flat arena

## Decision

The bottom-up CPS compiler (step 17) will use **structural recursion over
the flat `core_tree` arena** (Option A).  `fix<F>`-based folding (Option B)
is deferred indefinitely because `fix<F>` is not a literal type.

## The concrete constraint

`core_tree<MaxNodes, MaxList>` is a flat arena:

```cpp
template <int MaxNodes, int MaxList>
class core_tree {
    static_vector<core_node<MaxList>, MaxNodes> nodes_{};
  public:
    constexpr auto add(core_node<MaxList>) -> node_id;
    constexpr auto get(node_id) const -> core_node<MaxList> const&;
    constexpr auto size() const -> int;
};
```

Nodes are referenced by `node_id` (integer index).  The tree is not a
recursive type; it has no self-referential structure at the C++ level.

`fix<F>` uses `new` / `delete` internally:

```cpp
template <template <class> class F>
class fix {
    layer_type *layer_ = nullptr;  // heap-allocated
    constexpr explicit fix(layer_type layer)
        : layer_(new layer_type(std::move(layer))) {}
    constexpr ~fix() { delete layer_; }
    ...
};
```

Constexpr `new`/`delete` (C++20) is legal in a constant expression only if
the allocation is freed within the same constant expression.  A `fix<F>`
returned from a function outlives its allocation's constant-expression
context, so it is not a literal type.  That bars it from `static_assert`
tests.

## Option A — structural recursion (chosen)

Walk `core_tree` directly.  A recursive `cps_of` function dispatches on the
node variant, recurses by `node_id`, and threads the continuation as a
template parameter.

Sketch of the step-17 API:

```cpp
// cps_of<MaxNodes, MaxList>(core, id, cont)
//   -> a cps_code whose operator()(env, k) evaluates node `id` in CPS.
//
// cont is the current continuation; it is a constexpr callable.
// The outermost call passes the identity continuation.
template <int MaxNodes, int MaxList, class Cont>
[[nodiscard]] constexpr auto cps_of(
    core_tree<MaxNodes, MaxList> const& core,
    node_id                             id,
    Cont                                cont);
```

For a literal integer node `core_integer{v}` the result is a `cps_code`
whose body immediately calls `cont(v)`.  For `core_if{c, t, f}` the result
evaluates `c` in CPS, branches on the resulting boolean, then evaluates `t`
or `f` in CPS, both tailing into `cont`.

The continuation type at each recursive call wraps the continuation from its
call site.  For an arithmetic expression of depth D the outermost type
includes D layers of wrapping.  At the depths reachable by the arithmetic-
only phase (at most 3–4 levels) this is cheap.

## Option B — Fix<F>-based fold (deferred)

Convert `core_tree` to `fix<expr_f>` (a separate pass) and apply
`fold_fix` with a CPS algebra.  `fold_fix` from step 14 provides the
infrastructure.

This approach is algebraically clean but has two blockers:

1. **Not a literal type.**  `fix<F>` allocates on the heap, so it cannot
   appear in `static_assert` tests.  The project's compile-time test
   discipline requires every public constexpr API to be exercised in a
   `static_assert`.

2. **Extra pass.**  A conversion from the flat arena to `fix<expr_f>` is a
   non-trivial additional step with its own correctness surface.

Option B becomes viable once `constexpr new` allocation persistence lands in
the standard (post-C++26) and compilers implement it, allowing `fix<F>` to
be a literal type.  Until then the `fix<F>` playground from step 14 remains
a standalone experiment.

## Continuation threading

Step 17 uses the immediately-invoked lambda pattern for all `static_assert`
tests to avoid materialising large intermediate objects at namespace scope:

```cpp
static_assert([] {
    auto ct = ...; // core_tree built inside the lambda
    auto code = cps_of(ct, ct.size() - 1, [](value v) { return v; });
    auto result = code(default_env<8>(), [](value v) { return result<value>{v}; });
    return result.has_value() && std::get<int>(result.value()) == 42;
}());
```

`ct.size() - 1` is the root node (the arena grows left-to-right; the last
added node is the root).

## When to revisit

Revisit Option B if:

- A future standard provides constexpr allocation persistence, making
  `fix<F>` a literal type.
- Expression depth grows past ~8 levels and the nested continuation types
  cause unacceptable compile times.
- A "pre-pass" that converts the arena to `fix<expr_f>` is desirable for
  other reasons (e.g., optimization passes, sharing detection).
