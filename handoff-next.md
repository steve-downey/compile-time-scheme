# Next step: Step 18 (closure materialization over CPS program)

## Goal

Step 18 introduces closure materialization: given the CPS program produced by
`compile_cps`, produce a set of named C++ closure structs (one per CPS
continuation) that can be stored, passed around, and eventually linked into a
Beman Execution sender chain.

The immediate scope for step 18 is modest: extract the continuations that are
already implicit in `cps_dispatch` into explicitly named, constexpr-storable
objects, without yet involving the sender backend.

## Current state after step 17

`cps.hpp` now contains a genuine CPS transformation:

```
detail::identity_k         — fixed-type identity continuation (value -> result<value>)
detail::cps_dispatch<...>  — structural recursive evaluator threading cont and k
cps_of(core, id, cont)     — returns cps_code{lambda capturing (core, id, cont)}
compile_cps(core, root)    — convenience wrapper: cps_of(core, root, identity_k{})
```

`cps_dispatch` dispatches on five node kinds:
- `core_integer`, `core_boolean` — immediate value, call cont then k
- `core_symbol` — env lookup, call cont then k (or propagate error)
- `core_if` — evaluate condition with identity cont/k, branch, tail-call
- `core_application` — evaluate func and each arg with identity cont/k, apply builtin, call cont then k
- `core_lambda`, `core_define`, `core_quote` — return error ("unsupported form")

Two template instantiations of `cps_dispatch` are used in practice:
1. `<Cont, Env, K>` for the outermost call
2. `<identity_k, Env, identity_k>` for all intermediate sub-expression evaluations

The `eval_direct` include was removed from `cps.hpp`; `compile_cps` no longer
calls `eval_direct`.

## What step 18 should do

The checklist entry is: **Step 18: closure materialization over CPS program.**

The likely shape of step 18 is to introduce a `cps_closure.hpp` that defines:

1. A `cps_frame` variant type representing each kind of CPS continuation frame
   (one constructor per "what to do after evaluating a sub-expression"):
   - After evaluating the condition of an `if`: branch and continue
   - After evaluating the function position of an application: evaluate args
   - After evaluating each argument: accumulate, eventually apply

2. A `cps_stack` (or similar) that holds a static_vector of `cps_frame` values,
   so the continuation is represented as an explicit data structure rather than
   as nested C++ template types.

3. A `materialize(core, root)` function that converts the `core_tree` into a
   sequence of `cps_frame` objects using an iterative worklist algorithm
   (push continuations onto the stack, process nodes).

The goal is that the materialized representation is a literal type (all
`static_vector`, no heap), directly usable in `static_assert` tests.

## Design note from step 16/17

The `cps_dispatch` approach is correct but still builds continuations as
nested C++ lambda types at call time.  Step 18 makes the continuation chain
EXPLICIT as a runtime data structure (the frame stack) rather than implicit
in the C++ type system.  This is the "defunctionalization" mentioned in the
checklist.

## Files expected to change

```txt
src/smd/schemepoc/cps_closure.hpp     (new: cps_frame, cps_stack, materialize)
src/smd/schemepoc/cps_closure.test.cpp (new: tests for materialized closures)
src/smd/schemepoc/CMakeLists.txt       (add cps_closure.hpp to FILE_SET,
                                        cps_closure.test.cpp to test target)
checklist.md                           (mark step 18 done)
handoff-next.md                        (update for step 19)
```

## Test discipline

All tests must use the immediately-invoked lambda pattern inside `static_assert`:

```cpp
static_assert([] {
    auto ct = ...;  // build core_tree inside lambda
    auto frames = materialize(ct, ct.size() - 1);
    // inspect frames...
    return ...;
}());
```

Do NOT declare `constexpr auto ct = ...` at namespace scope — large
`core_tree<32,16>` objects (~17 KB) cause GCC to OOM at 20 parallel jobs.

## Safety notes

**Compilation memory:** for a cold build, limit parallelism:
```bash
cmake --build .build/build-gcc-16 --config Asan --target all -- -k 0 -j4
```

**Do not** use `ulimit -v` — ASan requires ~15 TB of virtual address space.

## Do not do

- Do not touch `cps_dispatch` or `compile_cps` internals (step 17 is done).
- Do not implement the sender backend (steps 28–30).
- Do not implement lambda/closure capture (steps 20–23).
- Do not proceed to step 19 in the same commit.
