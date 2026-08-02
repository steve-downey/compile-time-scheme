# DIV-0013: under `-fsanitize=null`, `compiled_lisp` cannot evaluate closure-building programs in a constant expression

- **Status:** open
- **Date:** 2026-07-26
- **Step:** L22 (public API, Godbolt, FFI parity)
- **Authority diverged from:** docs/cl-pivot-plan.md §9

## What diverged

`smd::smdlisp::compiled_lisp<Source>` is a namespace-scope
`inline constexpr` variable and is usable in constant expressions -- except
that, in the project's default `Asan` config, a program that materializes a
closure (any `lambda`, `function`, or `defun` form) cannot be *evaluated*
through it inside a `static_assert`.

`src/smd/smdlisp/smdlisp.test.cpp` therefore evaluates such programs at
compile time through a function-local `lisp_program<Source>{}` object and
reserves `compiled_lisp<Source>` for the runtime `TEST_CASE` twins and for
constexpr evaluation of closure-free programs.
The plan implies the public variable template is uniformly constexpr-callable.

## Why

Not an `smdlisp` defect. GCC 16 (`16.0.1 20260322`, trunk `r16-8246`) with
`-fsanitize=null` -- included in `-fsanitize=undefined`, which the `Asan`
config in `etc/gcc-16-toolchain.cmake` enables -- refuses to fold a comparison
between a null pointer and the address of a subobject of a namespace-scope
constexpr object. Minimal reproduction:

```cpp
struct Node { int v = 1; };
struct Holder {
    Node n{};
    Node const* p;
    constexpr Holder() : p(&n) {}
};
inline constexpr Holder h{};
static_assert(h.p != nullptr);
// error: '(((const Node*)(& h.Holder::n)) != 0)' is not a constant expression
```

The same file compiles clean without the flag, at every optimization level.
`-fsanitize=pointer-overflow`, `-fsanitize=alignment`, and
`-fsanitize=object-size` do not trigger it; `-fsanitize=null` and
`-fsanitize=nonnull-attribute` do.

The `smdlisp` path that hits this is `closure/cps_code.hpp`'s
`cps_apply_function_value`, whose first act on a `closure<Core>` value is the
guard `if (clo.node == nullptr)`. A closure's `node` points into the core
arena that `lisp_program` stores by value, so under `compiled_lisp<Source>`
that is exactly the rejected comparison. When the program object is instead a
local created during the constant evaluation, the comparison folds normally.

`smd::smdscheme::compiled_closure` is unaffected: its equivalent path does not
null-check a pointer into the compiled program.

## Consequences

- Tests added in later steps must not assume `compiled_lisp<Source>` is
  constexpr-evaluable for closure-building sources. Use a local
  `lisp_program<Source>{}` for the constexpr twin. The house rule that every
  `static_assert` gets a runtime `TEST_CASE` twin already covers the gap; here
  the runtime twin is the one that exercises the public variable template.
- Nothing is wrong at run time. `src/examples/godbolt_lisp.cpp` uses
  `compiled_lisp` with a `defun` program and passes under Asan.
- Step L24's consolidation into `docs/compiler_architecture.org` should carry
  this note, since it constrains how the public API is demonstrated.
- Anyone who removes `-fsanitize=undefined` from a config will see the
  restriction disappear there; do not conclude from that the tests were wrong.

## Revisit condition

Closed when GCC folds null comparisons against addresses of subobjects of
constexpr objects under `-fsanitize=null` (verify with the minimal
reproduction above), or when `cps_code.hpp`'s closure guard stops comparing a
program-internal pointer against null.

## 2026-08-01: re-verified during rebuild step R1

Still reproduces at GCC 16.0.1 20260322 (trunk r16-8246) with this doc's five-line reproduction: clean without sanitizers, rejected under `-fsanitize=null` with `'(((const Node*)(& h.Holder::n)) != 0)' is not a constant expression`.
The rebuild carries the constraint forward: `src/smd/cl/` code keeps null-pointer comparisons off the constant-evaluation path for namespace-scope `constexpr` program objects (R2's `symbol_table::find` reports absence with `std::optional`, not a sentinel pointer).
