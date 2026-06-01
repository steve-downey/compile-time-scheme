<div class="abstract" id="orgf83b3b0">
<p>
To truly compile my Scheme dialect into native code, I need an execution model.
Phase 7 bridges the gap between functional Scheme evaluation and modern C++ asynchronous workloads.
I achieve this by exploring two parallel approaches for mapping my CPS Abstract Syntax Tree into modern async executors using P2300 Senders and C++ Coroutines.
</p>

</div>


# The P2300 Sender/Receiver Model

C++26 brings standard asynchronous execution models (`std::execution [cite:@niebler2020p2300]`), commonly implemented in this codebase via the \`beman::execution\` and \`beman::task\` frameworks. The fundamental elements are:

-   **Senders**: Types describing units of work that will produce values (or errors).
-   **Receivers**: Callbacks that consume those values.
-   **Schedulers**: Contexts dictating *where* the work happens (e.g., thread pools).

Because my Scheme compiler relies on Continuation-Passing Style—where every operation simply says "do work, then pass the result to this callback"—it matches with the Sender/Receiver architecture. A Scheme continuation is functionally isomorphic to a C++ Receiver.


# Two Parallel Asynchronous Backends

While the core functionality of evaluating Scheme programs stays formally consistent, the mechanism mapping it to modern C++ execution semantics can be approached from two distinct architectural paths. Both can successfully represent the same set of correct Scheme programs.

I have implemented both to evaluate their tradeoffs against C++ structural typing bounds.


## Approach 1: Eager Graph Construction via Senders (\`sender\_cps\_program\`)

My first approach revolves around natively translating the AST into pure, nested \`beman::execution\` Senders. Here, the interpreter works synchronously wrapping its return types dynamically, building the data-flow chain.

By mapping over standard forms, the Scheme expression can execute out-of-order asynchronously. Complex parallel argument evaluations, like evaluating the arguments to a builtin function \`(+ (f 1) (g 2))\`, translates natively into a \`when\_all\`, spawning an async pipeline that awaits both results simultaneously before dropping to a \`then\` application block.

Here is the exact code snippet from \`sender\_cps\_program.hpp\` showing the \`when\_all\` implementation over function arguments natively resolving asynchronous execution structures:

```cpp
// Builtin application: when_all args, then apply operator
if (std::holds_alternative<closure::builtin>(func_r.value())) {
    auto const &bi = std::get<closure::builtin>(func_r.value());
    if (app.args.size() != 2)
        return Res{foundation::parse_error{{}, "arity mismatch"}};

    auto arg0_r = eval_node<MaxNodes, MaxList, MaxBindings>(
        arena.get(app.args[0]), arena, environment);
    auto arg1_r = eval_node<MaxNodes, MaxList, MaxBindings>(
        arena.get(app.args[1]), arena, environment);

    return detail::unwrap_sender<Res>(sender_v::then(
        sender_v::when_all(sender_v::just(std::move(arg0_r)),
                           sender_v::just(std::move(arg1_r))),
        [bi](Res a0r, Res a1r) -> Res {
            if (!a0r.has_value())
                return a0r;
            if (!a1r.has_value())
                return a1r;
            if (!std::holds_alternative<int>(a0r.value()) ||
                !std::holds_alternative<int>(a1r.value()))
                return Res{foundation::parse_error{{}, "type error"}};
            int a = std::get<int>(a0r.value());
            int b = std::get<int>(a1r.value());
            if (bi.op == closure::builtin_op::add)
                return Res{Val{a + b}};
            return Res{Val{a * b}};
        }));
}
```

The execution builds an explicit sender dependency graph over arguments \`arg0\_r\` and \`arg1\_r\`. It wraps them dynamically via \`sender\_v::just\` before joining them inside \`sender\_v::when\_all\`. Once both have correctly emitted their evaluated results via the dependency chain, the \`sender\_v::then\` block captures their success states and explicitly runs the math operators, which are run through synchronous wait evaluation.

The major benefit to this approach is that the data graph structure is explicitly typed out locally. As long as every recursive return path is synchronously waited upon to type-erase its distinct pipeline return into homogeneous results, it builds cleanly.


## Approach 2: Asynchronous State Machines via Coroutines (\`sender\_program\`)

While \`sender\_cps\_program\` functions admirably using synchronous \`unwrap\_sender\` evaluations locally against explicit type boundaries, there exists a second model that treats the entire program traversal as a fully asynchronous, unbroken pipeline.

However, executing sender pipelines recursively out natively without synchronous unwrapping hits severe structural limits in pure C++ template typing. If \`if\` branches evaluate differently into different generic typed sender structures, they cannot naturally resolve to the same recursive template return type syntactically.

To circumvent this, the alternative \`sender\_program.hpp\` backend falls back to **Coroutines**! We evaluate the root AST nodes asynchronously utilizing \`beman::task::task\` as an asynchronous factory. By utilizing standard \`co\_await\`, the execution dynamically type-erases the disparate internal graph structures down into a common coroutine state machine type natively provided by the compiler. It elegantly merges any mismatched paths over \`when\_all\` smoothly.


# Conclusion

By implementing dual representations of our backend logic utilizing strictly local synchronous Senders and dynamically type-erased async Coroutines, my compiler safely bridges the gap. It demonstrates that regardless of which mechanical representation of execution concurrency you prefer running across C++, our evaluated Scheme topology gracefully matches execution invariants safely.


# References
