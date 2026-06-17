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

Mendler's approach (1991) is to give the algebra an explicit "recurse" function:

```cpp
template <typename R, template <typename> class F>
auto mendler_fold(
    const auto& alg,      // alg : (A → R) → F<A> → R
    const Fix<F>& tree)
    -> R
{
    return alg(
        [](const Fix<F>& child) { return mendler_fold(alg, child); },
        unwrap_fix(tree)
    );
}
```

The algebra **receives a function** that recurses on demand.
It can choose whether to call it, and how many times, and with what arguments.
This breaks the uniformity of the catamorphism and allows encoding of control flow.

In our implementation, `mendler_run` receives the full `Comp` node and the current environment.
It controls recursion explicitly:
- For `comp_if`: evaluate condition only, choose branch, recurse on that branch only.
- For `comp_lambda`: capture the current env, return a closure (no recursion yet).
- For `comp_apply` + closure: evaluate args in call-site env, build new env, recurse on body with extended env.

## The `CompF` Functor

`CompF` mirrors the structure of the core AST but replaces `arena_box` handles with `Box<A>` (heap pointers).

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

The environment flows through the interpretation:
- Leaf nodes (`comp_pure`, `comp_lookup`) use `env` directly.
- Control flow (`comp_if`) passes `env` unchanged to both branches.
- **Lambda application** is the only place that extends `env`:
  1. Evaluate args in the **call-site** `env`.
  2. Create `new_env` from the closure's captured env (or current env if uncaptured).
  3. Bind each arg in `new_env`.
  4. Evaluate body in the **extended** `new_env`.

This explicit threading of `env` through every recursive call implements the reader monad without relying on monadic bind syntax.

## Applicative Parallelism Preserved

In `comp_apply`, the arguments are **structurally independent**.
Each `Box<Comp>` in the `args` vector has no data dependency on any other.

```cpp
comp_apply {
    func: Box<Comp>,
    args: [Box<Comp>, Box<Comp>, ...]  // each arg is independent
}
```

The current `mendler_run` evaluates args sequentially:
```cpp
for (auto const& arg : app.args) {
    auto arg_r = mendler_run(*arg, env);  // sequential
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
- **C++26 Execution (senders)**: P2300R9. "std::execution".
- **C++26 std::indirect**: P3019R13. "Portable assumptions".
