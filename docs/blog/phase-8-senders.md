<div class="abstract" id="org9efc5e9">
<p>
The Mendler interpreter evaluates arguments sequentially, but the tree
structure says they are independent. I wrap each sub-expression in a sender
and combine independent arguments with <code>when_all</code>, making their
parallelism visible to the execution framework.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 7 - Mendler Interpretation ←](phase-7-mendler.md)

</nav>


# Sender-Based Evaluation

The Mendler interpreter from the previous phase threads an environment through the tree and evaluates every sub-expression to a value. The code is clear and correct, but it is also unconditionally sequential. Every argument to a function call is evaluated one after the other, even though the tree structure encodes the fact that they do not depend on each other.

This phase introduces a sender-based interpreter that makes that structural independence explicit. The same semantics, the same cases — but each sub-expression is expressed as a deferred sender, and independent arguments are combined with `when_all`.


## The P2300 Sender/Receiver Model

P2300 (`std::execution`) is the C++26 proposal for structured concurrency (Niebler, Eric and others, 2020). The central abstraction is the **sender**: a value that **describes** work without performing it. A sender is connected to a **receiver** that handles its result, and a **scheduler** decides where the work runs.

The vocabulary I use comes from `sender_v.hpp`:

```cpp
using beman::execution26::just;
using beman::execution26::let_value;
using beman::execution26::sync_wait;
using beman::execution26::then;
using beman::execution26::when_all;
```

-   `just(v)` — a sender that completes immediately with value `v`
-   `then(s, f)` — when sender `s` completes with value `v`, call `f(v)` and produce its result
-   `when_all(s0, s1, ...)` — run all senders; when all complete, produce all their results as a tuple
-   `sync_wait(s)` — connect `s` to an inline receiver and block until it completes

With `sync_wait`, everything runs on the calling thread in order. With a thread-pool scheduler, `when_all` arguments run concurrently — no changes to the sender descriptions needed.


## Beman Execution

The project uses the Beman Project implementation of P2300, vendored at `vendor/execution` as a git submodule and integrated via `add_subdirectory`. There is no FetchContent, no install-time discovery: the implementation is pinned and present.


## The Deferral Pattern

The first thing to understand about this interpreter is what "deferred" means in practice.

The naive approach would be to evaluate a sub-expression and then wrap the result in `just`:

```c++
// WRONG — evaluates eagerly, then wraps the already-computed result
auto s = just(eval(sub_expr, env));
```

This does not defer anything. The call to `eval` happens at the point where `just` is called, not when the sender is connected to a receiver.

The correct pattern defers evaluation inside a `then` lambda:

```c++
// RIGHT — evaluation is deferred until the sender is connected
auto s = then(just(0), [&](int) -> Res {
    return sender_mendler_run<MaxList, MaxBindings>(sub_expr, env);
});
```

The lambda captures the sub-expression and environment but does not evaluate them. The lambda runs only when `s` is connected — which happens inside `sync_wait` (or a scheduler). Until then, `s` is just a description of work to be done.

This pattern appears throughout `sender_mendler_eval.hpp`. The `comp_pure` case:

```c++
// src/smd/smdscheme/sender/sender_mendler_eval.hpp
[](fixpoint_eval::comp_pure const &p) -> Res {
    return detail::unwrap_sender<Res>(sender_v::then(
        sender_v::just(p),
        [](fixpoint_eval::comp_pure const &pure) -> Res {
            return std::visit(
                smd::fixpoint::overloaded{
                    [](int i) -> Res { return Val{i}; },
                    [](bool b) -> Res { return Val{b}; },
                    [](std::string_view sv) -> Res {
                        return Val{closure::symbol{sv}};
                    }},
                pure.atom);
        }));
},
```

Even for a literal, the conversion from atom to value is expressed as a sender. The uniformity matters: every path through the interpreter is a sender, so `when_all` can combine any two of them.


## `when_all` for Builtin Arguments

The payoff comes in the `comp_apply` builtin case. Builtins take exactly two arguments — an arity enforced by a runtime check — and the two argument sub-trees are sibling `Box` nodes with no data dependency between them.

```cpp
// Builtin: when_all for parallel arg evaluation
[&env, &app](closure::builtin const &bi) -> Res {
    if (app.args.size() != 2)
        return Res{foundation::parse_error{
            {}, "arity mismatch"}};

    // Deferred senders — evaluation happens inside
    // when_all, not before it
    auto s0 = sender_v::then(
        sender_v::just(0), [&](int) -> Res {
            return sender_mendler_run<MaxList,
                                      MaxBindings>(
                *app.args[0], env);
        });
    auto s1 = sender_v::then(
        sender_v::just(0), [&](int) -> Res {
            return sender_mendler_run<MaxList,
                                      MaxBindings>(
                *app.args[1], env);
        });

    return detail::unwrap_sender<Res>(sender_v::then(
        sender_v::when_all(std::move(s0),
                           std::move(s1)),
        [bi](Res a0r, Res a1r) -> Res {
            if (!a0r.has_value())
                return a0r;
            if (!a1r.has_value())
                return a1r;
            if (!std::holds_alternative<int>(
                    a0r.value()) ||
                !std::holds_alternative<int>(
                    a1r.value()))
                return Res{foundation::parse_error{
                    {}, "type error"}};
            int a = std::get<int>(a0r.value());
            int b = std::get<int>(a1r.value());
            if (bi.op == closure::builtin_op::add)
                return Res{Val{a + b}};
            return Res{Val{a * b}};
        }));
},
```

`s0` and `s1` are constructed but not started. `when_all(s0, s1)` produces a new sender that, when connected, starts both. With `sync_wait`, this is sequential: `s0` runs to completion, then `s1`. With a thread-pool scheduler, both start immediately and run concurrently. The arithmetic in the `then` lambda runs only after both have produced results.

The code does not change between the sequential and parallel cases. The sender structure already encodes the independence; the scheduler decides how to exploit it.


## Why Closures and Foreign Functions Stay Sequential

`when_all` is a variadic template: it requires the number of senders to be known at compile time. For builtins that is easy — there are always exactly two arguments.

For closures and foreign functions, arguments live in a `static_vector<Box<A>, MaxList>` — a runtime-sized range. There is no way to spell `when_all(args[0], args[1], ..., args[n-1])` when `n` is not a compile-time constant.

The closure case evaluates arguments in a sequential loop:

```c++
// src/smd/smdscheme/sender/sender_mendler_eval.hpp
for (int i = 0; i < app.args.size(); ++i) {
    auto arg_r = detail::unwrap_sender<Res>(sender_v::then(
        sender_v::just(0), [&](int) -> Res {
            return sender_mendler_run<MaxList, MaxBindings>(
                *app.args[i], env);
        }));
    if (!arg_r.has_value()) return arg_r;
    new_env.define(lam.params[i], arg_r.value());
}
```

Each argument is still expressed as a deferred sender — the structure is uniform — but `sync_wait` is called inside the loop, so execution is sequential. A `when_all` that accepts a range of senders (not in P2300) or type-erased senders via an `any_sender` adaptor would solve this. For now, the limitation is intentional: the sender machinery is being introduced gradually, and the closure case shows exactly where the boundary lies.


## Structural Preservation vs. CPS Trampolines

There is a deeper point here about what the `Fix<CompF>` representation buys.

A CPS trampoline evaluates `(+ (f 1) (g 2))` by linearizing it: evaluate `f(1)`, store the result; evaluate `g(2)`, store the result; add. The parallelism between the two argument evaluations is **represented** in the original expression but **destroyed** by the transformation to CPS.

The `Fix<CompF>` tree does not destroy it. In the tree, `(f 1)` and `(g 2)` are sibling `Box` nodes under the `comp_apply` for `+`. They have no edge between them. Any traversal that respects the tree structure can see that independence directly — and `when_all` does exactly that.

This is the sense in which the representation **preserves** the parallelism structure. CPS flattens the tree into a sequence; `Fix<CompF>` keeps the branching. The sender framework can exploit branching. It cannot exploit a sequence.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Phase 9 - Visualizing Execution →](phase-9-graphs.md)

</nav>


# References

Niebler, Eric et al. (2020). **P2300: std::execution**, ISO C++ Committee Papers.
