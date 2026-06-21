<div class="abstract" id="org0a7938b">
<p>
A catamorphism folds every child before the algebra sees the result. But
Scheme's <code>if</code> must choose a branch before evaluating it, and lambda
application must extend the environment before evaluating the body. I use a
Mendler-style fold that gives the algebra explicit control over which children
to recurse into, and with what context.
</p>

</div>


# Mendler-Style Interpretation

The previous phase assembled all the runtime machinery: `Comp<MaxList>` trees as the interpreter input, `closure::value` as the output, and `env` as the threading context. Now I write the interpreter itself.

The natural tool for evaluating a `Fix<F>` tree is a catamorphism — a structural fold that peels off one layer, recursively folds all children, and passes the results to an algebra. But Scheme resists this approach. This phase explains why, and how Mendler's generalization escapes the trap.


## F-Algebras and Catamorphisms

An F-algebra for a functor `F` and a carrier type `A` is a function `alg : F<A> → A`. A catamorphism over `Fix<F>` is mechanically derived from any such algebra:

1.  Unwrap one layer: `Fix<F>` → `F<Fix<F>>`
2.  `fmap` the catamorphism over all recursive children: `F<Fix<F>>` → `F<A>`
3.  Apply the algebra: `F<A>` → `A`

The key word is **all**. `fmap` transforms every child before the algebra sees any of them. The algebra receives fully-evaluated children; it has no say in the order, and it cannot skip any (Bird, Richard S. and de Moor, Oege, 1997).

This is the right tool for evaluating arithmetic expressions. Given a tree representing `(+ (* 3 4) 2)`, a catamorphism evaluates `(* 3 4)` to `12`, evaluates `2` to `2`, and then passes both results to the `+` algebra. The algebra has nothing more to do; all the work happened in the fold.

For Scheme, this does not work.


## Why Catamorphisms Fail for Scheme


### Conditional Branching

Consider `(if #f (error!) 42)`. The correct result is `42`. A catamorphism evaluates both `(error!)` and `42` before the algebra for `if` gets to run. There is no way to short-circuit. The branch not taken is evaluated unconditionally.

Lazy evaluation — deciding based on the condition, then evaluating only the chosen branch — cannot be expressed as a catamorphism. The fold structure commits to evaluating every child.


### Lambda Application

Consider `(let ((x 10)) x)`, which desugars to `((lambda (x) x) 10)`. The body `x` must be evaluated in an environment where `x` is bound to `10`. But the catamorphism evaluates the body **before** the algebra runs, in whatever environment the fold was started with. The argument `10` is also evaluated before the algebra runs.

When the algebra for `comp_apply` finally executes, it has pre-evaluated children — a closure and an integer — but it cannot go back and re-evaluate the body in the extended environment. The evaluation order is fixed by the fold.

The same problem appears with the lambda itself: a catamorphism would try to recurse into the body immediately. But a lambda should not evaluate its body; it should capture the current environment and defer body evaluation until the lambda is applied.


## Mendler's Approach

Mendler's generalization replaces the algebra's carrier `A` with an algebra that receives an explicit recursion function (Mendler, N. Paul, 1991) (Abel, Andreas and Pientka, Brigitte, 2012). Instead of:

```
alg : F<A> → A
```

the algebra becomes:

```
alg : (∀ B. (B → A) → F<B> → A)
```

In practice this means the algebra receives a `recurse` function alongside each layer. It can call `recurse` on any child, in any order, with any arguments it chooses. It can decline to call `recurse` on a child at all.

This breaks the uniformity constraint that makes catamorphisms tractable — and breaks it in exactly the right direction for an interpreter that needs environment threading and lazy branching.


## `mendler_run`: A Case-by-Case Walk

The Mendler-style interpreter for `Comp` trees is `mendler_run` in `src/smd/smdscheme/sender/fixpoint_eval.hpp`. It is not a catamorphism. It is a recursive function where the environment `env` is a parameter, not ambient state, and each case decides independently whether and how to recurse.

The signature:

```c++
// src/smd/smdscheme/sender/fixpoint_eval.hpp
template <int MaxList, int MaxBindings>
[[nodiscard]] constexpr auto
mendler_run(Comp<MaxList> const &comp,
            closure::env<Comp<MaxList>, MaxBindings> const &env)
    -> foundation::result<closure::value<Comp<MaxList>>>;
```

`mendler_run(comp, env)` evaluates the tree rooted at `comp` in the environment `env` and returns either a value or an error. The dispatch is a `std::visit` over `unwrap_fix(comp)`, the outermost `CompF` layer.


### `comp_pure`: Atoms

```c++
// src/smd/smdscheme/sender/fixpoint_eval.hpp
[](comp_pure const &p) -> Res {
    return std::visit(smd::fixpoint::overloaded{
                          [](int i) -> Res { return Val{i}; },
                          [](bool b) -> Res { return Val{b}; },
                          [](std::string_view sv) -> Res {
                              return Val{closure::symbol{sv}};
                          }},
                      p.atom);
},
```

Literals need no recursion. An integer becomes `value{int}`, a boolean becomes `value{bool}`, and a string becomes `value{symbol}`. The environment is not consulted.


### `comp_lookup`: Variable Reference

```c++
// src/smd/smdscheme/sender/fixpoint_eval.hpp
[&env](comp_lookup const &l) -> Res { return env.lookup(l.name); },
```

Variable lookup delegates entirely to `env`. If the name is unbound, `env.lookup` returns a `parse_error` that propagates up.


### `comp_if`: Lazy Branching

```c++
// src/smd/smdscheme/sender/fixpoint_eval.hpp
[&env](comp_if<CompT> const &ci) -> Res {
    auto cond_r = mendler_run<MaxList, MaxBindings>(*ci.cond, env);
    if (!cond_r.has_value())
        return cond_r;

    bool taken = !std::holds_alternative<bool>(cond_r.value()) ||
                 std::get<bool>(cond_r.value());
    return mendler_run<MaxList, MaxBindings>(
        taken ? *ci.cons : *ci.alt, env);
},
```

This is where the Mendler structure pays off. The condition is evaluated first. Then exactly one of `cons` or `alt` is recursed into — the other is never touched. Any catamorphism over this node would force both branches first.

The truthiness rule follows Scheme: anything other than `#f` is truthy.


### `comp_lambda`: Closure Creation

```c++
// src/smd/smdscheme/sender/fixpoint_eval.hpp
[&env, &comp](comp_lambda<CompT, MaxList> const &) -> Res {
    return Val{closure::closure<CompT>{
        &comp, closure::constexpr_box<closure::env<CompT, 16>>{
                   new closure::env<CompT, 16>{env}}}};
},
```

A `lambda` expression does not evaluate its body. It captures the current environment in a `constexpr_box` and bundles it with a non-owning pointer to the `Comp` node itself. No recursion happens here.


### `comp_apply` with a Builtin

```c++
// src/smd/smdscheme/sender/fixpoint_eval.hpp
[&env, &app](closure::builtin const &bi) -> Res {
    // evaluate arg0 in current env
    auto arg0_r = mendler_run<MaxList, MaxBindings>(*app.args[0], env);
    if (!arg0_r.has_value()) return arg0_r;
    // evaluate arg1 in current env
    auto arg1_r = mendler_run<MaxList, MaxBindings>(*app.args[1], env);
    if (!arg1_r.has_value()) return arg1_r;
    // dispatch on op
    int a = std::get<int>(arg0_r.value());
    int b = std::get<int>(arg1_r.value());
    if (bi.op == closure::builtin_op::add) return Val{a + b};
    return Val{a * b};
},
```

The function is evaluated first, revealing it is a builtin. Arguments are then evaluated left-to-right in the current environment. The builtin operation is applied to the integer results.


### `comp_apply` with a Closure

```c++
// src/smd/smdscheme/sender/fixpoint_eval.hpp
[&env, &app](closure::closure<CompT> const &clo) -> Res {
    // extract lambda node and params
    auto const &lam = std::get<comp_lambda<CompT, MaxList>>(
        smd::fixpoint::unwrap_fix(*clo.node));

    // start from captured env (or current if none)
    auto new_env = clo.captured ? *clo.captured
                                : closure::env<CompT, 16>{env};

    // evaluate args in CURRENT env, bind in new_env
    for (int i = 0; i < app.args.size(); ++i) {
        auto arg_r = mendler_run<MaxList, MaxBindings>(*app.args[i], env);
        if (!arg_r.has_value()) return arg_r;
        new_env.define(lam.params[i], arg_r.value());
    }

    // evaluate body in EXTENDED env
    return mendler_run<MaxList, 16>(*lam.body, new_env);
},
```

This is the core of lexical scoping. Arguments evaluate in `env` — the call-site environment. Then `new_env` starts from the closure's captured environment and is extended with the argument bindings. The body evaluates in `new_env`.

The two environments serve different roles: `env` is where the caller lives; `new_env` is where the callee lives. Confusing them would be dynamic scoping.


## The Reader Monad Pattern

The signature `mendler_run(comp, env) → result<value>` is a Reader monad computation: `Reader Env (Result Value)`. The environment threads explicitly through every recursive call. There is no mutable global environment, no stack of frames pushed and popped implicitly. Each call site passes the environment it wants the callee to use.

Lambda application is the only site that extends the environment. Every other recursive call passes `env` unchanged. This makes the scoping rules visible in the call structure of the interpreter: find the `new_env` construction and you have found the only place where new names enter scope.


## Applicative Parallelism in Argument Evaluation

In the `comp_apply` case, the arguments are sibling `Box` nodes with no data dependencies on each other. The current interpreter evaluates them sequentially in a `for` loop. But the tree structure makes their independence structurally visible.

This is not an accident. The `Comp` tree separates the representation of a computation from any particular evaluation strategy. The current interpreter chooses sequential evaluation for simplicity. The next phase introduces a sender-based interpreter that exploits this independence directly, launching argument evaluations as concurrent sender chains and joining them with `when_all`.


# References

Bird, Richard S. and de Moor, Oege (1997). **The Algebra of Programming**, Prentice Hall.

Mendler, N. Paul (1991). **Recursive Types and Type Constraints**, PhD thesis, Cornell University.

Abel, Andreas and Pientka, Brigitte (2012). **Wellfounded Recursion with Copatterns**, ICFP.
