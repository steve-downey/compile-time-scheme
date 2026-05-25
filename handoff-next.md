# Next step: Step 17 (defunctionalized CPS program)

## Goal

Implement a true bottom-up CPS compiler that transforms a `core_tree` into a
`cps_code` by structural recursion over the flat arena, threading
continuations as template parameters.  Replace the interpreter-backed
`compile_cps` stub in `cps.hpp` with a genuine CPS transformation.

## Files expected to change

```txt
src/smd/schemepoc/cps.hpp              (replace compile_cps stub; add cps_of)
src/smd/schemepoc/cps.test.cpp         (extend CPS tests for structural cases)
checklist.md                           (mark step done)
handoff-next.md                        (update for step 18)
```

No new header files are needed — `cps_of` lives in `cps.hpp`.

## Decision from step 16

**Structural recursion over the flat `core_tree` arena** was chosen.

`fix<F>` is not a literal type (it allocates on the heap with `new`/`delete`)
and cannot appear in `static_assert` tests.  The project's compile-time test
discipline requires all public constexpr APIs to be exercised in a
`static_assert`.  See `docs/cps-direction.md` for the full rationale.

## Current state of cps.hpp

The existing `compile_cps` is an interpreter-backed stub:

```cpp
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto compile_cps(
    core_tree<MaxNodes, MaxList> const& core,
    node_id                             root) {
    return cps_code{[core, root](auto const& env, auto k) constexpr {
        auto v = eval_direct(core, root, env);
        if (!v.has_value())
            return v;
        return k(v.value());
    }};
}
```

Step 17 replaces this with a real structural CPS transformation via `cps_of`.

## Target API for step 17

```cpp
// cps_of(core, id, cont)
//   -> a cps_code whose operator()(env, k) evaluates node `id` in CPS,
//      then passes the result to cont, then passes cont's result to k.
//
// cont is the current (inner) continuation; k is the outer continuation
// supplied at call-time.  For the outermost call, cont is the identity.
template <int MaxNodes, int MaxList, class Cont>
[[nodiscard]] constexpr auto cps_of(
    core_tree<MaxNodes, MaxList> const& core,
    node_id                             id,
    Cont                                cont);
```

Expose a convenience wrapper that matches the existing public name:

```cpp
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto compile_cps(
    core_tree<MaxNodes, MaxList> const& core,
    node_id                             root) {
    return cps_of(core, root, [](value v) { return result<value>{v}; });
}
```

## CPS transformation rules

For each node kind in `core_node<MaxList>`:

| Node | CPS form |
|------|----------|
| `core_integer{v}` | immediately call `cont(v)` |
| `core_boolean{v}` | immediately call `cont(v)` |
| `core_symbol{name}` | look up `name` in env; call `cont(v)` |
| `core_if{c, t, f}` | CPS-eval `c`; branch; CPS-eval `t` or `f` |
| `core_application{func, args}` | CPS-eval `func`; CPS-eval each arg; apply |

Lambda (`core_lambda`) and define (`core_define`) are out of scope for step 17
(they arrive in steps 20–21).  Return an error result if encountered.

## Continuation threading

Each recursive call to `cps_of` produces a new `cps_code` whose `F` type
wraps the previous continuation.  The type depth is O(expression depth).
For arithmetic-only expressions depth is at most 3–4 levels.

Example for `(+ a b)` where `+` applies the builtin add:

```
cps_of(app_node, id_cont)
  -> cps_code { eval func in CPS, then
       cps_of(arg_a, cont_a) where cont_a:
         cps_of(arg_b, cont_b) where cont_b:
           apply func_value to [a_val, b_val]
           cont(result) }
```

## Static_assert test pattern

All tests must use the immediately-invoked lambda pattern.  Do NOT declare
`constexpr auto ct = ...` at namespace or function scope — large
`core_tree<32,16>` objects (~17 KB each) cause GCC to OOM at 20 parallel
jobs.

```cpp
static_assert([] {
    auto ct = make_some_core_tree();           // inside the lambda
    auto code = compile_cps(ct, ct.size() - 1);
    auto r = code(default_env<8>(), [](value v) { return result<value>{v}; });
    return r.has_value() && std::get<int>(r.value()) == 42;
}());
```

`ct.size() - 1` is the root node (arena grows left-to-right).

## Safety notes

**Compilation memory:**
Incremental `make compile` is fast.  For a cold build, limit parallelism:

```bash
cmake --build .build/build-gcc-16 --config Asan --target all -- -k 0 -j4
```

Do NOT use `ulimit -v` — ASan requires ~15 TB of virtual address space.

**Template depth:**
Avoid deep recursion in `cps_of` itself — prefer iteration when processing
argument lists, only recurse for the tree structure.

**Error propagation:**
`result<value>` propagates errors.  `cps_of` for an unsupported node kind
(e.g., `core_lambda` in step 17) returns a `cps_code` that immediately
returns an error result without calling `cont` or `k`.

**compile_cps keeps the same signature:**
The existing `cps.test.cpp` tests call `compile_cps(core, root_id)`.
Preserve that signature.  The internal implementation changes, but tests
that already pass must continue to pass.

**No eval_direct dependency:**
Step 17's `compile_cps` must not call `eval_direct`.  The goal is a genuine
CPS transformation.  Remove the `#include <smd/schemepoc/eval_direct.hpp>`
from `cps.hpp` once `compile_cps` no longer needs it.

## Do not do

- Do not implement lambda or closure capture (steps 20–21).
- Do not implement a define form (step 20+).
- Do not implement the sender backend (steps 28–30).
- Do not proceed to step 18 in the same commit.
