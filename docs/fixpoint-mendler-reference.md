# Fixpoint and Mendler-Style Interpretation: A Reference

## Motivation

The `sender_cps_program.hpp` backend builds sender graphs and immediately `sync_wait`s them at every node, defeating the compositional purpose of senders.
The goal is to separate the expression tree (computation structure) from the execution policy (how to run it).
By introducing `Fix<CompF>` — a concrete fixpoint type — and a Mendler-style interpreter that explicitly threads the environment, we preserve the applicative structure of independent arguments (enabling future parallelization) while handling the monadic structure of conditionals and closures.

## F-Algebras and Catamorphisms

An **F-algebra** for an endofunctor `F` is a function `alg : F<A> → A` for some carrier type `A`.
A **catamorphism** is a fold over a recursive type `Fix<F>`.
The algorithm is:
1. Unwrap one layer of `Fix<F>` to get `F<Fix<F>>`.
2. Apply `fmap` to map the fold recursively over all children: each `Fix<F>` becomes `A` via the catamorphism.
3. Apply the algebra to the fully-folded layer: `F<A> → A`.

Example: counting the depth of a natural number tree.

```cpp
template <typename A, typename F>
using NatF = std::variant<Zero, Succ<A>>;

auto count_algebra = [](const NatF<int>& n) -> int {
    if (auto* z = std::get_if<Zero>(&n)) return 0;
    if (auto* s = std::get_if<Succ<int>>(&n)) return *s.pred + 1;
    return 0;
};

auto fmap_nat = [](auto &&f, const auto &nat) { /* maps Succ child */ };

Nat my_tree = /* ... */;
int depth = fold_fix<int>(count_algebra, fmap_nat, my_tree);
```

Catamorphisms are *structural recursion*: every child is folded uniformly before the algebra sees it.

## Why Environment-Threading Breaks Catamorphisms

A catamorphism folds all children with the **same** transformation.
But Scheme evaluation is **not** uniform:

1. **Conditional (`if`)**: Only one branch should execute.
   A catamorphism evaluates both branches eagerly before the algebra can choose.
   We need lazy evaluation: decide based on the condition, then evaluate only the chosen branch.

2. **Lambda application**: The body should be evaluated in the **extended** environment (call-site bindings plus formal parameters).
   Arguments should be evaluated in the **call-site** environment.
   A catamorphism processes all children with one environment.

These two cases require the interpreter to **control the order of evaluation and the context** passed to each child.
This control is incompatible with the catamorphic pattern of "process all children uniformly, then apply algebra."

## Mendler-Style Folds

Mendler's approach (1991) is to give the algebra an explicit "recurse" function.
The project provides two generic combinators in `src/smd/fixpoint/recursion_schemes.hpp`:

**`mendler_fold`** — Mendler catamorphism with context threading:

```cpp
template <typename Result, template <typename> class F, typename Ctx,
          typename Algebra>
constexpr auto mendler_fold(Algebra const &alg, Ctx const &ctx,
                            Fix<F> const &tree) -> Result {
    auto recurse = [&alg](Fix<F> const &child, Ctx const &c) -> Result {
        return mendler_fold<Result, F>(alg, c, child);
    };
    return alg(recurse, ctx, unwrap_fix(tree));
}
```

Algebra signature: `(recurse_fn, Ctx const&, F<Fix<F>> const&) → Result`

**`mendler_para`** — Mendler paramorphism (primitive recursion) with context:

```cpp
template <typename Result, template <typename> class F, typename Ctx,
          typename Algebra>
constexpr auto mendler_para(Algebra const &alg, Ctx const &ctx,
                            Fix<F> const &tree) -> Result {
    auto recurse = [&alg](Fix<F> const &child, Ctx const &c) -> Result {
        return mendler_para<Result, F>(alg, c, child);
    };
    return alg(recurse, ctx, tree, unwrap_fix(tree));
}
```

Algebra signature: `(recurse_fn, Ctx const&, Fix<F> const&, F<Fix<F>> const&) → Result`

The algebra **receives a function** that recurses on demand.
It can choose whether to call it, how many times, and with what context.
This breaks the uniformity of the catamorphism and allows encoding of control flow.
The `Ctx` parameter threads an environment (or any state) through the recursion.
The `mendler_para` variant additionally passes the original `Fix<F>` node, enabling
cases that need the recursive structure itself (e.g., closures store a pointer to the node).

The interpreter uses `mendler_para` with `Ctx = env<Comp<MaxList>, MaxBindings>`:
- For `comp_if`: call `recurse` on condition only, choose branch, call `recurse` on that branch only.
- For `comp_lambda`: capture the current env, return a closure (no `recurse` call — uses `node` from para).
- For `comp_apply` + closure: call `recurse` on args in call-site env, build new env, call `recurse` on body with extended env.

## The `CompF` Functor

`CompF` mirrors the structure of the core AST but replaces `arena_box` handles with `Box<A>`, a custom constexpr-capable owning pointer.
`Box<A>` is not `std::indirect`: `std::indirect`'s explicit default constructor blocks aggregate initialization in `static_vector`,
and constexpr support for `std::indirect` is not yet available in GCC16.
`Box<A>` avoids both constraints.

```cpp
struct comp_pure { std::variant<int, bool, std::string_view> atom; };
struct comp_lookup { std::string_view name; };

template <typename A>
struct comp_if { Box<A> cond, cons, alt; };

template <typename A, int MaxList>
struct comp_lambda { static_vector<string_view, MaxList> params; Box<A> body; };

template <typename A, int MaxList>
struct comp_apply { Box<A> func; static_vector<Box<A>, MaxList> args; };

template <int MaxList>
struct comp_f_factory {
    template <typename A>
    using type = std::variant<comp_pure, comp_lookup, comp_if<A>,
                              comp_lambda<A, MaxList>, comp_apply<A, MaxList>>;
};

template <int MaxList>
using Comp = Fix<comp_f_factory<MaxList>::template type>;
```

**Why `comp_pure` stores atoms only** (not full `closure::value`):
`closure::value<Comp>` contains `closure::closure<Comp>`, which contains a pointer into the `Comp` tree.
If `comp_pure` stored `closure::value<Comp>`, the variant would have a circular type dependency at instantiation time.
By limiting `comp_pure` to literal atoms, we break the cycle.
The interpreter converts atoms to values at evaluation time via a simple `std::visit`.

**Why `comp_apply` uses `static_vector<Box<A>, MaxList>` for args** (not `std::vector`):
`static_vector` has no heap allocator and is constexpr-capable; `MaxList` is a compile-time bound matching the pipeline's list limit.
`std::vector` would prevent constexpr evaluation and require a heap allocator at constant-evaluation time.

## Two-Level Fixpoint: CoreF → CompF

The **source AST** is `foundation::fix<core_f_factory>::type` — a fixpoint using `arena_box` integer handles.
The arena stores nodes contiguously and is constexpr-friendly.

The **computation tree** is `Fix<comp_f_factory>`— a fixpoint using `Box<A>` (heap pointers).
Children are indirectly owned, making the tree copyable and moveable (unlike arena-based trees).

**The conversion** (`core_to_comp`) maps:
- Nodes in the arena to nodes in the heap-allocated tree.
- `arena_box` handles to `Box` pointers (via `arena.get(handle)` then `&result`).

This is conceptually an **anamorphism** (unfold_fix), but implemented as manual recursion to support error propagation when encountering invalid nodes like `core_define` in expression context.

## The Reader Monad Pattern

`mendler_run(comp, env) -> result<value>` is `Reader Env (Result Value)` in monadic notation.
The `Ctx` parameter of `mendler_para` is exactly this reader context.

The environment flows through the interpretation via `recurse`:
- Leaf nodes (`comp_pure`, `comp_lookup`) use `env` directly without calling `recurse`.
- Control flow (`comp_if`) passes `env` unchanged to `recurse` on the chosen branch.
- **Lambda application** is the only place that extends `env`:
  1. Call `recurse` on args with the **call-site** `env`.
  2. Create `new_env` from the closure's captured env (or current env if uncaptured).
  3. Bind each arg in `new_env`.
  4. Call `recurse` on body with the **extended** `new_env`.

This explicit threading of `env` through every call to `recurse` implements the reader monad without relying on monadic bind syntax.

## Applicative Parallelism Preserved

In `comp_apply`, the arguments are **structurally independent**.
Each `Box<Comp>` in the `args` vector has no data dependency on any other.

```cpp
comp_apply {
    func: Box<Comp>,
    args: [Box<Comp>, Box<Comp>, ...]  // each arg is independent
}
```

The current algebra evaluates args sequentially via `recurse`:
```cpp
for (auto const& arg : app.args) {
    auto arg_r = recurse(*arg, env);  // sequential
    // ...
}
```

But the **tree structure is visible**: a future sender-based interpreter can check for independence and use `when_all`:
```cpp
// pseudocode: sender-based evaluation
auto run_sender(Comp const& comp) -> sender<result<value>> {
    if (auto* app = std::get_if<comp_apply>(&unwrap_fix(comp))) {
        auto func_s = run_sender(*app->func);
        auto args_s = senders::when_all(
            run_sender(*app->args[0]),
            run_sender(*app->args[1]),
            // ...
        );
        return senders::then(
            senders::when_all(func_s, args_s),
            [](auto func, auto args) { /* apply */ }
        );
    }
    // ...
}
```

A **CPS trampoline** would linearize this structure:
```cpp
// trampoline: evaluates left-to-right in a loop
step 1: evaluate func
step 2: evaluate arg[0]
step 3: evaluate arg[1]
step 4: apply
```

The tree structure (and its parallelism) would be lost.
`Fix<CompF>` preserves it.

## Sender-Based Mendler Interpreter

`sender_mendler_run` has the same signature and semantics as `mendler_run` but wraps each sub-expression evaluation as a deferred sender:

```cpp
// src/smd/smdscheme/sender/sender_mendler_eval.hpp
// Each sub-expression is a deferred sender via then(just(0), lambda)
auto s0 = sender_v::then(sender_v::just(0), [&](int) -> Res {
    return sender_mendler_run<MaxList, MaxBindings>(*app.args[0], env);
});
auto s1 = sender_v::then(sender_v::just(0), [&](int) -> Res {
    return sender_mendler_run<MaxList, MaxBindings>(*app.args[1], env);
});

// For builtins (arity 2), the two argument senders are combined with when_all
return detail::unwrap_sender<Res>(sender_v::then(
    sender_v::when_all(std::move(s0), std::move(s1)),
    [bi](Res a0r, Res a1r) -> Res { /* apply builtin */ }));
```

The key dispatch:

- **Builtin application (arity 2)**: the two argument senders are combined with `when_all`, making their structural independence visible to the sender framework.
- **Closure application**: args are evaluated sequentially in a loop because the runtime-sized args list prevents variadic `when_all`.
- **Foreign function application**: also sequential for the same reason.

With `sync_wait`, execution is inline-sequential.
A thread-pool scheduler would parallelize builtin argument evaluation without any code changes to `sender_mendler_run`.

## Let and Let* Desugaring

`let` and `let*` are not new IR nodes — both desugar in the elaborator before the computation tree is built:

- `(let ((x e1) (y e2)) body)` → `((lambda (x y) body) e1 e2)`
- `(let* ((x e1) (y e2)) body)` → `((lambda (x) ((lambda (y) body) e2)) e1)`

Both are handled in `elaborate.hpp`, not the interpreter.
No new `Comp` IR nodes are needed: the interpreter sees only `comp_lambda` and `comp_apply`.

`let*` allows each binding to reference earlier bindings because each binding creates a separate lambda scope.
The sequential nesting of lambdas encodes the left-to-right dependency chain directly in the tree structure.

## Closure Ownership and Lifetime

A `closure::closure<CompT>` holds:
- `CompT const* node` — non-owning pointer to the lambda node in the Comp tree.
- `constexpr_box<env<CompT, 16>> captured` — owned deep copy of the captured environment.

The lifetime contract: the `fixpoint_program` owns the entire `Comp` tree by value.
When an interpreter evaluates a lambda, it stores a pointer to that tree.
**That pointer is only valid while the program object exists and is not copied.**

Example hazard:
```cpp
auto prog1 = compile_fixpoint("(lambda (x) x)");
auto clo1 = prog1(env);  // evaluates lambda, stores pointer into prog1's tree
auto prog2 = prog1;      // copies the tree and all its nodes to new addresses
// clo1.node now points into prog1's tree, but prog1 may be destroyed
// dereferencing clo1.node is undefined behavior
```

In practice, programs are created once and used for multiple evaluations with different environments (not copied).
The lifecycle is safe.

## References

- **F-algebras and catamorphisms**: Bird, R. S., & de Moor, O. (1997). *The Algebra of Programming*. Prentice Hall.
- **Anamorphisms and hylomorphisms**: Meijer, E., Fokkinga, M., & Paterson, R. (1991). "Functional Programming with Bananas, Lenses, Envelopes and Barbed Wire." *FPCA*.
- **Mendler-style recursion**: Mendler, N. P. (1991). "Recursive Types and Type-checking." *PhD thesis*, Cornell University.
- **Modern recursive patterns**: Abel, A., & Pientka, B. (2012). "Wellfounded Recursion with Copatterns." *ICFP*.
- **C++26 std::indirect**: P3019R13. The standard type for owned indirection;
  the project uses a custom constexpr `Box<A>` as a workaround until `std::indirect`
  gains full constexpr support.
- **C++26 Execution (senders)**: P2300R9. "std::execution". The sender-based
  Mendler interpreter uses Beman Execution's implementation.
